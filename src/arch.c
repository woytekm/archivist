/*
 *    This file is part of Archivist - network device config archiver.
 *
 *    Archivist is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Archivist is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Archivist. If not, see <http://www.gnu.org/licenses/>.
 *
 *    Author: Wojtek Mitus (woytekm@gmail.com)
 *
 *    arch.c - multithreaded procedures for device config archivization 
 */

#include <pthread.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/stat.h>

#include <svn_client.h>
#include <svn_cmdline.h>
#include <svn_pools.h>
#include <svn_config.h>
#include <svn_fs.h>
#include <svn_error.h>

#include "defs.h"
#include "archivist_config.h"


int a_sync_device
(char *device_group, char *hostname, char *config_by, char *platform, char *authset, char *arch_method)
/*
 * here, we are trying to sync device config to svn repository. 
 * if device is in a device_group other than "none" - check if group exists. if not - create the group
 * (SVN subdirectory), and check device config in to this group (SVN subdirectory).
 * if initial checkout of head fails without svn error, we assume that the device config is not 
 * under version control yet, and we are trying to add config of this device to svn.
 */
{

   int resolver_result;
   int check,fail = 0;
   int checkout_status;
   char downloaded_config[MAXPATH];
   char working_copy_config[MAXPATH];
   char svn_tmp_dirname[MAXPATH];
   char *full_svn_path = NULL;
   char *group_path = NULL;
   struct addrinfo hints;
   struct addrinfo *res = NULL;
   int apr_pool_initialized = 0;
   int svn_pool_initialized = 0;
   apr_pool_t *thread_global_svn_pool;
   apr_pool_t *thread_global_apr_pool;


   /* check if given hostname is resolving (thread-safe version): */

   memset((char *) &hints, 0, sizeof(hints));

   hints.ai_family = PF_UNSPEC;
   hints.ai_flags = AI_CANONNAME;
   hints.ai_socktype = SOCK_STREAM;

   #define NO_SERVICE ((char *) 0)

   resolver_result = getaddrinfo(hostname, NO_SERVICE, &hints, &res);

   if(resolver_result != 0)
    { 
     a_logmsg("%s: FATAL: name is not resolving! Not archived.",hostname);
     fail = 1;
     goto skip;
    }

   /* construct filenames and paths needed for SVN checkout/commit: */

   snprintf(svn_tmp_dirname,MAXPATH,"%s.%d.%s",G_svn_tmp_prefix,G_config_info.instance_id,hostname);
   snprintf(downloaded_config,MAXPATH,"%s.new",hostname);
   snprintf(working_copy_config,MAXPATH,"%s/%s",svn_tmp_dirname,hostname);

   /* try to get current config from the device. 
    * if this will succeed, <hostname>.new file should appear in the working directory
    */

   if(a_get_from_device(hostname,platform,authset,arch_method)==-1) 
    {
     a_debug_info2(DEBUGLVL3,"a_sync_device: %s: configuration download failed! exiting!",hostname);
     a_logmsg("%s: FATAL: cannot get configuration from a device! not archived!",hostname); 
     fail = 1;
     goto skip;
    }

   /* try to make a checkout of previous config version into svn_tmp_dirname: */
   /* first, allocate sub - global (per thread) memory pools for SVN operation */
   /* svn_pool_create will cause program exit on alloc fail, so there is no error checking here */

   thread_global_svn_pool = svn_pool_create(G_svn_root_pool);
   svn_pool_initialized = 1;

   if(apr_pool_create(&thread_global_apr_pool, G_apr_root_pool) == APR_SUCCESS) 
    apr_pool_initialized = 1;
   else
    {
     a_debug_info2(DEBUGLVL3,"a_sync_device: %s: cannot allocate memory for SVN pool!",hostname);
     fail = 1;
     goto skip;
    }


   a_debug_info2(DEBUGLVL3,"a_sync_device: %s: device in group: %s",hostname,device_group);

   if(strstr(device_group,"none"))  
    {
     checkout_status = a_svn_checkout(hostname,G_config_info.repository_path,svn_tmp_dirname,
                                      thread_global_apr_pool, thread_global_svn_pool);
    }
   else
    {
     if( (full_svn_path = malloc(strlen(G_config_info.repository_path) + strlen(device_group) + 4) ) == NULL )
      { 
       a_debug_info2(DEBUGLVL3,"a_sync_device: %s: malloc failed!"); 
       goto skip; 
      }

     if( (group_path = malloc(strlen(svn_tmp_dirname) + strlen(device_group) + 4)) == NULL )
      {
       a_debug_info2(DEBUGLVL3,"a_sync_device: %s: malloc failed!");
       goto skip;
      }

     snprintf(full_svn_path,MAXPATH,"%s/%s",G_config_info.repository_path,device_group);
     snprintf(group_path,MAXPATH,"%s/%s",svn_tmp_dirname,device_group);

     checkout_status = a_svn_checkout(hostname,full_svn_path,svn_tmp_dirname,
                                      thread_global_apr_pool, thread_global_svn_pool);

     if(checkout_status == -3)  /* specified group does not exist - let's create it */
      {

       a_debug_info2(DEBUGLVL3,"a_sync_device: %s: group %s does not exist - we'll create it.",
                     hostname,device_group);
       a_logmsg("adding new device group: %s",device_group);

       checkout_status = a_svn_sparse_checkout(G_config_info.repository_path,svn_tmp_dirname,
                                               thread_global_apr_pool, thread_global_svn_pool);

       if(checkout_status < 0) 
        {
         a_debug_info2(DEBUGLVL3,"a_sync_device: %s: sparse checkout failed!",hostname);
         fail = 1;
         goto skip;
        }
       else
        a_debug_info2(DEBUGLVL3,"a_sync_device: %s: sparse checkout OK.",hostname);
    
       if(mkdir(group_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) 
        { 
         a_debug_info2(DEBUGLVL3,"a_sync_device: %s: failed to create group directory %s",device_group);
         a_logmsg("%s: FATAL: failed to create group directory %s ! not archived!",hostname,device_group);
         fail = 1;
         goto skip;
        }
       else
        {
         if(a_svn_add(device_group, svn_tmp_dirname, thread_global_apr_pool, thread_global_svn_pool) != -1)
         a_debug_info2(DEBUGLVL5,"a_sync_device: %s: device group add OK",device_group);
         else
          {
           a_debug_info2(DEBUGLVL3,"a_sync_device: %s: device group add failed",device_group);
           a_logmsg("%s: a_sync_device: SVN add failed when adding new device group.",device_group);
           fail = 1;
           goto skip;
          }

         if(a_svn_commit(device_group,svn_tmp_dirname,"new_device_group",
                         thread_global_apr_pool, thread_global_svn_pool) != -1)
          {
           a_debug_info2(DEBUGLVL5,"a_sync_device: %s: new device group commit OK",device_group);
           a_logmsg("%s: first time seen. adding device group to svn repository.",device_group);
          }
         else
          {
           a_debug_info2(DEBUGLVL3,"a_sync_device: %s: new device group commit failed.",device_group);
           a_logmsg("%s: a_sync_device: SVN commit failed when adding new device group.",device_group);
           fail = 1;
           goto skip;
          }
        } /* if mkdir of new device group succeeded */

        /* new group created - go on as usually */

        checkout_status = a_svn_checkout(hostname,full_svn_path,svn_tmp_dirname,
                                         thread_global_apr_pool, 
                                         thread_global_svn_pool); /* do a checkout again */

       } /* if group doesn't exist */
     } /* if group != "none" */

   if(checkout_status == -1) /* -1 means that checkout failed, but no svn error was generated. 
                              * device config may not be under version control yet - we'll try to 
                              * add it to svn repository 
                              */
    {
     a_debug_info2(DEBUGLVL3,"a_sync_device: checkout failed but no svn err. trying to add to repository.");

     rename(downloaded_config,working_copy_config);

     if(a_svn_add(hostname, svn_tmp_dirname, thread_global_apr_pool, thread_global_svn_pool) != -1) 
       a_debug_info2(DEBUGLVL5,"a_sync_device: %s: add OK",hostname); 
     else 
      {
       a_debug_info2(DEBUGLVL3,"a_sync_device: %s: add failed",hostname);
       a_logmsg("a_sync_device: %s: SVN add failed when adding new device.",hostname); 
       fail = 1;
       goto skip;
      }

     if(a_svn_commit(hostname,svn_tmp_dirname,"new_device",
                     thread_global_apr_pool, thread_global_svn_pool) != -1) 
      {
       a_debug_info2(DEBUGLVL5,"a_sync_device: %s: commit OK",hostname); 
       a_logmsg("%s: first time seen. adding device to svn repository.",hostname);
#ifdef USE_MYSQL
       a_mysql_update_timestamp(hostname);
#endif
      }
     else 
      { 
       a_debug_info2(DEBUGLVL3,"a_sync_device: %s: commit failed.",hostname);
       a_logmsg("%s: a_sync_device: SVN commit failed when adding new device.",hostname); 
       fail = 1;
       goto skip;
      }

    }
   else if(checkout_status > -1) /* checkout ok - device config is already under version control */
    {

     rename(downloaded_config,working_copy_config); /* prepare SVN diff  */

     if(a_svn_diff(device_group,hostname,config_by,G_config_info.repository_path,svn_tmp_dirname,
                   thread_global_apr_pool, thread_global_svn_pool))
      {
       if(a_svn_commit(hostname,svn_tmp_dirname,config_by,
                       thread_global_apr_pool, thread_global_svn_pool)) 
        {
         a_debug_info2(DEBUGLVL5,"a_sync_device: %s: commit OK",hostname);
         a_logmsg("%s: archiving changes.",hostname);
#ifdef USE_MYSQL
         a_mysql_update_timestamp(hostname);
#endif
        }
       else 
        {
         a_debug_info2(DEBUGLVL3,"a_sync_device: %s: commit failed",hostname);
         a_logmsg("%s: SVN commit failed when commiting config changes.",hostname); 
         fail = 1;
        }
      }
     else 
       a_logmsg("%s: no changes to config.",hostname);
    }


   skip:

   /* move temp files to one directory, and then remove it */
   
   rename(downloaded_config,working_copy_config); 

   a_remove_directory(svn_tmp_dirname);

   if(res != NULL) 
    freeaddrinfo(res);

   if(full_svn_path != NULL)
    free(full_svn_path);

   if(group_path != NULL)
    free(group_path); 

   if(apr_pool_initialized)
    apr_pool_destroy(thread_global_apr_pool);

   if(svn_pool_initialized)
    svn_pool_destroy(thread_global_svn_pool);

   if(fail) 
   {
#ifdef USE_MYSQL
    a_mysql_update_failed_archivizations(hostname);
#endif
    return -1; 
   }
   else return 1;

}


int a_archive_bulk
(void *arg)
/*
*
* bulk archivization routine
*
*/
{

  router_db_entry_t *device_entry_pointer; 
  config_event_info_t *confinfo;
  int thread_counter = 0;
  unsigned int timer = 0;

  #define DEADLOCK_TIMEOUT 2000000 /* five minutes in 300 usecond units  */

  while( G_active_bulk_archiver_threads >= MAX_CONCURRENT_BULK_THREADS )
   {
    a_debug_info2(DEBUGLVL5,"a_archive_bulk: waiting for another bulk archiving to end.");

    sleep(60);

    timer += 1; /* one minute here */ 
    if(timer > 10) 
     {
      a_logmsg("a_archive_bulk: timeout while waiting for another archiving thread to end. exit.");
      return -1;
     }

    if(G_stop_all_processing)
     return 1;

   }

  pthread_mutex_lock (&G_M_thread_count_mutex);
  G_active_bulk_archiver_threads++;
  pthread_mutex_unlock (&G_M_thread_count_mutex);

  a_debug_info2(DEBUGLVL5,"a_archive_bulk: starting bulk thread. G_active_bulk_archiver_threads now %d\n",
                  G_active_bulk_archiver_threads);

#ifdef USE_MYSQL

  MYSQL_RES *raw_router_db;
  MYSQL_ROW router_db;

  if(raw_router_db = a_mysql_select("select * from router_db"))
   while( (router_db = mysql_fetch_row(raw_router_db)) && !G_stop_all_processing )
    {

#else
 
    device_entry_pointer = G_router_db;  /* begin of the device database linked list  */

    while(device_entry_pointer!=NULL)
    {

#endif
     timer = 0;
     while( (G_active_archiver_threads >= G_config_info.archiver_threads) && !G_stop_all_processing )
      {
       /* throttle number of active archiver threads to G_config_info.archiver_threads */
       usleep(300);

       timer += 1; /* 300 usecond units here */
       if(timer > DEADLOCK_TIMEOUT)
        {
         a_logmsg("a_archive_bulk: timeout waiting for single threads to end - deadlock? exit.");
         pthread_exit(NULL);
        }

       if(G_stop_all_processing)
        pthread_exit(NULL);
      }

    /* start archiver thread for a single device  */

     if((confinfo = malloc(sizeof(config_event_info_t))) == NULL)
      {
       a_debug_info2(DEBUGLVL3,"a_archive_bulk: %s: malloc failed!");
       pthread_exit(NULL);
      }

     a_debug_info2(DEBUGLVL5,"a_archive_bulk: allocated new data structure at 0x%p",confinfo);

#ifdef USE_MYSQL

     a_debug_info2(DEBUGLVL5,"a_archive_bulk: checking %s",router_db[1]);
     a_logmsg("bulk archiver thread: checking %s ",router_db[1]);

     strcpy(confinfo->configured_by,"scheduled_archiving");
     strncpy(confinfo->device_id,router_db[1],sizeof(confinfo->device_id));

#else

     a_debug_info2(DEBUGLVL5,"a_archive_bulk: checking %s",device_entry_pointer->hostname);
     a_logmsg("bulk archiver thread: checking %s ",device_entry_pointer->hostname);

     strcpy(confinfo->configured_by,"scheduled_archiving");
     strncpy(confinfo->device_id,device_entry_pointer->hostname,sizeof(confinfo->device_id));

#endif

     pthread_t next_thread;
     size_t stacksize = ARCHIVIST_THREAD_STACK_SIZE;

     pthread_attr_t thread_attr;
     pthread_attr_init(&thread_attr);
     pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
     pthread_attr_setstacksize(&thread_attr, stacksize);
     
     if(pthread_create(&next_thread, &thread_attr, a_archive_single, (void *)confinfo))
      {
       a_logmsg("bulk archiver thread:fatal! cannot create archiver thread!");
       a_debug_info2(DEBUGLVL5,"a_archive_bulk: cannot create archiver thread for %s (%d)!",
                     confinfo->device_id,errno);
       free(confinfo);
      }

     device_entry_pointer = device_entry_pointer->prev;
     usleep(5000); 
    }

   sleep(60); /* give device archiving threads time to finish before exiting from bulk thread  */

   pthread_mutex_lock (&G_M_thread_count_mutex);
   G_active_bulk_archiver_threads--;
   pthread_mutex_unlock (&G_M_thread_count_mutex);

   a_debug_info2(DEBUGLVL5,"a_archive_bulk: thread exiting. G_active_bulk_archiver_threads now %d\n",
                 G_active_bulk_archiver_threads);
   a_logmsg("bulk archiver thread finished. there can be some logging from unfinished threads yet.");

   pthread_exit(NULL);
 
}


void a_archive_single
(void *arg)
/*
*
* always called via pthread_create. archive config from a single device:
*
*/
{

  config_event_info_t config_event_info;
  router_db_entry_t *router_entry;
  pthread_t my_id;
  char *devtype;
  int unlocked = 0, unlock_wait = 0;

  #define ARCH_WAIT_TIMEOUT 30

  a_debug_info2(DEBUGLVL5,"a_archive_single: input data at 0x%p",arg);

  config_event_info = *((config_event_info_t*)(arg));

  my_id = pthread_self();

  pthread_mutex_lock(&G_thread_count_mutex);
  G_active_archiver_threads++;
  pthread_mutex_unlock(&G_thread_count_mutex);

  a_debug_info2(DEBUGLVL5,"a_archive_single(%u): thread starting. G_active_archiver_threads now %d",
                my_id,G_active_archiver_threads);

  if( (router_entry = a_router_db_search(G_router_db, config_event_info.device_id)) != NULL )
   {
    a_debug_info2(DEBUGLVL5,"a_archive_single(%u): found device %s in database!",
                  my_id,config_event_info.device_id); 
    a_debug_info2(DEBUGLVL5,"a_archive_single(%u): trying to sync to SVN...",my_id);

    while( (!unlocked) && (unlock_wait < ARCH_WAIT_TIMEOUT) )
    {
     /* check if another archivization is running */
     if(!a_is_archived_now(G_router_db, config_event_info.device_id)) 
      {
       unlocked = 1;

       pthread_mutex_lock(&G_router_db_mutex);
       a_set_archived(G_router_db, config_event_info.device_id, 1);  /* lock the device for us */
       pthread_mutex_unlock(&G_router_db_mutex);

       a_debug_info2(DEBUGLVL5,"a_archive_single(%u): args to a_sync_device: %s, %s, %s, %s, %s, %s",
                     my_id,
                     config_event_info.device_id,config_event_info.configured_by,
                     router_entry->group,router_entry->hosttype,router_entry->authset,
                     router_entry->arch_method);

       a_sync_device(router_entry->group,config_event_info.device_id,config_event_info.configured_by,
                     router_entry->hosttype,router_entry->authset,router_entry->arch_method);
       
       pthread_mutex_lock(&G_router_db_mutex);
       a_set_archived(G_router_db, config_event_info.device_id, 0);  /* unlock the device after archiving */
       pthread_mutex_unlock(&G_router_db_mutex);
      }
     else
      {
       a_debug_info2(DEBUGLVL5,"a_archive_single(%u): another archivization of %s is running. waiting...",
                     my_id,config_event_info.device_id);
       sleep(1);  /* wait for device to be unlocked by another thread */
       unlock_wait++;
      }
    }
  

   if(unlock_wait > ARCH_WAIT_TIMEOUT)
    {
     a_debug_info2(DEBUGLVL3,
                   "a_archive_single(%u): timeout waiting for another archivization of %s to end! archivization failed!",
                   my_id,config_event_info.device_id);
    }

    a_debug_info2(DEBUGLVL5,"a_archive_single(%u): after SVN sync...",my_id);

   }
  else 
   {
    a_debug_info2(DEBUGLVL3,"a_archive_single(%u): device %s not found in database!",
                  my_id,config_event_info.device_id);
    a_logmsg("%s not found in the router.db. not archiving.",config_event_info.device_id);
   }

  pthread_mutex_lock (&G_thread_count_mutex);
  G_active_archiver_threads--;
  pthread_mutex_unlock (&G_thread_count_mutex);

  
  free(arg);
 
  /* ^^^ i'm still not sure if it's safe to do this in-thread... */

  a_debug_info2(DEBUGLVL5,"a_archive_single(%u): thread exiting. G_active_archiver_threads now %d",
                my_id,G_active_archiver_threads);

#ifdef USE_MYSQL
  if(router_entry != NULL)
   free(router_entry);
#endif

  pthread_exit(NULL);

}


/* end of arch.c  */
