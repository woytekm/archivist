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
*    mysql.c - MYSQL database access routines
*/

#ifdef USE_MYSQL

#include "defs.h"
#include "archivist_config.h"

int a_mysql_connect
(void)
/*
* Connect to the MYSQL database. Initialize G_db_connection descriptor.
*/
{
  G_db_connection = mysql_init(NULL);

  if(G_db_connection == NULL) 
   {
    a_debug_info2(DEBUGLVL3,"a_mysql_connect: MYSQL connection error %u: %s\n", 
                  mysql_errno(G_db_connection), mysql_error(G_db_connection));
    printf("FATAL: MYSQL connection error! Turn on debug (-d) for details.\n");
    a_cleanup_and_exit();
   }

  if(mysql_real_connect(G_db_connection, G_config_info.mysql_server, G_config_info.mysql_user, 
                        G_config_info.mysql_password, NULL, 0, NULL, 0) == NULL) 
   {
    a_debug_info2(DEBUGLVL3,"a_mysql_connect: MYSQL connection error %u: %s\n", 
                  mysql_errno(G_db_connection), mysql_error(G_db_connection));
    printf("FATAL: MYSQL connection error! Turn on debug (-d) for details.\n");
    a_cleanup_and_exit();
   }

  if(mysql_select_db(G_db_connection,G_config_info.mysql_dbname))
   {
    a_debug_info2(DEBUGLVL3,"a_mysql_connect: cannot select configured MYSQL database!");
    printf("FATAL: Cannot select configured database!\n");
    a_cleanup_and_exit();
   }

  return 1;
}

MYSQL_RES *a_mysql_select
(char *sql_command)
/*
* Execute MYSQL SELECT statement. Return a pointer to the data structure.
*/
{
  MYSQL_RES *sql_data;

  pthread_mutex_lock(&G_SQL_query_mutex);

  if(mysql_query(G_db_connection, sql_command)) 
   {
    a_debug_info2(DEBUGLVL3,"a_mysql_select: MYSQL error %u: %s\n", 
                  mysql_errno(G_db_connection), mysql_error(G_db_connection));
    pthread_mutex_unlock(&G_SQL_query_mutex);
    return NULL;
   }

  if( (sql_data =  mysql_store_result(G_db_connection)) == NULL)
   {
    a_debug_info2(DEBUGLVL3,"a_mysql_select: MYSQL error %u: %s\n", 
                 mysql_errno(G_db_connection), mysql_error(G_db_connection));
    pthread_mutex_unlock(&G_SQL_query_mutex);
    return NULL;
   }

  pthread_mutex_unlock(&G_SQL_query_mutex);
  return sql_data;
}

int a_mysql_update
(char *sql_command)
/*
* Execute MYSQL UPDATE statement. Return execution status.
*/
{
  pthread_mutex_lock(&G_SQL_query_mutex);

  if(mysql_query(G_db_connection, sql_command))
   {
    pthread_mutex_unlock(&G_SQL_query_mutex);
    a_debug_info2(DEBUGLVL3,"a_mysql_update: MYSQL error %u: %s\n", 
                  mysql_errno(G_db_connection), mysql_error(G_db_connection));
                  pthread_mutex_unlock(&G_SQL_query_mutex);
    return 0;
   }

  pthread_mutex_unlock(&G_SQL_query_mutex);
  return 1;
}


int a_mysql_update_timestamp(char *device_id)
/*
* Update device archivization timestamp in MYSQL database.
*/
{
 char sql_query[1024];

 snprintf(sql_query,MAXQUERY,"update router_db set last_archived = now() where hostname='%s'",device_id);

 pthread_mutex_lock(&G_SQL_query_mutex);

 if(a_mysql_update(sql_query)) 
  { 
   pthread_mutex_unlock(&G_SQL_query_mutex);
   return 1;
  }

 pthread_mutex_unlock(&G_SQL_query_mutex);
 return 0;

}


int a_mysql_update_failed_archivizations(char *device_id)
/*
* Update failed archivizations column in MYSQL database.
*/
{
 char sql_query[1024];

 snprintf(sql_query,MAXQUERY,
          "update router_db set failed_archivizations = failed_archivizations+1 where hostname='%s'",
          device_id);
 
 if(a_mysql_update(sql_query)) 
  return 1;

 return 0;

}

int a_mysql_update_archivist_timestamp(void)
/*
* Update timestamp in the database - sign that we are still alive
*
*/
{
 char sql_query[1024];

 snprintf(sql_query,MAXQUERY,"update info set archivist_timestamp = now() where 1=1");

 if(a_mysql_update(sql_query))
  return 1;

 return 0;

}


void a_mysql_init_database
(void)
/*
* Init MYSQL database with default configuration info
*/
{
  
}

#endif

/* end of mysql.c */
