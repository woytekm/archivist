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

#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/stat.h>

#include "Python.h"

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
* get device config using Really Awesome Cisco confIg Differ frontend (rancid-fe)
* rancid uses .cloginrc as a device credentials storage, so our AuthSets defined in config file 
* have no efect here - user must have valid .cloginrc for this method to work.
*/
{
 
   struct stat outfile;
   char rancid_command[MAXPATH];
   char rancid_filename[MAXPATH];

   /* build filenames and command strings */
   snprintf(rancid_filename,MAXPATH,"%s.new",device_name);
   snprintf(rancid_command,MAXPATH,"%s %s:%s",G_config_info.rancid_exec_path,device_name,device_type);

   remove(rancid_filename); /* try to remove the file */

   a_debug_info2(DEBUGLVL5,"a_get_using_rancid: executing rancid %s %s:%s...",
                 G_config_info.rancid_exec_path, device_name, device_type);
   a_our_system(rancid_command);
   a_debug_info2(DEBUGLVL5,"a_get_using_rancid: rancid finished...");

   if(stat(rancid_filename,&outfile) == -1)
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
(char *filename,char *platform_type)
/*
*
* cleanup device config file downloaded using expect method.
* (python version)
*
*/
{

 int py_argc, python_run_fail = 0;
 char * py_argv[3];
 struct stat script_file;
 PyThreadState *myThreadState;
 PyObject *PyFileObject;
 PyObject *PyNullFile;

 py_argc = 2;
 if( ((py_argv[0] = malloc(strlen(platform_type) + strlen(G_config_info.script_dir) + 20)) == NULL) ||
     ((py_argv[1] = malloc(strlen(filename) + 3)) == NULL ) )
  {
   a_debug_info2(DEBUGLVL3,"a_cleanup_config_file: malloc failed!");
   return -1;
  }


 strcpy(py_argv[0],G_config_info.script_dir);
 strcat(py_argv[0],"/");
 strcat(py_argv[0],platform_type);
 strcat(py_argv[0],".process.py");

 strcpy(py_argv[1],filename);

 if(stat(py_argv[0],&script_file))
  {
   a_logmsg("FATAL: cannot open %s post-processing script!",py_argv[0]);
   return -1;
  }

 if(script_file.st_size == 0)
  {
   a_logmsg("FATAL: %s post-processing script file has size zero!",py_argv[0]);
   return -1;
  }


 a_debug_info2(DEBUGLVL5,"a_cleanup_config_file: python script to be interpreted: %s",py_argv[0]);

 a_debug_info2(DEBUGLVL5,"a_cleanup_config_file: acquiring GIL...");

 PyEval_AcquireLock();

 a_debug_info2(DEBUGLVL5,"a_cleanup_config_file: GIL acquired. creating new interpreter...");

 if( (myThreadState = Py_NewInterpreter()) == NULL )
  {
   a_debug_info2(DEBUGLVL3,"a_cleanup_config_file: cannot create python interpreter!");
   PyEval_ReleaseLock();
   return -1;
  }


 a_debug_info2(DEBUGLVL5,"a_cleanup_config_file: running python script...");

 PySys_SetArgv(py_argc, py_argv);
 
 PyFileObject = PyFile_FromString(py_argv[0], "r");
 PyNullFile = PyFile_FromString("/dev/null", "w");

 PySys_SetObject("stderr",PyNullFile); /* redirect standard error to /dev/null */

 if( (PyRun_SimpleFile(PyFile_AsFile(PyFileObject), py_argv[0])) == -1)
  {
   python_run_fail = YES;
   a_debug_info2(DEBUGLVL3,"a_cleanup_config_file: python script [%s] failed!",py_argv[0]);
  }

 a_debug_info2(DEBUGLVL5,"a_cleanup_config_file: script executed. GIL release and exit.");

 Py_DECREF(PyFileObject);
 Py_DECREF(PyNullFile);

 Py_EndInterpreter(myThreadState);

 PyEval_ReleaseLock();

 a_refresh_signals(); /* refresh signals just in case */

 if(python_run_fail)
  {
   a_logmsg("ERROR: python script %s failed to execute properly!",py_argv[0]);
   return -1;
  }

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
  struct stat outfile,helper_script;
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

  if(stat(script_path,&helper_script))
   {
    a_logmsg("FATAL: cannot open %s helper script!",script_path);
    return -1;
   }
  
  if(helper_script.st_size == 0)
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
    return -1; /* no expect log file after expect run - fail. */
   }
  else if(outfile.st_size < MIN_WORKING_COPY_LEN) 
   {
    a_logmsg("%s: expect method: device config file is shorter than minimum expected size (%d bytes)!",device_name,MIN_WORKING_COPY_LEN);
    remove(result_file);
    return -1; /* expect log file empty or a few bytes long - fail. */
   }
  else 
   {
    a_debug_info2(DEBUGLVL5,"a_get_using_expect: re-formatting device config file.");

    if(a_cleanup_config_file(result_file,device_type) == -1) /* re-format output config file */
     {
      a_logmsg("%s: expect method: post-processing of config file failed.",device_name);
      /* post-processing is optional, so archive config anyway */
     }
   }

  return 1;

}

/* end of get_methods.c */
