/*
 *
 *    This file is part of Archivist - network device config archiver.
 *
 *    Archivist is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
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
 *    taillog.c - syslog file follower procedures
 */

#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/wait.h>

#include "defs.h"
#include "archivist_config.h"

FILE *a_syslog_fstream_setup
(void)
/*
* get file descriptor of the tailed syslog file ready
*
*/
{
  int fsize;
  int fdesc;

  fdesc = open(G_config_info.syslog_filename,O_RDONLY);

  if(fdesc == NULL)
   {
    a_debug_info2(DEBUGLVL3,"a_syslog_fstream_setup: error opening syslog file for tailing!");
    exit(1);
   }

  fsize = a_filesize(fdesc);

  lseek(fdesc, fsize, SEEK_SET); /* move to the end of the file to get ready for tailing */

  a_debug_info2(DEBUGLVL5,"a_syslog_fstream_setup: syslog file descriptor ready.");

  return fdesc;

}



int a_check_taillog_stream
(int fd,char *syslog_buffer,char *ip_from)
/*
* check tailed syslog file for config event messages
* +logfile rotation detection & reopening
*/
{

    int new_fsize=0,bufsize;
    struct stat syslogfile_stat;

    if(G_stop_all_processing)
     return 1;   /* program is in the state of cleanup&exit - just return. */

    bzero(syslog_buffer,BUFLEN);

    if((stat(G_config_info.syslog_filename,&syslogfile_stat)) == -1)   /* there is no syslog file! */
      return 0;							       /* maybe it's rotated now - return.*/

    new_fsize = syslogfile_stat.st_size;
    
    if((new_fsize - G_syslog_file_size)>0)
      {
       G_syslog_file_size = new_fsize;
       a_get_new_lines(fd,syslog_buffer);
       if(a_config_regexp_match(syslog_buffer))
        {
         bufsize = strlen(syslog_buffer);
         a_debug_info2(DEBUGLVL5,"a_check_taillog_stream: got something!");
         return bufsize;
        } 
       else 
        return 0;  /* no match for config regexp - discard current buffer and go on */
      }

     else if((new_fsize-G_syslog_file_size)<0) /* logfile turned over */
      {
       a_logmsg("tailed syslog file turned over. reopening.");
       close(G_syslog_file_handle);
       G_syslog_file_handle = a_syslog_fstream_setup();
      }

     G_syslog_file_size = new_fsize;

     return 0; /* no match because routine couldn't tail file in this pass */

}


int a_get_new_lines
(int fd, char *buf)
/*
* folow-tail file, return new data in buf
*
*/
{
  char buffer[BUFLEN];
  int bytes_read;
  long total;

  bzero(buf,BUFLEN);

  total = 0;

  while ((bytes_read = a_safe_read (fd, buffer, BUFLEN)) > 0)
    {
     memcpy(buf,buffer,bytes_read);
     total += bytes_read;
    }
 
  if(total)
   a_debug_info2(DEBUGLVL5,"a_get_new_lines: got %d bytes of new data from syslog file",total);

  return total;
}


/* end of taillog.c  */

