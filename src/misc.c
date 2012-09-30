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
*    misc.c - miscellaneous procedures 
*/

#include <../config.h>

#include <sys/types.h>
#include <sys/stat.h>  
#include <dirent.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <stdio.h>
#include <time.h>

#ifdef HAVE_SYS_PROCFS_H 

#include <sys/procfs.h>

#endif

#if defined(HAVE_LIBSNMP) || defined(HAVE_LIBNETSNMP)

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/library/snmp_logging.h>

#endif

#ifdef HAVE_SYS_TIME_H

#include <sys/time.h>

#endif

#include <sys/syslog.h>
#include <regex.h>

#include "defs.h"
#include "archivist_config.h"


void a_debug_info2
(int message_debug_level, char *debug_message, ...)
/*
*
* print debugging information to stderr
*
*/
{

  #define MAX_DEBUG_MSG_LEN 2048

  va_list ap;
  char message_cr[MAX_DEBUG_MSG_LEN];

  if(message_debug_level <= G_current_debug_level)
    {
     snprintf(message_cr,MAX_DEBUG_MSG_LEN,"%s\n",debug_message);
     va_start(ap, message_cr);
     vfprintf(stderr, message_cr, ap);
     va_end(ap);
    }
  
}



int a_filesize
(int file_descriptor)
/*
*
* return size of a file 
*
*/
{
   struct stat stats;
   if (fstat(file_descriptor, &stats) < 0) 
    {
     a_debug_info2(DEBUGLVL3,"filesize error - failed to get file size"); 
     return -1;
    }
   return stats.st_size;
}


ssize_t a_safe_read
(int fd, void *buf, size_t count)
/*
*
* read from file descriptor
*
*/
{
     ssize_t n;
     do 
      {n = read(fd, buf, count);} 
     while (n < 0 && errno == EINTR);
     return n;
}

void a_logmsg
(char *fmt, ...)
/*
*
* log system event message: 
* to a logfile (descriptor in a public variable G_logfile_handle)
* or 
* to MYSQL database.
*
*/
{

  #define MAX_LOG_MSG_LEN 2048

  va_list ap;
  char timestr[26]; /* 26 bytes according to the opengroup docs */
  char message[MAX_LOG_MSG_LEN];
  int total_msg_len;

  if(G_logfile_handle > 0) /* logfile handle open and ready to write */
   {

    struct timeval tv;
    struct timezone tz;
    struct tm tm;

    gettimeofday(&tv, &tz);
    localtime_r(&tv.tv_sec,&tm);
    asctime_r(&tm,&timestr);
    timestr[strlen(timestr)-1] = 0x0;   /* get rid of newline at the end of asctime result */

    total_msg_len = strlen(timestr) + strlen(fmt) + 10;

    snprintf(message,MAX_LOG_MSG_LEN,"%s %s\n",timestr,fmt);

    va_start(ap, message);

#ifndef USE_MYSQL

    vfprintf(G_logfile_handle, message, ap);
    fflush(G_logfile_handle);

#else

    char formatted_log_msg[MAX_LOG_MSG_LEN];
    char escaped_formatted_log_msg[MAX_LOG_MSG_LEN+MAX_LOG_MSG_LEN+2]; /* length according to MYSQL API */
    char sql_query[MAX_LOG_MSG_LEN+1024];

    sql_query[0] = 0x0;
    formatted_log_msg[0] = 0x0;

    vsnprintf(formatted_log_msg,MAX_LOG_MSG_LEN,message,ap);  /* format log message */

    mysql_real_escape_string(G_db_connection,escaped_formatted_log_msg,formatted_log_msg,
                             strlen(formatted_log_msg));

    snprintf(sql_query,MAX_LOG_MSG_LEN+1024,"insert into eventlog values (now(),'INFO','','%s')",
             escaped_formatted_log_msg);

    a_mysql_update(sql_query); 

#endif

    va_end(ap);

   }
}



int a_remove_directory
(const char *path)
/*
* rm -r implemented in c
* code taken from:
* http://stackoverflow.com/questions/2256945/removing-a-non-empty-directory-programmatically-in-c-or-c
*/
{
   DIR *d = opendir(path);
   size_t path_len = strlen(path);
   int r = -1;

   if (d)
   {
      struct dirent *p;
      r = 0;
      while (!r && (p = readdir(d)))
      {
          int r2 = -1;
          char *buf;
          size_t len;

          if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
          { continue; }

          len = path_len + strlen(p->d_name) + 2; 
          buf = malloc(len);

          if (buf)
          {
            struct stat statbuf;
            snprintf(buf, len, "%s/%s", path, p->d_name);
            if (!stat(buf, &statbuf))
             { 
                if (S_ISDIR(statbuf.st_mode))
                  r2 = a_remove_directory(buf); 
                else
                 { 
                  if(r2 = unlink(buf))
                   { 
                    a_debug_info2(DEBUGLVL3,"a_remove_directory: cannot unlink %s!",buf); 
                    a_logmsg("ERROR: cannot unlink file %s!",buf);
                   }
                 }
             }
            free(buf);
          }
          r = r2;
      }
      closedir(d);
   }

   if (!r)
   { 
    if(r = rmdir(path)) 
     {
      a_debug_info2(DEBUGLVL3,"a_remove_directory: cannot rmdir %s!",path);
      a_logmsg("ERROR: cannot remove directory %s!",path);
     } 
   }
   return r;

}


void a_cleanup_and_exit
(void)
/*
* function responsible for cleanup after catching a termination signal 
*/
{

  int timer = 0;

  if( (G_active_archiver_threads > 0) || (G_active_bulk_archiver_threads > 0) )  /* we catched the TERM signal during ongoing archiving process */
   {
     a_logmsg("Catched SIGTERM - waiting 10 sec. for started processing to end.");
     G_stop_all_processing = 1;    /* signal key routines that they should stop furhter processing and exit (or go idle) */

     while( ( (G_active_archiver_threads > 0) || (G_active_bulk_archiver_threads > 0) ) && (timer < 10) ) /* wait approx. 30 sec. */
      {
        a_debug_info2(DEBUGLVL5,
                      "a_cleanup_and_exit: waiting %ds for active threads to finish before exit...",
                      (10 - timer));
        sleep(1);   /* wait for all processing to end */
        timer += 1;
      }

   }

   a_debug_info2(DEBUGLVL3,"a_cleanup_and_exit: SIGTERM handler called - cleaning up and exiting...");
   a_logmsg("Archivist stopping at SIGTERM.");

   a_remove_lockfile();

   fclose(G_logfile_handle);
   close(G_syslog_file_handle);

#ifdef USE_MYSQL
   mysql_close(G_db_connection);
#endif

   apr_pool_destroy(G_apr_root_pool);
   svn_pool_destroy(G_svn_root_pool);

   PyEval_RestoreThread(G_py_main_thread_state);

   Py_Exit(0);

   exit(0);

}


#ifndef USE_MYSQL

void a_sighup_handler
(void)
/*
* function responsible for servicing SIGHUP sent to archivist.
* basically it'll cause re-reading of device database (router.db).
* for re-reading config file, you'll have to restart the daemon.
*/
{

   pthread_mutex_lock(&G_router_db_mutex);
   a_router_db_free(G_router_db);
   G_router_db = a_load_router_db(G_config_info.router_db_path);
   pthread_mutex_unlock(&G_router_db_mutex); 

   a_logmsg("SIGHUP: Re-readed device database file (%d entries now).", G_router_db_entries);

}

#endif

int a_remove_lockfile
(void)
 {
  char lock_file_name[MAXPATH];

  snprintf(lock_file_name,MAXPATH,"%s/archivist.%d.lock",
           G_config_info.locks_dir,G_config_info.instance_id);

  if(unlink(lock_file_name)) 
   return 1;
  else
   return 0;
 }

int a_create_lockfile
(void)
 {
  char lock_file_name[MAXPATH];

  snprintf(lock_file_name,MAXPATH,"%s/archivist.%d.lock",
           G_config_info.locks_dir,G_config_info.instance_id);

  if(a_ftouch(lock_file_name)) 
   return 1; 
  else 
   return 0;
 }

int a_check_for_lockfile
(void)
 {
   struct stat stats;
   char lock_file_name[MAXPATH];

   snprintf(lock_file_name,MAXPATH,"%s/archivist.%d.lock",
            G_config_info.locks_dir,G_config_info.instance_id);

   if (stat(lock_file_name, &stats) == 0) 
    return 1;
   else
    return 0;
 }


int a_is_valid_ip(const char *ip_str)
/*
*
* check if string is a valid IP address
*
*/
{

 if(a_regexp_match(ip_str,"^[1-9]{1}[0-9]{0,2}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}$"))
  return 1;
 return 0;

}


void a_tolower_str
(char *string)
/*
* convert a string to lowercase
*/
{
   char *p;
   for (p = string; *p != '\0'; p++)
   *p = (char)tolower(*p);
}

int a_rename
(char *src, char *dst)
/* 
* rename a file (even across the filesystems). Ugly version using system() call.
*/
{
 char command[MAXPATH];

 bzero(command,MAXPATH);
 snprintf(command,MAXPATH,"/bin/mv %s %s",src,dst); 
 if((a_our_system(command)) == -1)
   return -1;
 else
   return 1; 
}


int a_ftouch
(const char *path)
/*
* create an empty file
*/
{
  int fd = -1;
  int open_errno = 0;

  /* Try to open FILE, creating it if necessary.  */
  fd = open (path, O_WRONLY | O_CREAT | O_NONBLOCK | O_NOCTTY,
             S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  if (fd == -1)
   open_errno = errno;

  if (fd != -1 && (close (fd)) == 0 )
      return 1;

  return 0;
}

void a_refresh_signals
(void)
/*
* (re)set the signals to our handlers
*
*/
{

   struct sigaction sa_g1,sa_g2,sa_g3;

   sa_g1.sa_handler = (void *)a_cleanup_and_exit;
   sigemptyset(&sa_g1.sa_mask);
   sa_g1.sa_flags = SA_RESTART;

   sigaction(SIGINT, &sa_g1, NULL);
   sigaction(SIGTERM, &sa_g1, NULL);

   sa_g2.sa_handler = (void *)a_apr_reinit;
   sigemptyset(&sa_g2.sa_mask);
   sa_g2.sa_flags = SA_RESTART;

   sigaction(SIGUSR1, &sa_g2, NULL);


#ifndef USE_MYSQL

   sa_g3.sa_handler = (void *)a_sighup_handler;
   sigemptyset(&sa_g3.sa_mask);
   sa_g3.sa_flags = SA_RESTART;

   sigaction(SIGHUP, &sa_g3, NULL);

#endif

}



int a_our_system
(const char *cmd)
/*
* taken from opengroup.org, and modified for MT-safety.
* http://pubs.opengroup.org/onlinepubs/009695399/functions/system.html
*
* notes about system(), exec(), fork() and thread-safety:
*
* it is said, that fork() - when used in threads - produces unpredictable results. 
* normally, when forking, entire program that calls fork() is cloned in the memory, and this copy 
* is executed from the point where fork() was called. in case of a multithreaded program, usually 
* only thread calling fork() is cloned, which is probably not what one would want when using 
* fork() to do some further processing in the forked code. 
* according to various sources, only reasonable use of fork() in threads, is to immediately call exec() 
* after fork() - that is to replace forked, "orphaned" single thread, with a new program - which 
* is exactly what we are doing here, so essentially - regarding thread safety - this function 
* _should_ be MT-safe...
* 
*/
{
    int stat;
    pid_t pid;
    struct sigaction sa, savintr, savequit;
    sigset_t saveblock;
    if (cmd == NULL)
        return(1);
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);       
    sa.sa_flags = 0;
    sigemptyset(&savintr.sa_mask);
    sigemptyset(&savequit.sa_mask);
    sigaction(SIGINT, &sa, &savintr);      /* save SIGINT action, and temporarily set it to IGNORE */
    sigaction(SIGQUIT, &sa, &savequit);    /* savve SIGQUIT action, and temporarily set it to IGNORE */
    sigaddset(&sa.sa_mask, SIGCHLD);
    pthread_sigmask(SIG_BLOCK, &sa.sa_mask, &saveblock);
    if ((pid = fork()) == 0) {                                  /* child will execute this  - restore signal handlers and execl */
        sigaction(SIGINT, &savintr, (struct sigaction *)0);     
        sigaction(SIGQUIT, &savequit, (struct sigaction *)0);
        sigprocmask(SIG_SETMASK, &saveblock, (sigset_t *)0);
        execl("/bin/sh", "sh", "-c", cmd, (char *)0);           
        _exit(127);
    }
    if (pid == -1) {
        stat = -1; /* errno comes from fork() */
    } else {                                                   /* parent will execute this - wait for a child to terminate */
        while (waitpid(pid, &stat, 0) == -1) {
            if (errno != EINTR){
                stat = -1;
                break;
            }
        }
    }
    sigaction(SIGINT, &savintr, (struct sigaction *)0);             /* restore SIGINT/SIGQUIT signal handlers in parent thread */
    sigaction(SIGQUIT, &savequit, (struct sigaction *)0);
    pthread_sigmask(SIG_SETMASK, &saveblock, (sigset_t *)0);
    return(stat);
}


void a_init_globals
(void)
/*
* init global variables - called only once during a daemon statrup
*
*/
{
   G_logfile_handle = 0;
   G_syslog_file_handle = 0;
   G_active_archiver_threads = 0;
   G_active_bulk_archiver_threads = 0;
   G_syslog_file_size = 0;
   G_apr_reset_timer = 0;
   G_stop_all_processing = 0;
   G_config_dump_memstats = 0;
   G_auth_set_list = NULL;
   G_config_regexp_list = NULL;

   pthread_mutex_init(&G_thread_count_mutex, NULL);
   pthread_mutex_init(&G_M_thread_count_mutex, NULL);
   pthread_mutex_init(&G_embedded_running_mutex, NULL);
   pthread_mutex_init(&G_changelog_write_mutex, NULL);
   pthread_mutex_init(&G_SQL_query_mutex, NULL);
 
   apr_initialize(); /* initialize APR data structures - once */

   G_svn_root_pool = svn_pool_create(NULL);
   apr_pool_create(&G_apr_root_pool,NULL);


   /* turn off SNMP logging */
   /* following piece of code comes from:                                            */
   /* http://www.mail-archive.com/net-snmp-users@lists.sourceforge.net/msg28517.html */

   snmp_disable_log();

   netsnmp_log_handler *logh_head = get_logh_head();
   while(NULL != logh_head)
     netsnmp_remove_loghandler( logh_head );
   netsnmp_log_handler *logh = netsnmp_register_loghandler(NETSNMP_LOGHANDLER_NONE, LOG_DEBUG);
   if (logh)
    logh->pri_max = LOG_EMERG; 

   init_snmp("archivist_snmp");
   
   /* initalize python interpreter */

   Py_Initialize();
   PyEval_InitThreads();

   G_py_main_thread_state = PyThreadState_Get();

   PyEval_ReleaseLock();

}

void a_showversion
(void)
/*
* function name says it all
*
*/
{
   printf("Archivist version %s\n",VERSION);
   #ifdef USE_MYSQL
   printf("\(MYSQL support compiled in\)\n");
   #endif
}

void a_showhelp
(void)
/*
* function name says it all
*
*/
{
   a_showversion();
   printf("Usage: ");
#ifdef USE_MYSQL
   printf("archivist [ -v ] [ -h ] [ -d ] [ -i ] [ -m ] [ -c configfile ]\n");
#else
   printf("archivist [ -v ] [ -h ] [ -d ] [ -m ] [ -c configfile ]\n");
#endif
}

char *a_config_regexp_match
(char *syslog_buffer)
/*
* search syslog buffer for a patterns specified in ConfigRegex entries
*/
{
  config_regexp_t *workptr;

  if(syslog_buffer == NULL) return NULL;

  for(workptr = G_config_regexp_list;workptr!=NULL;workptr=workptr->prev)
   {
    if(a_regexp_match(syslog_buffer,workptr->config_regexp_string))
     {
      return workptr->username_field_token;
     }
   }
  return NULL;
}


int a_regexp_match
(const char *string, char *pattern)
/*
* regexp matching routine
* from: http://pubs.opengroup.org/onlinepubs/009695399/functions/regcomp.html
*/
{
    int status;
    regex_t re;

    if(regcomp(&re, pattern, REG_EXTENDED|REG_NOSUB) != 0) 
     {
      a_debug_info2(DEBUGLVL3,"a_regexp_match: Regular expression compilation error: [%s]!",pattern);
      return(0);      
     }

    status = regexec(&re, string, (size_t) 0, NULL, 0);
    regfree(&re);
    
    if (status == 1) 
     return 0;

    if (status > 1) 
     {
      a_debug_info2(DEBUGLVL3,"a_regexp_match: regexec failed! [%d,%s,%s]!",status,pattern,string);
      return 0;      
     }

    return 1;

 }


char *a_trimwhitespace
(char *str)
/*
*
* from: http://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
*
*/
{
  char *end;

  /* Trim leading space */
  while(isspace(*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  /* Trim trailing space */
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;

  /* Write new null terminator */
  *(end+1) = 0;

  return str;
}

void a_remove_quotes
(char *string)
/*
*
* remove heading and trailing double or single quotes from a string
*
*/
{
 int i,len;

 #define SINGLE_QUOTE_ASCII 39
 #define DOUBLE_QUOTE_ASCII 34

 len = strlen(string);

 if(len == 0) return;

 if( (string[0] == SINGLE_QUOTE_ASCII) || (string[0] == DOUBLE_QUOTE_ASCII))
  {
   for(i = 0; i <= len; i++)
     string[i] = string [i+1];
   string[len] = 0;
   len--;
  }

 if( (string[len - 1] == SINGLE_QUOTE_ASCII) || (string[len - 1] == DOUBLE_QUOTE_ASCII) )
  string[len - 1] = 0;
}



void a_disp_thread_stacksize
(void)
/*
*
* display OS default thread stack size (for debugging purposes)
*
*/
{
   int thread_stacksize;
   pthread_attr_t thread_attr;

   pthread_attr_init(&thread_attr);
   pthread_attr_getstacksize(&thread_attr, &thread_stacksize);

   #if defined(sun) || defined(__sun)
   if(thread_stacksize == 0)
     a_debug_info2(DEBUGLVL5,
        "a_disp_thread_stacksize: default thread stack size on this system: system-default");
   else
      a_debug_info2(DEBUGLVL5,
      "a_disp_thread_stacksize: default thread stack size on this system: %dK",
            thread_stacksize/1024);

   #else
   a_debug_info2(DEBUGLVL5,
      "a_disp_thread_stacksize: default thread stack size on this system: %dK",
            thread_stacksize/1024);
   #endif

}

void a_apr_reinit
(void)
/*
*
* reinitialize APR data structures. user triggerable, hooked up to SIGUSR1.
*
*/
{
 if( (G_active_archiver_threads == 0) &&
     (G_active_bulk_archiver_threads == 0) )
  {
   a_debug_info2(DEBUGLVL5,"a_apr_reinit: catched SIGUSR1: reinitializing APR data structures ");

   apr_pool_destroy(G_apr_root_pool);
   svn_pool_destroy(G_svn_root_pool);

   apr_terminate();
   usleep(100);
   apr_initialize();

   G_svn_root_pool = svn_pool_create(NULL);
   apr_pool_create(&G_apr_root_pool,NULL);
   
  }
}

int a_add_changelog_entry
(char *diff_filename, int diff_filesize, char *device_name, char *configured_by)
/*
*
* add an entry to a device config changelog file
*
*/
{
   char *diff_buffer;
   char asctime_str[26]; /* 26 bytes according to the opengroup docs */
   char header[512], footer[512];
   int wrote = 0;
   size_t readed = 0;
   time_t t;
   struct tm tstruct;
   FILE *svn_diff_file;
   FILE *diff_logfile;
   int fail = 0;

   header[0] = 0;
   footer[0] = 0;

   a_debug_info2(DEBUGLVL5,"a_add_changelog_entry: %s: size of the changelog entry to write: %d",device_name, diff_filesize);

   diff_buffer = malloc(diff_filesize);

   if( (svn_diff_file = fopen(diff_filename, "rb")) == NULL)
     {
      a_debug_info2(DEBUGLVL5,"a_add_changelog_entry: %s: cannot open SVN diff file %s (%d)!",device_name, diff_filename, errno);
      free(diff_buffer);
      return 0;
     }

   if( (readed = fread(diff_buffer, diff_filesize, 1, svn_diff_file)) == 0 )
     {
      a_debug_info2(DEBUGLVL5,"a_add_changelog_entry: %s: cannot read in SVN diff file %s (%d)!",device_name, diff_filename, errno);
      free(diff_buffer);
      return 0;
     }

   fclose(svn_diff_file);

   remove(diff_filename);

   a_debug_info2(DEBUGLVL5,"a_add_changelog_entry: %s: writing %d bytes diff to a changelog file.",
                 device_name, diff_filesize);

   pthread_mutex_lock(&G_changelog_write_mutex);

   if((diff_logfile = fopen(G_config_info.changelog_filename,"a+")) == NULL)
     {
      a_debug_info2(DEBUGLVL5,"a_svn_diff: ERROR: cannot open diff log file (%d)!",errno);
      free(diff_buffer);
      pthread_mutex_unlock(&G_changelog_write_mutex);
      return 0;
     }

   t = time(0);
   localtime_r(&t,&tstruct);
   asctime_r(&tstruct,&asctime_str);

   snprintf(header,512,
            "\n====== following config changes made by %s at: %s\n",configured_by,asctime_str);
   snprintf(footer,512,
            "\n====== change end ======\n");

   fwrite(header,strlen(header),1,diff_logfile);
   wrote = fwrite(diff_buffer,diff_filesize,1,diff_logfile);
   fwrite(footer,strlen(footer),1,diff_logfile);   

   if(wrote < 1)
      {
       a_logmsg("ERROR: cannot write to diff file (%d)!",errno);  
       free(diff_buffer);
       pthread_mutex_unlock(&G_changelog_write_mutex);
       return 0;
      }

   fclose(diff_logfile);

   pthread_mutex_unlock(&G_changelog_write_mutex);

   free(diff_buffer);

   return 1;


}


/*
*
* case insensitive strstr
* from: http://www.daniweb.com/software-development/c/code/216564
*
*/

const char *a_mystristr(const char *haystack, const char *needle)
{
 if ( !*needle )
  {
   return haystack;
  }

 for ( ; *haystack; ++haystack )
  {
    if ( toupper(*haystack) == toupper(*needle) )
     {
       /*
        * Matched starting char -- loop through remaining chars.
        */
       const char *h, *n;
       for ( h = haystack, n = needle; *h && *n; ++h, ++n )
        {
         if ( toupper(*h) != toupper(*n) )
          {
            break;
          }
        }
        if ( !*n ) /* matched all of 'needle' to null termination */
        {
          return haystack; /* return the start of the match */
        }
      }
   }

   return 0;
}

void a_config_error(char *config_field)
/*
*
* print config error message and exit.
*
*/
{
 printf("FATAL: data from configuration field \"%s\" is unreadable!\n(too long, too short, wrong value or nonexistent).\n",config_field);
 a_cleanup_and_exit();
}


void a_dump_memstats(void)
/*
*
* dump memory stats to log file - leak detection helper.
* this must be operating system dependent hack, as POSIX does not offer 
* anything more than getrusage(), wich is usually not fully implemented (linux, solaris).
*
*/
{

 #ifdef LINUX
  a_dump_memstats_linux();
 #endif

 #ifdef SOLARIS2
 #ifdef HAVE_SYS_PROCFS_H
  a_dump_memstats_solaris();
 #endif
 #endif

 /* other system-dependent methods have yet to be implemented */

}


#ifdef SOLARIS2
#ifdef HAVE_SYS_PROCFS_H

void a_dump_memstats_solaris(void)
/*
*
* dump memory stats - solaris version
*
*/
{

 char proc_path[MAXPATH];
 int fd;
 prpsinfo_t ps_info;

 sprintf(proc_path, "/proc/self");
 fd = open(proc_path, O_RDONLY);
 ioctl(fd, PIOCPSINFO, &ps_info);

 a_logmsg("memstats: total: %u, rss: %u", ps_info.pr_bysize, ps_info.pr_byrssize);

 close(fd);

}

#endif
#endif

void a_dump_memstats_linux(void)
/*
*
* dump memory stats - linux version
*
*/
{

 char proc_path[MAXPATH];

 snprintf(proc_path, 30, "/proc/self/statm");

 FILE* fd = fopen(proc_path, "r");

 unsigned size;     //  total program size
 unsigned resident; //  resident set size
 unsigned share;    //  shared pages
 unsigned text;     //  text (code)
 unsigned lib;      //  library
 unsigned data;     //  data/stack
 unsigned dt;       //  dirty pages (unused in Linux 2.6)


 if (fd) {
    fscanf(fd, "%u %u %u %u %u %u", &size, &resident, &share, &text, &lib, &data);
  }

 fclose(fd);

 a_logmsg("memstats: shared: %u, text: %u, data: %u, total: %u", share, text, data, size);

}

/* end of misc.c */

