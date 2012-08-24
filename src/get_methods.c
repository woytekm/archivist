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
*    along with Archivist.  If not, see <http://www.gnu.org/licenses/>.
*
*    Author: Wojtek Mitus (woytekm@gmail.com)
*
*    get_methods.c - definitions of config download methods
*/

#include<sys/stat.h>
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>

#include<EXTERN.h>
#include<perl.h>

#include "defs.h"
#include "archivist_config.h"

int a_get_from_device
(char *device_name, char *device_type, char *device_auth_set_name, char *device_arch_method)
/*
*
* dispatch config get request according to the configured config get method
*
*/
{

  int op_status = -1;

  if(strstr(device_arch_method,"snmp"))
    op_status = a_get_using_snmp(device_name,device_type,device_auth_set_name);
  else if(G_config_info.archiving_method == ARCHIVE_USING_RANCID)
    op_status = a_get_using_rancid(device_name,device_type);
  else if(G_config_info.archiving_method == ARCHIVE_USING_INTERNAL)
    op_status = a_get_using_expect(device_name,device_type,device_auth_set_name,device_arch_method);
  else return -1;

  return op_status;

}


int a_get_using_rancid
(char *device_name, char *device_type)
/*
* get device config using Really Awesome Cisco confIg Differ.
* rancid uses .cloginrc as a device credentials storage, so our AuthSets defined in config file 
* have no efect here - user must have valid .cloginrc for this method to work.
*/
{
 
   struct stat outfile;
   char rancid_command[MAXPATH];
   char rancid_hostname[MAXPATH];

   /* build filenames and command strings */
   snprintf(rancid_hostname,MAXPATH,"%s.new",device_name);
   snprintf(rancid_command,MAXPATH,"%s %s %s",G_config_info.rancid_exec_path,device_name,device_type);

   remove(rancid_hostname); /* try to remove the file */

   a_debug_info2(DEBUGLVL5,"a_pull_from_device: executing rancid %s %s ...",device_name,device_type);
   a_our_system(rancid_command);
   a_debug_info2(DEBUGLVL5,"a_pull_from_device: rancid finished...");

   if(stat(rancid_hostname,&outfile) == -1)
    {
     a_logmsg("FATAL: %s: rancid failed!",device_name); 
     return -1;
    } /* no rancid file after rancid run - fail */
   else if(outfile.st_size < MIN_WORKING_COPY_LEN) 
    {
     a_logmsg("FATAL: %s: rancid outfile is less that predefined minimum length (%d bytes) - looks like rancid failed!",
              device_name,MIN_WORKING_COPY_LEN); 
     return -1;
    } /* rancid file empty or a few bytes long - fail */
   else return 1;
 
}


int a_cleanup_config_file
(char *filename,char *device_type)
/*
* cleanup device config file downloaded using expect method. 
* the file is processed by perl script interpreted by a perl
* instance created in this program.
* this method is a little faster and cleaner than system()'ing perl 
* to re-format file, and finally it allows for easy customization 
* (scripts for user-defined platforms).
*/
{

   static PerlInterpreter *my_perl;
   struct stat perl_script_stat;
   char *argval[3];
   int perl_res,argcount=3;

   a_debug_info2(DEBUGLVL5,"a_cleanup_config_file: args: %s, %s",filename,device_type);
   
   argval[0] = malloc(5);
   argval[1] = malloc(strlen(device_type) + strlen(G_config_info.script_dir) + 20);
   argval[2] = malloc(strlen(filename) + 3);

   /* build up argument list for perl interpreter: */

   strcpy(argval[0]," "); 
   strcpy(argval[1],G_config_info.script_dir);
   strcat(argval[1],"/");
   strcat(argval[1],device_type);
   strcat(argval[1],".process"); /* <platform>.process - post-processing perl script name */
   strcpy(argval[2],filename);

   if(stat(argval[1],&perl_script_stat))
    {
     a_logmsg("cannot open %s helper script!",argval[1]); 
     return 1;
    } /* config post-processing is optional, so return 1 here */

   if(perl_script_stat.st_size == 0)
    {
     a_logmsg("%s helper script has zero size!",argval[1]);
     return 1;
    } /* config post-processing is optional, so return 1 here */
   
   if((my_perl = perl_alloc()) == NULL) 
    {
     a_logmsg("%s: cleanup_config_file: failed to create perl interpreter.",filename);
     return -1;
    } /* prepare and exec perl interpreter */

   PL_perl_destruct_level = 1; 

   perl_construct(my_perl);

   PL_exit_flags |= PERL_EXIT_DESTRUCT_END;

   perl_res = perl_parse(my_perl, NULL, argcount, argval, (char **)NULL);

   if(perl_res)
    {
     a_logmsg("%s: cleanup_config_file: failed to parse post-processing perl script!",filename);
     PL_perl_destruct_level = 1;
     perl_destruct(my_perl);
     perl_free(my_perl);
     return -1;
    }

   if(perl_run(my_perl))
    {
     a_logmsg("%s: cleanup_config_file: failed to execute post-processing perl script!",filename);
     PL_perl_destruct_level = 1;
     perl_destruct(my_perl);
     perl_free(my_perl);
     return -1;
    }

   PL_perl_destruct_level = 1; 

   perl_destruct(my_perl);

   perl_free(my_perl);

   a_refresh_signals(); /* perl parser tends to mask previous signal hookups */

   free(argval[0]);
   free(argval[1]);
   free(argval[2]);

   return 1;

}


int a_get_using_expect
(char *device_name, char *device_type, char *auth_set, char *arch_method)
/*
*
* get device config using expect script
* libexpect is not thread-safe - only way to use expect in a thread is system()
*
*/
{

  auth_set_t *device_auth_set;
  struct stat outfile;
  char script_path[MAXPATH];
  char command[MAXPATH];
  char result_file[MAXPATH];
  int system_result = 0;


  /* build a command string for expect get script */
  snprintf(script_path,MAXPATH,"%s/%s.get.%s",G_config_info.script_dir,device_type,arch_method);

  if((device_auth_set = a_auth_set_search(G_auth_set_list,auth_set)) == NULL)
   {
    a_debug_info2(DEBUGLVL3,"a_get_using_expect: auth set %s not found for device %s. not archiving!",
    auth_set,device_name); 
    return -1;
   }

  snprintf(command,MAXPATH,"%s %s %s %s %s ",
           G_config_info.expect_exec_path,script_path,device_name,
           device_auth_set->login,device_auth_set->password1);

  if(device_auth_set->password2 != NULL)
   strcat(command,device_auth_set->password2);
  else 
   strcat(command,device_auth_set->password1);

  snprintf(result_file,MAXPATH,"%s.new",device_name);

  struct stat helper_script_stat;

  if(stat(script_path,&helper_script_stat))
   {
    a_logmsg("FATAL: cannot open %s helper script!",script_path);
    return -1;
   }
  
  if(helper_script_stat.st_size == 0)
   {
    a_logmsg("FATAL: %s helper script has zero size!",script_path);
    return -1;
   }

  system_result = a_our_system(command); 

  if((system_result >> 8) > 0)
   { 
    a_debug_info2(DEBUGLVL5,"a_get_using_expect: %s: expect script failed!",device_name);
    a_logmsg("%s: expect script failed!",device_name);
    return -1;
   }
  
  if(stat(result_file,&outfile) == -1)
   {
    a_logmsg("%s: expect method: no downloaded config file found.",device_name);
    return -1; /* no rancid file after rancid run - fail. */
   }
  else if(outfile.st_size < MIN_WORKING_COPY_LEN) 
   {
    a_logmsg("%s: expect method: device config file is shorter than minimum expected size (%d bytes)!",device_name,MIN_WORKING_COPY_LEN);
    remove(result_file);
    return -1; /* rancid file empty or a few bytes long - fail. */
   }
  else 
   {
    a_debug_info2(DEBUGLVL5,"a_get_using_expect: re-formatting device config file.");
    pthread_mutex_lock(&G_perl_running_mutex);

    if(a_cleanup_config_file(result_file,device_type) == -1) /* re-format output config file */
     {
      pthread_mutex_unlock(&G_perl_running_mutex);
      a_logmsg("%s: expect method: post-processing config file failed.",device_name);
      remove(result_file);
      return -1;
     }

    pthread_mutex_unlock(&G_perl_running_mutex);
   }

  return 1;

}

/* end of get_methods.c */
