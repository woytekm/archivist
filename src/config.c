/*
*
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
*    along with Archivist.  If not, see <http://www.gnu.org/licenses/>.
*
*    Author: Wojtek Mitus (woytekm@gmail.com)
*
*    config.c - implement config reading and parsing functions 
*
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "archivist_config.h"
#include "defs.h"
#include "scheduler.h"


int a_read_config_file
(char *filename, char *conf_tab[])
/*
* read a config file into a table of strings
*/
{

  FILE *fdes;
  char buf[CONFIG_MAX_LINELEN];
  char *workptr;
  int conflines=0,i;

  for(i=1;i<=MAX_CONF_LINES;i++) conf_tab[i] = NULL; /* nullify config table */

  if( (fdes = fopen(filename, "r")) == NULL )
   {
    printf("FATAL: cannot open config file %s!\n",filename); 
    return 0;
   }

  while (fgets(buf, CONFIG_MAX_LINELEN, fdes)) 
   {
    if (buf[0] == '#' || buf[0] == '\0' || buf[0] == '\n' || conflines >= MAX_CONF_LINES)	
	continue;
    conflines++;
    buf[strlen(buf)-1] = '\0'; /* remove trailing \n */

    if( (workptr = malloc(strlen(buf)+1)) == NULL )
     {
      printf("a_read_config_file: malloc failed!");
      return 0;
     }

    strcpy(workptr, buf);
    conf_tab[conflines] = workptr;
   }
 
   fclose(fdes);
  
   for(i=1;i<=conflines;i++)
    {
     a_debug_info2(DEBUGLVL5,"a_read_config_file: readed line: %s ",conf_tab[i]);
    }

  return 1;

}


void a_load_config_defaults
(struct config_info_t *conf_struct)
/*
* load config defaults from config.h
*/
{
  conf_struct->instance_id = DEFAULT_CONF_INSTANCE_ID;

  strcpy(conf_struct->working_dir,DEFAULT_CONF_WORKING_DIR);

  conf_struct->logging = DEFAULT_CONF_LOGGING;

  conf_struct->archiving_method = ARCHIVE_USING_INTERNAL;

  strcpy(conf_struct->log_filename,DEFAULT_CONF_LOGFILENAME);

  conf_struct->tail_syslog = DEFAULT_CONF_TAIL_SYSLOG;

  conf_struct->listen_syslog = DEFAULT_CONF_LISTEN_SYSLOG;

  conf_struct->open_command_socket = DEFAULT_CONF_LISTEN_CMDSOCK;

  conf_struct->syslog_port = DEFAULT_CONF_SYSLOG_PORT;

  conf_struct->keep_changelog = DEFAULT_CONF_CHANGELOG;

  conf_struct->archiver_threads = NUM_ARCH_THREADS;

  strcpy(conf_struct->changelog_filename,DEFAULT_CONF_CHANGELOG_FILENAME);

  strcpy(conf_struct->syslog_filename,DEFAULT_CONF_SYSLOG_FILENAME);

  strcpy(conf_struct->rancid_exec_path,DEFAULT_CONF_RANCID_PATH);

  strcpy(conf_struct->script_dir,DEFAULT_CONF_SCRIPT_DIR);

  strcpy(conf_struct->expect_exec_path,DEFAULT_CONF_EXPECT_PATH);

  strcpy(conf_struct->locks_dir,DEFAULT_CONF_LOCKS_DIR);

  strcpy(conf_struct->command_socket_path,DEFAULT_CONF_CMDSOCK);

#ifdef USE_MYSQL
  strcpy(conf_struct->mysql_dbname,DEFAULT_CONF_SQL_DBNAME);
#endif

  conf_struct->router_db_path = NULL; /* required to pick up from config file */
  conf_struct->repository_path = NULL; /* required to pick up from config file */
}




struct auth_set_t *a_auth_set_add
(struct auth_set_t *prev, char *data)
/*
* add an entry to auth set dynamic list
*/
{
 auth_set_t *workptr;
 char *set_name_tmp,*login_tmp,*password1_tmp,*password2_tmp;

 if(strlen(data)==0){return prev;}

 if( (workptr = malloc(sizeof(auth_set_t))) == NULL)
  goto malloc_fail;

 set_name_tmp = (char *)strtok(data," ");
 login_tmp = (char *)strtok(NULL," ");
 password1_tmp = (char *)strtok(NULL," ");
 password2_tmp = (char *)strtok(NULL," ");

 if((set_name_tmp == NULL) ||
    (login_tmp == NULL) ||
    (password1_tmp == NULL)) /* auth set must contain at least login and one password */

    {
     a_debug_info2(DEBUGLVL3,"auth_set_add: incomplete auth set found in config file!");
     a_logmsg("auth_set_add: incomplete auth set found in config file!");
     return prev;
    }

 a_debug_info2(DEBUGLVL5,"auth_set_add: new auth_set %s.",set_name_tmp);

 if( (workptr->set_name = malloc(strlen(set_name_tmp)+1)) == NULL)
  goto malloc_fail;
 if( (workptr->login = malloc(strlen(login_tmp)+1)) == NULL)
  goto malloc_fail;
 if( (workptr->password1 = malloc(strlen(password1_tmp)+1)) == NULL)
  goto malloc_fail;

 if(password2_tmp != NULL)
  if( (workptr->password2 = malloc(strlen(password2_tmp)+1)) == NULL)
   goto malloc_fail;

 strcpy(workptr->set_name,set_name_tmp);
 strcpy(workptr->login,login_tmp);
 strcpy(workptr->password1,password1_tmp);

 a_tolower_str(workptr->set_name);

 if(password2_tmp != NULL)
  strcpy(workptr->password2,password2_tmp);
 else workptr->password2=NULL;

 workptr->prev = prev;

 return (struct auth_set_t *)workptr;

 malloc_fail:
  a_debug_info2(DEBUGLVL3,"auth_set_add: malloc failed!");
  a_logmsg("auth_set_add: malloc failed!");
  return prev;


}


struct config_regexp_t *a_config_regexp_add
(struct config_regexp_t *prev, char *data)
/*
* add an entry to global config regexp list
*/
{
  config_regexp_t *workptr;
  char *regexp_string,*username_token;

  if( (workptr = malloc(sizeof(config_regexp_t))) == NULL)
   goto malloc_fail;

  regexp_string = (char *)strtok(data," ");
  username_token = (char *)strtok(NULL," ");

  if((regexp_string == NULL) || (username_token == NULL))
   {
    a_debug_info2(DEBUGLVL3,"config_regexp_add: incomplete entry found in config file!");
    a_logmsg("incomplete ConfigRegexp entry found in config file!");
    return prev;
   }

  if( (workptr->config_regexp_string = malloc(strlen(regexp_string)+1)) == NULL)
   goto malloc_fail;
  if( (workptr->username_field_token = malloc(strlen(username_token)+1)) == NULL)
   goto malloc_fail;

  strcpy(workptr->config_regexp_string,regexp_string);
  strcpy(workptr->username_field_token,username_token);

  workptr->prev = prev;

  a_debug_info2(DEBUGLVL3,"config_regexp_add: regexp: \'%s\' with username token: \'%s\'",
                workptr->config_regexp_string,

  workptr->username_field_token);

  return (struct config_regexp_t *)workptr;

  malloc_fail:
   a_debug_info2(DEBUGLVL3,"a_config_regexp_add: malloc failed!");
   a_logmsg("a_config_regex_add: malloc failed!");
   return prev;

}

struct router_db_entry_t *a_router_db_list_add
(struct router_db_entry_t *prev, char *data)
/*
* add an entry to router DB dynamic list (or) just convert a text line to a router_db_entry_t
*/
{
  router_db_entry_t *workptr;
  char *group_tmp,*hostname_tmp,*hosttype_tmp,*authset_tmp,*auth_method_tmp;

  if( (workptr = malloc(sizeof(router_db_entry_t))) == NULL)
   goto malloc_fail;

  group_tmp = (char *)strtok(data,":");
  hostname_tmp = (char *)strtok(NULL,":");
  hosttype_tmp = (char *)strtok(NULL,":");
  authset_tmp = (char *)strtok(NULL,":");
  auth_method_tmp = (char *)strtok(NULL,":");

  if((group_tmp == NULL) || (hostname_tmp == NULL) || (hosttype_tmp == NULL) ||
     (authset_tmp == NULL) || (auth_method_tmp == NULL))
   {
    a_debug_info2(DEBUGLVL3,"%s: a_router_db_list_add: incomplete entry found in router.db!",hostname_tmp);
    a_logmsg("%s: incomplete entry found in router.db!",hostname_tmp);
    return prev;
   }

  if( (workptr->group = malloc(strlen(group_tmp)+1)) == NULL)
   goto malloc_fail;
  if( (workptr->hostname = malloc(strlen(hostname_tmp)+1)) == NULL)
   goto malloc_fail;
  if( (workptr->hosttype = malloc(strlen(hosttype_tmp)+1)) == NULL)
   goto malloc_fail;
  if( (workptr->authset = malloc(strlen(authset_tmp)+1)) == NULL)
   goto malloc_fail;
  if( (workptr->arch_method = malloc(strlen(auth_method_tmp)+1)) == NULL)
   goto malloc_fail;

  strcpy(workptr->group,group_tmp);
  strcpy(workptr->hostname,hostname_tmp);
  strcpy(workptr->hosttype,hosttype_tmp);
  strcpy(workptr->authset,authset_tmp);
  strcpy(workptr->arch_method,auth_method_tmp);

  workptr->archived_now = 0;

  workptr->prev = prev;

  G_router_db_entries++;

  #ifdef USE_MYSQL
  a_debug_info2(DEBUGLVL5,"a_router_db_list_add: converting device data: %s:%s:%s:%s:%s.", 
                workptr->group,workptr->hostname,workptr->hosttype,workptr->authset,workptr->arch_method);
  #else
  a_debug_info2(DEBUGLVL5,"a_router_db_list_add: new device: %s:%s:%s:%s:%s",
                workptr->group,workptr->hostname,workptr->hosttype,workptr->authset,workptr->arch_method);
  #endif

  return (struct router_db_entry_t *)workptr;

  malloc_fail:
   a_debug_info2(DEBUGLVL3,"a_router_db_list_add: malloc failed!");
   a_logmsg("a_router_db_list_add: malloc failed!");
   return prev;

}


void a_parse_config_info
(char *config_lines[], struct config_info_t *conf_struct)
/*
*
* parse config table and load config values into appropriate structures
* check keywords and corresponding data for basic validity
*
*/ 
{
 
  char *conf_field;
  char cron_job[255];
  char auth_set_data[255];
  char config_regexp_data[255];
  char *tmp;
  int i = 1,tmp1;

  time(&G_now);

  G_jobcount = 0;

  if(conf_struct->logging)
   {
    /* auto-adding log marking cronjob: if logging enabled - insert marker into logfile each 12 hours */
    a_debug_info2(DEBUGLVL5,"a_parse_config_info: auto-adding log-marker cronjob...");
    strcpy(cron_job,"00 00,12 * * * log-marker");
    G_cronjobs[G_jobcount] = (cronjob_t *)a_job_parse(cron_job,&G_jobcount); 
   }

  if(G_config_dump_memstats)
   {
    /* if configured - add memory stats dump every 24h  */
    a_debug_info2(DEBUGLVL5,"a_parse_config_info: auto-adding dump-memstats cronjob...");
    strcpy(cron_job,"01 00 * * * dump-memstats");
    G_cronjobs[G_jobcount] = (cronjob_t *)a_job_parse(cron_job,&G_jobcount);
   }


#ifdef USE_MYSQL


  a_debug_info2(DEBUGLVL5,"a_parse_config_info: auto-adding timestamp-updating cronjob...");
  strcpy(cron_job,"* * * * * update-timestamp");
  G_cronjobs[G_jobcount] = (cronjob_t *)a_job_parse(cron_job,&G_jobcount);

  MYSQL_RES *raw_archivist_config;
  MYSQL_ROW archivist_config;


  while(config_lines[i] != NULL)
   {

    conf_field = (char *)strtok(config_lines[i], " ");

    if(strstr(conf_field,"MYSQLServer"))
        {
         conf_field = (char *)strtok(NULL, " ");
         if(strlen(conf_field)>0)
         {strcpy(conf_struct->mysql_server,conf_field);}
        }
    if(strstr(conf_field,"MYSQLUser"))
        {
         conf_field = (char *)strtok(NULL, " ");
         if(strlen(conf_field)>0)
        {strcpy(conf_struct->mysql_user,conf_field);}
        }
    if(strstr(conf_field,"MYSQLPassword"))
        {
        conf_field = (char *)strtok(NULL, " ");
         if(strlen(conf_field)>0)
         {strcpy(conf_struct->mysql_password,conf_field);}
        }

    i++;

   }

  /* we have MYSQL connection info now - connect to the database, 
   * and read rest of the configuration from there */

  a_mysql_connect();

  raw_archivist_config = a_mysql_select("select * from archivist_config");
  archivist_config = mysql_fetch_row(raw_archivist_config);

  /* instance_id */ 
  tmp1 = atoi(archivist_config[0]);
  if((tmp1 > 0) && (tmp1 < 16)) 
   conf_struct->instance_id = tmp1;
  else
  {
   printf("FATAL: a_parse_config_info: instance number is not in 1 - 16 range!\n");
   a_cleanup_and_exit();
  }

  /* working_dir */
  if( (strlen(archivist_config[1]) > 0) && (strlen(archivist_config[1]) < MAXPATH) ) 
   strcpy(conf_struct->working_dir,archivist_config[1]);
  else a_config_error("WorkingDir");  

  /* logging */
  tmp1 = atoi(archivist_config[2]);
  if((tmp1 == 0 || tmp1 == 1)) 
   conf_struct->logging = tmp1;
  else a_config_error("Logging");

  /* log_filename */
  if( (strlen(archivist_config[3]) > 0) && (strlen(archivist_config[3]) < MAXPATH) )
   strcpy(conf_struct->log_filename,archivist_config[3]);
  else a_config_error("LogFilename");

  /* archiving method */
  if(strlen(archivist_config[4]) > 0)
   {
    if(strstr(archivist_config[4],"rancid")) conf_struct->archiving_method = ARCHIVE_USING_RANCID;
    if(strstr(archivist_config[4],"internal")) conf_struct->archiving_method = ARCHIVE_USING_INTERNAL;
    else a_config_error("ArchivingMethod");
   }
  else a_config_error("ArchivingMethod");
 
  /* TFTP directory */
  if( (strlen(archivist_config[5]) > 0) && (strlen(archivist_config[5]) < MAXPATH) )
   strcpy(conf_struct->tftp_dir,archivist_config[5]);
  else a_config_error("TFTPDir");
 
  /* TFTP server IP */
  if(strlen(archivist_config[6]) > 0)
   strcpy(conf_struct->tftp_ip,archivist_config[6]);

  /* path to helper scripts directory */
  if( (strlen(archivist_config[7]) > 0) && (strlen(archivist_config[7]) < MAXPATH) )
   strcpy(conf_struct->script_dir,archivist_config[7]);
  else a_config_error("InternalScripts");

  /* path to rancid exec */
  if( (strlen(archivist_config[8]) > 0) && (strlen(archivist_config[8]) < MAXPATH) )
   strcpy(conf_struct->rancid_exec_path,archivist_config[8]);
  else a_config_error("RancidExecPath");

  /* path to expect exec */
  if( (strlen(archivist_config[9]) > 0) && (strlen(archivist_config[9]) < MAXPATH) )
   strcpy(conf_struct->expect_exec_path,archivist_config[9]);
  else a_config_error("ExpectExecPath");

  /* tail some syslog file ? */
  tmp1 = atoi(archivist_config[10]);
  if((tmp1 == 0 || tmp1 == 1)) 
   conf_struct->tail_syslog = tmp1;
  else a_config_error("TailSyslogFile");

  /* name of syslog file to tail */
  if( (strlen(archivist_config[11]) > 0) && (strlen(archivist_config[11]) < MAXPATH) )
   strcpy(conf_struct->syslog_filename,archivist_config[11]);
  else a_config_error("TailFilename");

  /* listen to syslog on UDP socket ? */
  tmp1 = atoi(archivist_config[12]);
   if(( tmp1 == 0 || tmp1 == 1 )) conf_struct->listen_syslog = tmp1;
  else a_config_error("ListenSyslog");

  /* listen to syslog on port */
  tmp1 = atoi(archivist_config[13]);
   if((tmp1 > 0 || tmp1 < 65535)) conf_struct->syslog_port = tmp1;
  else a_config_error("SyslogPort");

  /* keep changelog ? */
  tmp1 = atoi(archivist_config[14]);
   if((tmp1 == 0 || tmp1 == 1)) conf_struct->keep_changelog = tmp1;
  else a_config_error("KeepChangelog");

  /* changelog filename */
  if( (strlen(archivist_config[15]) > 0) && (strlen(archivist_config[15]) < MAXPATH) )
   strcpy(conf_struct->changelog_filename,archivist_config[15]);
  else a_config_error("ChangelogFile");

  /* SVN repository path */
  if( (strlen(archivist_config[17]) > 0) && (strlen(archivist_config[17]) < MAXPATH) )
   {
    conf_struct->repository_path = malloc((strlen(archivist_config[17]))+1);
    strcpy(conf_struct->repository_path,archivist_config[17]);
   }
  else a_config_error("RepositoryPath");

  /* archiver thread count */
  tmp1 = atoi(archivist_config[18]);
   if(( tmp1 > 0 || tmp1 < 256 )) conf_struct->archiver_threads = tmp1;
  else a_config_error("ArchiverThreads");
  
  int record_counter = 0;
  char data[1024];

  if(raw_archivist_config = a_mysql_select("select * from auth_sets"))
   while((archivist_config = mysql_fetch_row(raw_archivist_config)))
    {
     bzero(data,1024);
     snprintf(data,1024,"%s %s %s %s",archivist_config[0],archivist_config[1],
              archivist_config[2],archivist_config[3]);
     record_counter++;
     G_auth_set_list = a_auth_set_add(G_auth_set_list,data);
     a_debug_info2(DEBUGLVL5,"a_parse_config_info: adding auth set from SQL database (%s)",data);
    }

  if(raw_archivist_config = a_mysql_select("select * from config_regexp"))
   while((archivist_config = mysql_fetch_row(raw_archivist_config)))
    {
     bzero(data,1024);
     snprintf(data,1024,"%s %s",archivist_config[0],archivist_config[1]);
     G_config_regexp_list = a_config_regexp_add(G_config_regexp_list,data);
     a_debug_info2(DEBUGLVL5,"a_parse_config_info: adding config regexp from SQL database (%s)",data);
    }

 if(raw_archivist_config = a_mysql_select("select * from cronjobs"))
   while((archivist_config = mysql_fetch_row(raw_archivist_config)))
    {
     bzero(data,1024);
     snprintf(data,1024,"%s %s %s %s %s %s",
              archivist_config[0],archivist_config[1],archivist_config[2],
              archivist_config[3],archivist_config[4],archivist_config[5]);
     if(G_cronjobs[G_jobcount] = (cronjob_t *)a_job_parse(data,&G_jobcount) != NULL)
      a_debug_info2(DEBUGLVL5,"a_parse_config_info: adding cron job from SQL database (%s)",data);
    }


#else

  while(config_lines[i]!=NULL)
   {

    conf_field = (char *)strtok(config_lines[i], " ");

    if(strstr(conf_field,"ScheduleBackup")) 
         {
          bzero(cron_job,254);

          while( (tmp = (char *)strtok(NULL, " ")) != NULL )
           {
            strcat(cron_job,tmp); 
            strcat(cron_job," ");
           }

          a_debug_info2(DEBUGLVL5,"a_parse_config_info: passing %s to a_job_parse",cron_job);

          if((G_cronjobs[G_jobcount] = (cronjob_t *)a_job_parse(cron_job,&G_jobcount)) != NULL)
           a_debug_info2(DEBUGLVL5,"a_parse_config_info: G_jobcount now: %d",G_jobcount);
         }

    if(strstr(conf_field,"InstanceID"))
         {
          conf_field = (char *)strtok(NULL, " ");
          tmp1 = atoi(conf_field);
          if((tmp1 > 0) && (tmp1 < 16)) 
           conf_struct->instance_id = tmp1;  
          else
           {
            printf("FATAL: a_parse_config_info: instance number is not in 1 - 16 range!\n");
            a_cleanup_and_exit();
           }
         }

    if(strstr(conf_field,"WorkingDirectory"))
         {
          conf_field = (char *)strtok(NULL, " ");
          if( (strlen(conf_field) > 0) && (strlen(conf_field) < MAXPATH) )
           strcpy(conf_struct->working_dir,conf_field);
          else 
           a_config_error("WorkingDirectory");
         }

    if(strstr(conf_field,"Logging")) 
         {
          conf_field = (char *)strtok(NULL, " ");
          tmp1 = atoi(conf_field);
          if((tmp1 == 0 || tmp1 == 1)) 
           conf_struct->logging = tmp1;
          else 
           a_config_error("Logging");
         }

    if(strstr(conf_field,"LogFile")) 
         {
          conf_field = (char *)strtok(NULL, " ");
          if( (strlen(conf_field) > 0) && (strlen(conf_field) < MAXPATH) )
           strcpy(conf_struct->log_filename,conf_field);
          else 
           a_config_error("LogFile");
         }

    if(strstr(conf_field,"RepositoryPath")) 
         {
          conf_field = (char *)strtok(NULL, " ");
          if( (strlen(conf_field) > 0) && (strlen(conf_field) < MAXPATH) )
           {
            conf_struct->repository_path = malloc((strlen(conf_field))+1);
            strcpy(conf_struct->repository_path,conf_field);
           }
          else 
           a_config_error("RepositoryPath");
         }

    if(strstr(conf_field,"TailSyslogFile")) 
         {
          conf_field = (char *)strtok(NULL, " ");
          tmp1 = atoi(conf_field);
          if((tmp1 == 0 || tmp1 == 1)) 
           conf_struct->tail_syslog = tmp1;
          else 
           a_config_error("TailSyslogFile");
         }

    if(strstr(conf_field,"TailFilename")) 
         {
          conf_field = (char *)strtok(NULL, " ");
          if( (strlen(conf_field) > 0) && (strlen(conf_field) < MAXPATH) )
           strcpy(conf_struct->syslog_filename,conf_field);
          else 
           a_config_error("TailFilename");
         }

    if(strstr(conf_field,"KeepChangelog"))
         {
          conf_field = (char *)strtok(NULL, " ");
          tmp1 = atoi(conf_field);
          if((tmp1 == 0 || tmp1 == 1)) 
           conf_struct->keep_changelog = tmp1;
          else 
           a_config_error("KeepChangelog");
         }

    if(strstr(conf_field,"ChangelogFile"))
         {
          conf_field = (char *)strtok(NULL, " ");
          if( (strlen(conf_field) > 0) && (strlen(conf_field) < MAXPATH) )
           strcpy(conf_struct->changelog_filename,conf_field);
          else
           a_config_error("ChangelogFile");
         }
    if(strstr(conf_field,"ListenSyslog")) 
         {
          conf_field = (char *)strtok(NULL, " ");
          tmp1 = atoi(conf_field);
          if((tmp1 == 0 || tmp1 == 1)) 
           conf_struct->listen_syslog = tmp1;
          else
           a_config_error("ListenSyslog");
         }

    if(strstr(conf_field,"ListenCommands"))
         {
          conf_field = (char *)strtok(NULL, " ");
          tmp1 = atoi(conf_field);
          if((tmp1 == 0 || tmp1 == 1))
           conf_struct->open_command_socket = tmp1;
          else
           a_config_error("ListenComands");
         }

    if(strstr(conf_field,"SyslogPort")) 
         {
          conf_field = (char *)strtok(NULL, " ");
          tmp1 = atoi(conf_field);
          if((tmp1 > 0 || tmp1 < 65535)) 
           conf_struct->syslog_port = tmp1;
          else
           a_config_error("SyslogPort");
         }

     if(strstr(conf_field,"ArchiverThreads"))
         {
          conf_field = (char *)strtok(NULL, " ");
          tmp1 = atoi(conf_field);
          /* 256 concurrent single-device threads max. */
          if((tmp1 > 0 || tmp1 < 256)) 
           conf_struct->archiver_threads = tmp1; 
          else
           a_config_error("ArchiverThreads");
         }

    if(strstr(conf_field,"RancidExecPath"))
         {
          conf_field = (char *)strtok(NULL, " ");
          if( (strlen(conf_field) > 0) && (strlen(conf_field) < MAXPATH) )
           strcpy(conf_struct->rancid_exec_path,conf_field);
          else a_config_error("RancidExecPath");
         }

    if(strstr(conf_field,"ExpectExecPath"))
         {
          conf_field = (char *)strtok(NULL, " ");
          if( (strlen(conf_field) > 0) && (strlen(conf_field) < MAXPATH) )
           strcpy(conf_struct->expect_exec_path,conf_field);
          else a_config_error("ExpectExecPath");
         }

    if(strstr(conf_field,"RouterDBPath"))
         {
          conf_field = (char *)strtok(NULL, " ");
          if( (strlen(conf_field) > 0) && (strlen(conf_field) < MAXPATH) )
           { 
           conf_struct->router_db_path = malloc((strlen(conf_field))+1);
           strcpy(conf_struct->router_db_path,conf_field);
           }
          else a_config_error("RouterDBPath");
         }

    if(strstr(conf_field,"CommandSocketPath"))
         {
          conf_field = (char *)strtok(NULL, " ");
          if( (strlen(conf_field) > 0) && (strlen(conf_field) < MAXPATH) )
           {
           conf_struct->router_db_path = malloc((strlen(conf_field))+1);
           strcpy(conf_struct->command_socket_path,conf_field);
           }
          else a_config_error("CommandSocketPath");
         }

    if(strstr(conf_field,"TerminalArchivingMethod"))
         {
          conf_field = (char *)strtok(NULL, " ");
          if( (strlen(conf_field) > 0) && (strlen(conf_field) < MAXPATH) )
           {
            if(strstr(conf_field,"rancid")) conf_struct->archiving_method = ARCHIVE_USING_RANCID;
            if(strstr(conf_field,"internal")) conf_struct->archiving_method = ARCHIVE_USING_INTERNAL;
           }
          else a_config_error("TerminalArchivingMethod");
         }

    if(strstr(conf_field,"InternalScripts"))
        {
         conf_field = (char *)strtok(NULL, " ");
         if( (strlen(conf_field) > 0) && (strlen(conf_field) < MAXPATH) )
          strcpy(conf_struct->script_dir,conf_field);
         else a_config_error("InternalScripts");
        }

    if(strstr(conf_field,"TFTPDir"))
        {
         conf_field = (char *)strtok(NULL, " ");
         if( (strlen(conf_field) > 0) && (strlen(conf_field) < MAXPATH) )
          strcpy(conf_struct->tftp_dir,conf_field);
         else a_config_error("TFTPDir");
        }

    if(strstr(conf_field,"TFTPIP"))
        {
         conf_field = (char *)strtok(NULL, " ");
         if(a_is_valid_ip(conf_field))
          strcpy(conf_struct->tftp_ip,conf_field);
         else a_config_error("TFTPIP");
        }

    if(strstr(conf_field,"AuthSet"))
        {
         bzero(auth_set_data,255);
         while( (tmp = (char *)strtok(NULL, " ")) != NULL )
          {
           strcat(auth_set_data,tmp); 
           strcat(auth_set_data," ");
          }
         G_auth_set_list = a_auth_set_add(G_auth_set_list,auth_set_data);
        }

    if(strstr(conf_field,"ConfigRegexp"))
        {
         bzero(config_regexp_data,255);
         while( (tmp = (char *)strtok(NULL, " ")) != NULL )
          {
           strcat(config_regexp_data,tmp); 
           strcat(config_regexp_data," ");
          }
         G_config_regexp_list = a_config_regexp_add(G_config_regexp_list,config_regexp_data);
        }

    if(strstr(conf_field,"EncryptedAuthSet"))
        {
         bzero(auth_set_data,255);
         while( (tmp = (char *)strtok(NULL, " ")) != NULL )
          {
           strcat(auth_set_data,tmp); 
           strcat(auth_set_data," ");
          }
         a_decrypt_auth_set_data(auth_set_data);
         G_auth_set_list = a_auth_set_add(G_auth_set_list,auth_set_data);
        }

    i++;

  }

#endif

   /* check for required options */

#ifndef USE_MYSQL
   if(conf_struct->router_db_path == NULL) 
    {
     printf("FATAL: no RouterDBPath defined in config file!\n"); 
     a_cleanup_and_exit();
    }
#endif

   if(conf_struct->repository_path == NULL) 
    {
     printf("FATAL: no RepositoryPath defined!\n"); 
     a_cleanup_and_exit();
    }

#ifdef USE_MYSQL
   a_debug_info2(DEBUGLVL3,"a_parse_config_info: configured from MYSQL database.");
#else
   a_debug_info2(DEBUGLVL3,"a_parse_config_info: configured from config file.");
#endif

   a_debug_info2(DEBUGLVL5,"a_parse_config_info: parsed config data:\n logging = %d\n log_filename = %s\n svn_path = %s\n tail_syslog = %d\n syslog_filename = %s\n listen_syslog = %d\n syslog_port = %d",
   conf_struct->logging,conf_struct->log_filename,conf_struct->repository_path,
   conf_struct->tail_syslog,conf_struct->syslog_filename,conf_struct->listen_syslog,
   conf_struct->syslog_port);

} 


auth_set_t *a_auth_set_search
(auth_set_t *auth_set_list_idx, char *setname)
/*
* search auth_set list
*/
{
  auth_set_t *tmp_pointer;
  char *strstr_out;

  tmp_pointer = auth_set_list_idx;

  a_tolower_str(setname);

  while(tmp_pointer!=NULL)
   {
    strstr_out = strstr(tmp_pointer->set_name,setname);
    if(strstr_out != NULL)
     if(strlen(strstr_out) == strlen(setname))
      {
       return tmp_pointer; /* found entry - return address of struct */
      }
    tmp_pointer = tmp_pointer->prev;
   }

 return NULL; /* return nothing as searched auth set was not found on the auth set list */

}


router_db_entry_t *a_router_db_search
(router_db_entry_t *router_db_idx, char *hostname)
/*
* search router DB dynamic list (or MYSQL table router_db).
* 
*/
{
   router_db_entry_t *tmp_pointer;

#ifndef USE_MYSQL /* search version for linked list */

   char *strstr_out;

   tmp_pointer = router_db_idx;

   a_debug_info2(DEBUGLVL5,"a_router_db_search: linked list at 0x%p",router_db_idx);

   if(strlen(hostname) > 0)  /* don't try to search for empty string */

    while(tmp_pointer != NULL)
     {

     if(strlen(a_trimwhitespace(tmp_pointer->hostname)) == 
        strlen(a_trimwhitespace(hostname)) ) /* we want exact match */
      {

       strstr_out = a_mystristr(tmp_pointer->hostname, hostname);

       if(strstr_out != NULL) 
         return tmp_pointer; /* found entry - return address of struct */
       }

      tmp_pointer = tmp_pointer->prev;

     }

#else  /* search version for MYSQL db */
  
   char *sql_query_string;
   char data[1024];
   MYSQL_RES *raw_sql_res;
   MYSQL_ROW sql_res;

   sql_query_string = malloc(1024);
   snprintf(sql_query_string,MAXQUERY,"select * from router_db where hostname = '%s'",hostname);
   
   if( (raw_sql_res = a_mysql_select(sql_query_string)) != NULL )
    {
     if( !(sql_res = mysql_fetch_row(raw_sql_res)) ) 
      return NULL;
    }  /* not found */
   else 
    {
     a_debug_info2(DEBUGLVL3,"a_router_db_search: cannot exec SQL query (%s) !",sql_query_string);
     return NULL;
    }

   if(strlen(sql_res[0])) /* true if found */
    {
     snprintf(data,1024,"%s:%s:%s:%s:%s",sql_res[0],sql_res[1],sql_res[2],sql_res[3],sql_res[4]);
     a_debug_info2(DEBUGLVL5,"a_router_db_search: found device in SQL database: %s",data);
     tmp_pointer = a_router_db_list_add(tmp_pointer,data); /* convert SQL data to router_db_entry_t */
     return tmp_pointer;       /* this has got to be freed somewhere later, or memory leak will occur */
    }

   free(sql_query_string);

 #endif

   return NULL; /* return nothing as searched hostname was not found in device database */
}

 
int a_is_archived_now
(router_db_entry_t *router_db_idx, char *hostname)
/*
* check router DB for a locked entry - if locked, this means that the device 
* is being archived just now by other thread. 
* 
*/
{

#ifndef USE_MYSQL
   router_db_entry_t *tmp_pointer;
   char *strstr_out;

   tmp_pointer = router_db_idx;

   while(tmp_pointer != NULL)
    {

     strstr_out = a_mystristr(hostname, tmp_pointer->hostname);

     if(strstr_out != NULL)
      if(strlen(strstr_out) == strlen(hostname))
      {
       a_debug_info2(DEBUGLVL5,"a_is_archived_now: device %s, value: %d !",hostname,tmp_pointer->archived_now);
       return tmp_pointer->archived_now; /* found entry - return archivization flag */
      }
     tmp_pointer = tmp_pointer->prev;
    }

   a_debug_info2(DEBUGLVL5,"a_is_archived_now: device %s not found on the linked-list!\n",hostname);

   return NULL; /* return null as hostname was not found in device database */
#else
   MYSQL_RES *raw_sql_res;
   MYSQL_ROW sql_res;

   char *sql_query_string;
   int retval;

   sql_query_string = malloc(1024);
   snprintf(sql_query_string,MAXQUERY,"select archived_now from router_db where hostname = '%s'",hostname);

   if(raw_sql_res = a_mysql_select(sql_query_string))
    sql_res = mysql_fetch_row(raw_sql_res);
   else
    {
     a_debug_info2(DEBUGLVL3,"a_is_archived_now: cannot exec SQL query (%s) !",sql_query_string);
     free(sql_query_string);
     return NULL;
    }

   free(sql_query_string);
   retval = atoi(sql_res[0]);

   if((retval == 0) || (retval == 1))
   return retval; else return NULL;
 
#endif
}


int a_set_archived
(router_db_entry_t *router_db_idx, char *hostname, int arch_status)
/*
* set/clear ongoing archivization flag. (this is done to avoid two concurrent archivizations of the
* same device). mutexes are operated before calling this routine, so there is no variable 
* locking/checking here. 
*/
{

#ifndef USE_MYSQL
   router_db_entry_t *tmp_pointer;
   char *strstr_out;
  
   tmp_pointer = router_db_idx;

   while(tmp_pointer != NULL)
    {

     strstr_out = a_mystristr(hostname, tmp_pointer->hostname);

     if(strstr_out != NULL)
      if(strlen(strstr_out) == strlen(hostname))
       {
        tmp_pointer->archived_now = arch_status; /* found entry - set archivization flag  */
        return 1; /* done */
       }
     tmp_pointer = tmp_pointer->prev;
    }
   return NULL; /* return null as hostname was not found in device database */
#else
   char *sql_query_string;
   int retval;

   sql_query_string = malloc(1024);
   snprintf(sql_query_string,MAXQUERY,"update router_db set archived_now = %d where hostname = '%s'",
            arch_status,hostname);

   retval = a_mysql_update(sql_query_string);

   free(sql_query_string);

   if(retval)
    return 1; /* archivization flag succefully set */
   else
    {
     a_debug_info2(DEBUGLVL3,"a_set_archived: SQL update failed (%s) !",sql_query_string);
     return NULL;
    }
#endif
   
}



#ifndef USE_MYSQL

void a_router_db_free
(router_db_entry_t *start)
/*
* delete router.db from memory. used in case of SHGHUP to re-read router.db from disk
*/
{
   router_db_entry_t *tmp_pointer,*freethis; 
   
   tmp_pointer = start;

    while(tmp_pointer!=NULL)
    {
     freethis = tmp_pointer;
     tmp_pointer = tmp_pointer->prev;
     free(freethis);
    }

}

#endif

struct router_db_entry_t *a_load_router_db
(char *filename)
/*
* load router DB from a rancid-style file 
*/
{
#ifndef USE_MYSQL

  struct router_db_entry_t *router_db_idx=NULL; /* initialize first entry with null */
  char buf[CONFIG_MAX_LINELEN];
  int fdes,counter = 0;

  G_router_db_entries = 0;

  if((fdes = fopen(filename, "r")) <= 0) 
   {
    printf("FATAL: cannot open %s (%d)!\n",filename,errno); 
    a_cleanup_and_exit();
   }

  while (fgets(buf, CONFIG_MAX_LINELEN, fdes)) {
    if (buf[0] == '#' || buf[0] == '\0' || buf[0] == '\n' )
        continue;
    buf[strlen(buf)-1] = '\0'; /* remove trailing \n */
    router_db_idx = a_router_db_list_add(router_db_idx,buf); /*translate a text line to a router_db_entry_t */
    counter++;
    }

   fclose((FILE *)fdes);
   if(router_db_idx == NULL)
    {
     a_debug_info2(DEBUGLVL5,"a_load_router_db: nothing loaded from router.db. no suitable entry found!");
     a_logmsg("FATAL: router.db does not contain any suitable entries! nothing to do - exit.");
     a_cleanup_and_exit();
    }
   else
     a_debug_info2(DEBUGLVL5,"a_load_router_db: %d entries loaded from router.db",counter);

   return (struct router_db_entry_t *)router_db_idx;

#else

  MYSQL_RES *raw_row;
  MYSQL_ROW row;
  int router_db_count = 0;

  /* check if there are any devices in the database */

  raw_row = a_mysql_select("select count(*) from router_db"); 
  row = mysql_fetch_row(raw_row);

  router_db_count = atoi(row[0]);

  if(router_db_count <= 0)
   {
    a_debug_info2(DEBUGLVL5,"a_load_router_db: there are no devices in the database! exit.");
    a_logmsg("FATAL: device database does not contain any suitable entries! nothing to do - exit.");
    a_cleanup_and_exit();
   }
  else
   {
    a_debug_info2(DEBUGLVL5,"a_load_router_db: %d entries in MYSQL-based router_db.",router_db_count);
    G_router_db_entries = router_db_count;
   }

  return NULL; 

#endif

}


/* end of config.c  */

