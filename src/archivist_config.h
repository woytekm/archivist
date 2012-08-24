/*
*   This file is part of Archivist - network device config archiver.
*
*   Archivist is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   Archivist is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with Archivist.  If not, see <http://www.gnu.org/licenses/>.
* 
*   Author: Wojtek Mitus (woytekm@gmail.com)
*
*   archivist_config.h - configuration data and structures 
*/

#define YES 1
#define NO 0

#define DEFAULT_CONF_INSTANCE_ID 1
#define DEFAULT_CONF_LOGGING 1
#define DEFAULT_CONF_LOGFILENAME "/usr/local/var/archivist.log"
#define DEFAULT_CONF_WORKING_DIR "/usr/local/tmp"
#define DEFAULT_CONF_TAIL_SYSLOG 0
#define DEFAULT_CONF_LISTEN_SYSLOG 0
#define DEFAULT_CONF_CHANGELOG 0
#define DEFAULT_CONF_SYSLOG_PORT 514
#define DEFAULT_CONF_SYSLOG_FILENAME "/var/log/messages"
#define DEFAULT_CONF_RANCID_PATH "/usr/local/rancid/bin/rancid"
#define DEFAULT_CONF_SCRIPT_DIR "/usr/local/share/archivist/helpers"
#define DEFAULT_CONF_LOCKS_DIR "/var/run"
#define DEFAULT_CONF_EXPECT_PATH "expect"    /* we assume that expect should be somewhere in the path */
#define DEFAULT_CONF_CHANGELOG_FILENAME "config_changelog.log"
#define DEFAULT_CONF_SQL_DBNAME "archivist"

#define ARCHIVIST_MYSQL_USER "root"
#define ARCHIVIST_MYSQL_PASSWORD "mysecret" 
#define ARCHIVIST_MYSQL_SERVER "127.0.0.1"

#ifndef MAXPATH
#define MAXPATH 4096
#endif

#define MAX_JOBS 128               /* maximum number of cronjob-scheduled archiver jobs  */

#define MIN_WORKING_COPY_LEN	200  /* suspicious downloaded config length - truncated? */

static char G_config_filename[MAXPATH]="/usr/local/etc/archivist.conf";   /* default config filename  */

static char G_svn_tmp_prefix[20]=".svn_tmp";   /* prefix for name of svn tmp directories */

/* structure holding archivist configuration data */

struct config_info_t{ int  instance_id;		/* Archivist instance ID  */
                      char working_dir[MAXPATH];    /* where should we store temp files/directories? */
                      char locks_dir[MAXPATH];      /* where should we create lock files? */
                      char logging;             /* should we enable logging? */
                      char log_filename[MAXPATH];   /* name of the daemon log file */
                      char archiving_method;    /* how to archive config from the devices */
                      char tftp_dir[MAXPATH];       /* location of TFTP directory (for SNMP-TFTP method) */
                      char tftp_ip[16];         /* IP address of TFTP server used in SNMP-TFTP method */
                      char script_dir[MAXPATH];       /* location of internal expect scripts directory */
                      char rancid_exec_path[MAXPATH]; /* if we are using rancid to get config - where is it? */
                      char expect_exec_path[MAXPATH]; /* if we are using exepct to get config - where is it? */
                      char tail_syslog;         /* whether to tail some syslog file in search of CONFIG msgs */
                      char syslog_filename[MAXPATH]; /* name of the syslog file to tail */
                      char listen_syslog;        /* wheter to listen to UDP syslog in search of CONFIG msgs */
                      int  syslog_port;          /* syslog port to listen on */
                      int  keep_changelog;       /* log every diff to a changelog file */
                      char changelog_filename[MAXPATH]; /* changelog filename */
                      char *router_db_path;         /* location of main device database - required */
                      char *repository_path;        /* URL of the SVN repository - required */
                      int  archiver_threads;	    /* number of concurent archiver threads */
                      struct cronjob_t *job_table[MAX_JOBS]; /* table of scheduled backup jobs */
#ifdef USE_MYSQL
		      char mysql_server[255];	/* mysql server name */
		      char mysql_user[255];         /* mysql connection username */
		      char mysql_password[255];	/* mysql connection password */
		      char mysql_dbname[255];	/* mysql database name */
#endif
                  };


/* structure holding one line of data from router.db - a single device information */

typedef struct{ char *group;
                char *hostname;
                char *hosttype;
                char *authset;
                char *arch_method;
                int archived_now;
                void *prev;
               }router_db_entry_t;

/* authentication set consists of login name and a two passwords (secon one optional - for enable). */

typedef struct{ char *set_name;
		char *login;
		char *password1;
		char *password2;
                void *prev;
	      }auth_set_t;


/* authentication set consists of login name and a two passwords (secon one optional - for enable). */

typedef struct{ char *config_regexp_string;
                char *username_field_token;
                void *prev;
              }config_regexp_t;


/* structure defining single cron job data */

struct cronjob_t{ char *min; 	        /* minute */
	          char *hour;	        /* hour */
	          char *dmnth;	        /* day of month */
	          char *month;	        /* month */
	          char *dweek;	        /* day of week */
	          char command[255];	/* job command (all|[hostname]) */
                  char running;           /* running job indicator */
                  int job_id;             /* job ID */
                 };

/* structure for loading device config into a dynamic list */

typedef struct { char *conf_line;
                 struct confline_t *next;
                }confline_t;

/* structure holding information about a configuration event */

typedef struct { char configured_from[255];
                 char configured_by[255];
                 char configured_on[255];
                 char device_id[255];
               }config_event_info_t;

/* declarations of public data structures */

struct config_info_t G_config_info;

struct router_db_entry_t *G_router_db;

struct auth_set_t *G_auth_set_list;

struct config_regexp_t *G_config_regexp_list;


/* end of archivist_config.h */

