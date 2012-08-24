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
*    defs.h - various global definitions
*/

#include <pthread.h>

#ifdef USE_MYSQL

#include <my_global.h>
#include <mysql.h>

#endif

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <svn_pools.h>

#define DEBUGLVL1 1
#define DEBUGLVL2 2
#define DEBUGLVL3 3
#define DEBUGLVL4 4
#define DEBUGLVL5 5

#define FAST_DLY 200       /* main program loop delay in microseconds - fast mode */
#define SLOW_DLY 1000000   /* main program loop delay in microseconds - slow mode */
#define MAX_CONF_LINES 256 /* max line count in the config file */

#define NUM_ARCH_THREADS 20     /* concurrent single-device archiver threads */
#define MAX_CONCURRENT_BULK_THREADS 1

#if defined(sun) || defined(__sun)
#define ARCHIVIST_THREAD_STACK_SIZE 1048576 * 4
#else
#define ARCHIVIST_THREAD_STACK_SIZE 1048576 * 2
#endif

/* various public defines */
#define SOCK_RECVLEN 1024
#define HOSTNAME_FIELD_IN_SYSLOG 4
#define MAX_SYSLOG_LINE_LEN 2084
#define BUFLEN 8192
#define CONFIG_MAX_LINELEN 1024
#define IPADDRLEN 40

#ifndef MAXPATH
#define MAXPATH 4096
#endif

#define MAXQUERY 2048           /* maximum length of SQL query */
#define SAFE_STRING_LEN 8192

/* substitute constants referring to possible archivization methods: */

#define ARCHIVE_USING_RANCID 1
#define ARCHIVE_USING_INTERNAL 2

#define nil ((void*)0)

/* global variables needed for thread operation and debugging: */

pthread_mutex_t G_thread_count_mutex;
pthread_mutex_t G_M_thread_count_mutex;
pthread_mutex_t G_router_db_mutex;
pthread_mutex_t G_perl_running_mutex;
pthread_mutex_t G_changelog_write_mutex;
pthread_mutex_t G_SQL_query_mutex;

int G_stop_all_processing;
int G_active_archiver_threads;
int G_active_bulk_archiver_threads;

char G_debug_msg[16384];

FILE *G_logfile_handle;
FILE *G_syslog_file_handle;

int G_syslog_file_size;
int G_router_db_entries;
int G_current_debug_level;
int G_config_dump_memstats;
int G_apr_reset_timer;
int G_mainloop_dly;

apr_pool_t *G_svn_root_pool;
apr_pool_t *G_apr_root_pool;

#ifdef USE_MYSQL

MYSQL *G_db_connection;

#endif

/* prototypes of pthread and signal callable routines: */

extern void a_signal_cleanup(void);

extern void a_archive_single(void *arg);

extern int a_archive_bulk(void *arg);

extern void a_apr_reinit(void);

/* defs.h end  */
