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
*    main.c - main loop
*
*    internally defined functions have names begginig with a_
*    global variables have names beginning with G_
*
*    in general - for thread safety - most global variables are read only, 
*    or mutex-locked writable.
*
*/

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "archivist_config.h"

main
(int argc, char **argv)
{

   char *raw_config[MAX_CONF_LINES];
   char syslog_msgbuffer[BUFLEN];
   char ip_from[IPADDRLEN];
   int syslog_socket,pid,c;

   a_init_globals(); /* init global variables, mutexes, and other one-time stuff  */

   opterr = 0;

   while ((c = getopt (argc, argv, "mivhdc:")) != -1)
     switch (c)
       {
        case 'd':
          G_current_debug_level = 5;
          break;
        case 'm':
          G_config_dump_memstats = YES;
          break;
        case 'c':
          if(strlen(optarg) > 0)
           strncpy(G_config_filename,optarg,MAXPATH);
          break;
        case 'h':
          a_showhelp();
          exit(-1);
          break;
        case 'v':
          a_showversion();
          exit(0);
          break;
#ifdef USE_MYSQL
        case 'i':
          a_mysql_init_database();
          exit(0);
          break;
#endif
        case '?':
          if (optopt == 'c')
           {
            fprintf (stderr, "ERROR: Option -%c requires an argument.\n", optopt); 
            a_showhelp();
           }
          else if (isprint (optopt))
           {
            fprintf (stderr, "ERROR: Unknown option `-%c'.\n", optopt);
            a_showhelp();
           }
          else
           {
            fprintf (stderr,"ERROR: Unknown option character `\\x%x'.\n",
                     optopt);
            a_showhelp();
           }
        }

   if(optind < argc) 
    { 
     printf("ERROR: invalid non-option arguments.\n"); 
     a_showhelp(); 
    }

   a_debug_info2(DEBUGLVL1,"main: Archivist starting...");

   if(G_current_debug_level>0)
    a_disp_thread_stacksize();    /* if DEBUG - display OS default thread stack size */

   if(G_current_debug_level == 0) /* if we are debugging - stay on the terminal - don't fork. */
    {
     pid = fork();                /* we aren't debugging - fork */
       if (pid < 0)
        {
         /* Could not fork  */
         exit(EXIT_FAILURE);
        }
       if (pid > 0)
        {
         /* Child created ok, so exit parent process  */
         exit(EXIT_SUCCESS);
        }
    }

   a_refresh_signals();                                 /* install signal handlers  */
   
   a_load_config_defaults(&G_config_info);              /* load and parse config file  */

   if(!a_read_config_file(G_config_filename,raw_config))
    {
     printf("ERROR: cannot read program configuration - exit.\n");
     a_cleanup_and_exit();
    }

   a_parse_config_info(raw_config,&G_config_info);      /* (DB connection is done here) */

   if(a_check_for_lockfile())                           /* lockfile check & create */
    { 
     printf("\nERROR: archivist lockfile for instance %d exists!",G_config_info.instance_id); 
     printf("\nThere may be another archivist process running!\n");
     printf("If you are sure that this isn't the case - remove lockfile and try again...\n\n");
     exit(-1);
    }
   else if(!a_create_lockfile()) 
    {
     printf("ERROR: cannot create lock file!\n");
     exit(-1);
    }
     
   if((chdir(G_config_info.working_dir) != 0))
    {
     printf("FATAL: cannot change directory to working directory %s. exit.\n",G_config_info.working_dir);
     a_cleanup_and_exit();
    }

   if(a_svn_configured_repo_stat() != 1)
    {
     printf("\nFATAL: configured SVN repository (%s) is not accessible!\n",G_config_info.repository_path);
     a_cleanup_and_exit();
    }

   if( (G_config_info.logging) && (strlen(G_config_info.log_filename) > 0) )
    G_logfile_handle = fopen(G_config_info.log_filename,"a+");

   G_router_db = a_load_router_db(G_config_info.router_db_path); /* load device list from router.db file */

   /* on startup, log config information to the logfile: */

#ifndef USE_MYSQL
   a_logmsg("Archivist - network device config archiver daemon starting (instance ID: %d).",
            G_config_info.instance_id);
 
   a_logmsg("--> router.db file: %s (%d entries)",G_config_info.router_db_path,G_router_db_entries);

   /*if above SVN test passed - we assume that SVN is accessible - OK:*/
   a_logmsg("--> SVN repository path: %s (OK)",G_config_info.repository_path); 

   if(G_config_info.logging)
    a_logmsg("--> logging events to %s",G_config_info.log_filename);
   if(G_config_info.tail_syslog)
    a_logmsg("--> tailing syslog file %s",G_config_info.syslog_filename);
   if(G_config_info.listen_syslog)
    a_logmsg("--> listening to syslog messages on port %d",G_config_info.syslog_port);
   if(G_config_info.keep_changelog)
    a_logmsg("--> logging config diffs to %s",G_config_info.changelog_filename);
   if(G_config_dump_memstats)
    a_logmsg("--> dumping memory statistics to event log every 24h");
#else
   a_logmsg("Archivist starting.");
#endif

   /* setup sockets and file descriptors to listen on (if configured to do so): */

   if(G_config_info.listen_syslog)
    syslog_socket = a_syslog_socket_setup();

   if(G_config_info.tail_syslog)
    G_syslog_file_handle = a_syslog_fstream_setup();

   /* choose main loop delay value */

   if(!G_config_info.listen_syslog && !G_config_info.tail_syslog)
    G_mainloop_dly = SLOW_DLY;  /* no need to waste system resources when we dont track any datastreams */
   else
    G_mainloop_dly = FAST_DLY;  /* when tracking some datastreams, we need to poll descriptors frequently */

   a_debug_info2(DEBUGLVL1,"main: entering main loop...");

   /* main program: */

   while(TRUE)
    {
     if(G_config_info.listen_syslog)
      if(a_check_syslog_stream(syslog_socket,syslog_msgbuffer,ip_from))
        a_parse_syslog_buffer(syslog_msgbuffer,ip_from);

     if(G_config_info.tail_syslog)
      if(a_check_taillog_stream(G_syslog_file_handle,syslog_msgbuffer,NULL)) 
        a_parse_syslog_buffer(syslog_msgbuffer,NULL); 

     a_check_and_run_jobs();         /* execute cron-like job manager */

     usleep(G_mainloop_dly);
    }

}


/* end of main.c  */

