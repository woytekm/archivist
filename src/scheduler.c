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
*    This file incorporates work covered by the following copyright and  
*    permission notice: 
*
*      Copyright (c) 1987, 1997, 2006, Vrije Universiteit, Amsterdam,
*      The Netherlands All rights reserved. Redistribution and use of the MINIX 3
*      operating system in source and binary forms, with or without
*      modification, are permitted provided that the following conditions are
*      met:
*
*      * Redistributions of source code must retain the above copyright
*      notice, this list of conditions and the following disclaimer.
*
*      * Redistributions in binary form must reproduce the above copyright
*      notice, this list of conditions and the following disclaimer in the
*      documentation and/or other materials provided with the distribution.
*
*      * Neither the name of the Vrije Universiteit nor the names of the
*      software authors or contributors may be used to endorse or promote
*      products derived from this software without specific prior written
*      permission.
*
*      * Any deviations from these conditions require written permission
*      from the copyright holder in advance
*
*
*      Disclaimer
*
*      THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS, AUTHORS, AND
*      CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
*      INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
*      MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
*      NO EVENT SHALL PRENTICE HALL OR ANY AUTHORS OR CONTRIBUTORS BE LIABLE
*      FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
*      CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
*      SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
*      BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
*      WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
*      OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
*      ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*    Author of original cron code: Kees J. Bot (7 Dec 1996)
*    Author of Archivist adaptation: Wojtek Mitus (woytekm@gmail.com)
*
*    scheduler.c - implement own cron-like scheduler for scheduling archiving jobs
*    (code adapted from minix 3 cron sources)
*/

#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/syslog.h>

#include "archivist_config.h"
#include "defs.h"
#include "scheduler.h"


void a_job_reschedule
(cronjob_t *job)
/*                                                                              
*   Reschedule one job. Compute the next time to run the job in job->rtime.   
*   (code donor here was minix 3 cron server)                                 
*/
{
        struct tm prevtm, nexttm, tmptm;
        time_t nodst_rtime, dst_rtime;

        /* Was the job scheduled late last time? */
        if (job->late) job->rtime= G_now;

        localtime_r(&job->rtime, &prevtm);
        prevtm.tm_sec= 0;

        nexttm= prevtm;
        nexttm.tm_min++;        /* Minimal increment */

        for (;;)
        {
                if (nexttm.tm_min > 59)
                {
                        nexttm.tm_min= 0;
                        nexttm.tm_hour++;
                }
                if (nexttm.tm_hour > 23)
                {
                        nexttm.tm_min= 0;
                        nexttm.tm_hour= 0;
                        nexttm.tm_mday++;
                }
                if (nexttm.tm_mday > 31)
                {
                        nexttm.tm_hour= nexttm.tm_min= 0;
                        nexttm.tm_mday= 1;
                        nexttm.tm_mon++;
                }
                if (nexttm.tm_mon >= 12)
                {
                        nexttm.tm_hour= nexttm.tm_min= 0;
                        nexttm.tm_mday= 1;
                        nexttm.tm_mon= 0;
                        nexttm.tm_year++;
                }

                /* Verify tm_year. A crontab entry cannot restrict tm_year
                 * directly. However, certain dates (such as Feb, 29th) do
                 * not occur every year. We limit the difference between
                 * nexttm.tm_year and prevtm.tm_year to detect impossible dates
                 * (e.g, Feb, 31st). In theory every date occurs within a
                 * period of 4 years. However, some years at the end of a
                 * century are not leap years (e.g, the year 2100). An extra
                 * factor of 2 should be enough.
                 */
                if (nexttm.tm_year-prevtm.tm_year > 2*4)
                {
                        job->rtime= NEVER;
                        return;                 /* Impossible combination */
                }

                if (!job->do_wday)
                {
                        /* Verify the mon and mday fields. If do_wday and
                         * do_mday are both true we have to merge both
                         * schedules. This is done after the call to mktime.
                         */
                        if (!bit_isset(job->mon, nexttm.tm_mon))
                        {
                                /* Clear other fields */
                                nexttm.tm_mday= 1;
                                nexttm.tm_hour= nexttm.tm_min= 0;

                                nexttm.tm_mon++;
                                continue;
                        }

                        /* Verify mday */
                        if (!bit_isset(job->mday, nexttm.tm_mday))
                        {
                                /* Clear other fields */
                                nexttm.tm_hour= nexttm.tm_min= 0;

                                nexttm.tm_mday++;
                                continue;
                        }
                }

                /* Verify hour */
                if (!bit_isset(job->hour, nexttm.tm_hour))
                {
                        /* Clear tm_min field */
                        nexttm.tm_min= 0;

                        nexttm.tm_hour++;
                        continue;
                }

                /* Verify min */
                if (!bit_isset(job->min, nexttm.tm_min))
                {
                        nexttm.tm_min++;
                        continue;
                }

                /* Verify that we don't have any problem with DST. Try
                 * tm_isdst=0 first. */
                tmptm= nexttm;
                tmptm.tm_isdst= 0;
#if 0
                fprintf(stderr,
        "a_tab_reschedule: trying %04d-%02d-%02d %02d:%02d:%02d isdst=0",
                                1900+nexttm.tm_year, nexttm.tm_mon+1,
                                nexttm.tm_mday, nexttm.tm_hour,
                                nexttm.tm_min, nexttm.tm_sec);
#endif
                nodst_rtime= job->rtime= mktime(&tmptm);
                if (job->rtime == -1) {
                        /* This should not happen. */
                        job->rtime= NEVER;
                        return;
                }
                localtime_r(&job->rtime, &tmptm);
                if (tmptm.tm_hour != nexttm.tm_hour ||
                        tmptm.tm_min != nexttm.tm_min)
                {
                        /* following dst asserts were commented out, as they are */
                        /* causing random exceptions on my test linux system (wm)*/

                        /* assert(tmptm.tm_isdst); */ 
                        tmptm= nexttm;
                        tmptm.tm_isdst= 1;
#if 0
                        a_logmsg(
        "a_tab_reschedule: trying %04d-%02d-%02d %02d:%02d:%02d isdst=1",
                                1900+nexttm.tm_year, nexttm.tm_mon+1,
                                nexttm.tm_mday, nexttm.tm_hour,
                                nexttm.tm_min, nexttm.tm_sec);
#endif
                        dst_rtime= job->rtime= mktime(&tmptm);
                        if (job->rtime == -1) {
                                /* This should not happen. */
                                job->rtime= NEVER;
                                return;
                        }
                        localtime_r(&job->rtime,&tmptm);
                        if (tmptm.tm_hour != nexttm.tm_hour ||
                                tmptm.tm_min != nexttm.tm_min)
                        {
                                /* assert(!tmptm.tm_isdst); */
                                /* We have a problem. This time neither
                                 * exists with DST nor without DST.
                                 * Use the latest time, which should be
                                 * nodst_rtime.
                                 */
                                /* assert(nodst_rtime > dst_rtime); */

                                job->rtime= nodst_rtime;

                                a_logmsg(
                        "a_tab_reschedule: During DST trans. %04d-%02d-%02d %02d:%02d:%02d (tmptm.tm_isdst: %d)",
                                        1900+nexttm.tm_year, nexttm.tm_mon+1,
                                        nexttm.tm_mday, nexttm.tm_hour,
                                        nexttm.tm_min, nexttm.tm_sec, tmptm.tm_isdst);

                        }
                }

                /* Verify this the combination (tm_year, tm_mon, tm_mday). */
                if (tmptm.tm_mday != nexttm.tm_mday ||
                        tmptm.tm_mon != nexttm.tm_mon ||
                        tmptm.tm_year != nexttm.tm_year)
                {
                        /* Wrong day */
#if 0
                        fprintf(stderr, "Wrong day\n");
#endif
                        nexttm.tm_hour= nexttm.tm_min= 0;
                        nexttm.tm_mday++;
                        continue;
                }

                /* Check tm_wday */
                if (job->do_wday && bit_isset(job->wday, tmptm.tm_wday))
                {
                        /* OK, wday matched */
                        break;
                }

                /* Check tm_mday */
                if (job->do_mday && bit_isset(job->mon, tmptm.tm_mon) &&
                        bit_isset(job->mday, tmptm.tm_mday))
                {
                        /* OK, mon and mday matched */
                        break;
                }

                if (!job->do_wday && !job->do_mday)
                {
                        /* No need to match wday and mday */
                        break;
                }

                /* Wrong day */
#if 0
                fprintf(stderr, "Wrong mon+mday or wday\n");
#endif
                nexttm.tm_hour= nexttm.tm_min= 0;
                nexttm.tm_mday++;
        }
#if 0
        fprintf(stderr, "Using %04d-%02d-%02d %02d:%02d:%02d \n",
                1900+nexttm.tm_year, nexttm.tm_mon+1, nexttm.tm_mday,
                nexttm.tm_hour, nexttm.tm_min, nexttm.tm_sec);
        localtime_r(&job->rtime,&tmptm);
        fprintf(stderr, "Act. %04d-%02d-%02d %02d:%02d:%02d isdst=%d\n",
                1900+tmptm.tm_year, tmptm.tm_mon+1, tmptm.tm_mday,
                tmptm.tm_hour, tmptm.tm_min, tmptm.tm_sec,
                tmptm.tm_isdst);
#endif


        /* Is job issuing lagging behind with the progress of time? */
        job->late= (job->rtime < G_now);

        /* The result is in job->rtime. */
        if (job->rtime < G_next) G_next= job->rtime;
}

#define isdigit(c)      ((unsigned) ((c) - '0') < 10)

static char *a_get_token
(char **ptr)
/* Return a pointer to the next token in a string.  Move *ptr to the end of
 * the token, and return a pointer to the start.  If *ptr == start of token
 * then we're stuck against a newline or end of string.
 * (code donor was minix 3 cron server)
 */
{
        char *start, *p;

        p= *ptr;
        while (*p == ' ' || *p == '\t') p++;

        start= p;
        while (*p != ' ' && *p != '\t' && *p != '\n' && *p != 0) p++;
        *ptr= p;
        return start;
}

static int a_range_parse
(char *data, bitmap_t map,int min, int max, int *wildcard)
/* Parse a comma separated series of 'n', 'n-m' or 'n:m' numbers.  'n'
 * includes number 'n' in the bit map, 'n-m' includes all numbers between
 * 'n' and 'm' inclusive, and 'n:m' includes 'n+k*m' for any k if in range.
 * Numbers must fall between 'min' and 'max'.  A '*' means all numbers.  A
 * '?' is allowed as a synonym for the current minute, which only makes sense
 * in the minute field, i.e. max must be 59.  Return true iff parsed ok.
 */
{
        char *p;
        int end;
        int n, m;
        struct tm tmptm;

        /* Clear all bits. */
        for (n= 0; n < 8; n++) map[n]= 0;

        p= data;
        while (*p != ' ' && *p != '\t' && *p != '\n' && *p != 0) p++;
        end= *p;
        *p= 0;
        p= data;

        if (*p == 0) {
                return 0;
        }

        /* Is it a '*'? */
        if (p[0] == '*' && p[1] == 0) {
                for (n= min; n <= max; n++) bit_set(map, n);
                p[1]= end;
                *wildcard= 1;
                return 1;
        }
        *wildcard= 0;

        /* Parse a comma separated series of numbers or ranges. */
        for (;;) {
                if (*p == '?' && max == 59 && p[1] != '-') {
                        
                        localtime_r(&G_now, &tmptm);
                        n = tmptm.tm_min;
                        p++;
                } else {
                        if (!isdigit(*p)) goto syntax;
                        n= 0;
                        do {
                                n= 10 * n + (*p++ - '0');
                                if (n > max) goto range;
                        } while (isdigit(*p));
                }
                if (n < min) goto range;

                if (*p == '-') {        /* A range of the form 'n-m'? */
                        p++;
                        if (!isdigit(*p)) goto syntax;
                        m= 0;
                        do {
                                m= 10 * m + (*p++ - '0');
                                if (m > max) goto range;
                        } while (isdigit(*p));
                        if (m < n) goto range;
                        do {
                                bit_set(map, n);
                        } while (++n <= m);
                } else
                if (*p == ':') {        /* A repeat of the form 'n:m'? */
                        p++;
                        if (!isdigit(*p)) goto syntax;
                        m= 0;
                        do {
                                m= 10 * m + (*p++ - '0');
                                if (m > (max-min+1)) goto range;
                        } while (isdigit(*p));
                        if (m == 0) goto range;
                        while (n >= min) n-= m;
                        while ((n+= m) <= max) bit_set(map, n);
                } else {
                                        /* Simply a number */
                        bit_set(map, n);
                }
                if (*p == 0) break;
                if (*p++ != ',') goto syntax;
        }
        *p= end;
        return 1;
  syntax:
        return 0;
  range:
        return 0;
}


cronjob_t *a_job_parse
(char *job_data,int *jobcnt)
/*
 * parse cron job string to a cronjob_t structure
 * (code donor was minix 3 cron server)
 */
{
        cronjob_t *job;
        char *p, *real_p, *q;
        size_t n;
        ssize_t r;
        int ok,wc,joblen;
          
        joblen = strlen(job_data)+1;
 
        real_p = malloc(joblen);

        strncpy(real_p,job_data,joblen);

        p = real_p;     

        /* Parse the job data */
        ok= 1;
        while (ok && *p != 0) {
                q= a_get_token(&p);

                if (*q == '#' || q == p) {
                        /* Comment or empty. */
                        while (*p != 0 && *p++ != '\n') {}
                        continue;
                }

                /* One new job coming up. */
                job= malloc(sizeof(*job));
                job->next = nil;

                if (!a_range_parse(q, job->min, 0, 59, &wc)) {
                        ok= 0;
                        break;
                }

                q= a_get_token(&p);
                if (!a_range_parse(q, job->hour, 0, 23, &wc)) {
                        ok= 0;
                        break;
                }

                q= a_get_token(&p);
                if (!a_range_parse(q, job->mday, 1, 31, &wc)) {
                        ok= 0;
                        break;
                }
                job->do_mday= !wc;

                q= a_get_token(&p);
                if (!a_range_parse(q, job->mon, 1, 12, &wc)) {
                        ok= 0;
                        break;
                }
                job->do_mday |= !wc;

                q= a_get_token(&p);
                if (!a_range_parse(q, job->wday, 0, 7, &wc)) {
                        ok= 0;
                        break;
                }
                job->do_wday= !wc;

                /* 7 is Sunday, but 0 is a common mistake because it is in the
                 * tm_wday range.  We allow and even prefer it internally.
                 */
                if (bit_isset(job->wday, 7)) {
                        bit_clr(job->wday, 7);
                        bit_set(job->wday, 0);
                }

                /* The month range is 1-12, but tm_mon likes 0-11. */
                job->mon[0] >>= 1;
                if (bit_isset(job->mon, 8)) bit_set(job->mon, 7);
                job->mon[1] >>= 1;

                /* Scan for options. */
                job->user= nil;
                while (q= a_get_token(&p), *q == '-') {
                        q++;
                        if (q[0] == '-' && q+1 == p) {
                                /* -- */
                                q= a_get_token(&p);
                                break;
                        }
                        while (q < p) switch (*q++) {
                        case 'u':
                                if (q == p) q= a_get_token(&p);
                                if (q == p) goto usage;
                                memmove(q-1, q, p-q);   /* gross... */
                                p[-1]= 0;
                                job->user= q-1;
                                q= p;
                                break;
                        default:
                        usage:
                                ok= 0;
                                goto parse_error;
                        }
                }


                /* Inspect the first character of the command. */
                job->cmd= q;
                if (q == p || *q == '#') {
                        /* Rest of the line is empty, i.e. the commands are on
                         * the following lines indented by one tab.
                         */
                        while (*p != 0 && *p++ != '\n') {}
                        if (*p++ != '\t') {
                                ok= 0;
                                goto parse_error;
                        }
                        while (*p != 0) {
                                if ((*q = *p++) == '\n') {
                                        if (*p != '\t') break;
                                        p++;
                                }
                                q++;
                        }
                } else {
                        /* The command is on this line.  Alas we must now be
                         * backwards compatible and change %'s to newlines.
                         */
                        p= q;
                        while (*p != 0) {
                                if ((*q = *p++) == '\n') break;
                                if (*q == '%') *q= '\n';
                                q++;
                        }
                }

                *q= 0;
                job->rtime= G_now;
                job->late= 0;           /* It is on time. */
                job->atjob= 0;          /* True cron job. */
                a_job_reschedule(job);    /* Compute next time to run. */
        }
   
     (*jobcnt)++;

     return job;
    
    parse_error:
     a_debug_info2(DEBUGLVL3,"a_job_parse: cronjob parsing error!\n");
     a_logmsg("ScheduleBackup - config line parse error!");
     return NULL;

}



void a_check_and_run_jobs
(void)
/*
* check job table for jobs with incoming run time. if scheduled time is now - run a job, 
* and then reschedule it.
* as a job command we take the following:
* device hostname - to schedule backup of a single device 
* "all" - to schedule backup of all devices
* "log-marker" - to schedule insert of log marking in a daemon log file
*/
{

   int i;
   pthread_t scheduled_thread;


   if(G_stop_all_processing)
     return 1;           /* program is in the state of cleanup&exit - just return. */

   time(&G_now);

   for(i=0;i<G_jobcount;i++)
    {
    if(G_cronjobs[i]->rtime <= G_now)
     {

      a_debug_info2(DEBUGLVL5,"a_check_and_run_jobs: time has come for job %d: %s",i,G_cronjobs[i]->cmd);

      if(strstr(G_cronjobs[i]->cmd,"log-marker")) 
        a_logmsg("-- MARK --");
      else if(strstr(G_cronjobs[i]->cmd,"dump-memstats"))
        a_dump_memstats();
#ifdef USE_MYSQL
      else if(strstr(G_cronjobs[i]->cmd,"update-timestamp")) 
        a_mysql_update_archivist_timestamp();
#endif
      else if(strstr(G_cronjobs[i]->cmd,"all"))
       {

        /* starting a bulk archiving thread is different that starting one 
         * for a single device.
         * in bulk thread started from scheduler, we want to start, and go on
         * further as quickly as possible, so checking active thread count
         * and waiting for thread_count to decrease is done in-thread, 
         * instead of doing this before starting a thread.
         */

        pthread_attr_t thread_attr;

        size_t stacksize = ARCHIVIST_THREAD_STACK_SIZE;

        pthread_attr_init(&thread_attr);
        pthread_attr_setdetachstate(&thread_attr,PTHREAD_CREATE_DETACHED);
        pthread_attr_setstacksize(&thread_attr, stacksize);

        a_logmsg("starting scheduled bulk archiver thread");

        if(pthread_create(&scheduled_thread, &thread_attr, a_archive_bulk,NULL))
         {
          a_logmsg("ERROR: cannot create bulk archiver thread!");
          a_debug_info2(DEBUGLVL5,"a_check_and_run_jobs: cannot create bulk archiver thread!");
         }

       }
      else 
       { 
        /* scheduled archiving for a single device */
        
        config_event_info_t *confinfo;
        pthread_attr_t thread_attr;

        confinfo = malloc(sizeof(config_event_info_t));

        size_t stacksize = ARCHIVIST_THREAD_STACK_SIZE;

        pthread_attr_init(&thread_attr);
        pthread_attr_setdetachstate(&thread_attr,PTHREAD_CREATE_DETACHED);
        pthread_attr_setstacksize(&thread_attr, stacksize);

        strcpy(confinfo->configured_by,"scheduled_archiving");
        strncpy(confinfo->device_id,G_cronjobs[i]->cmd,sizeof(confinfo->device_id));
        a_trimwhitespace(confinfo->device_id); 
        
        a_debug_info2(DEBUGLVL5,"a_check_and_run_jobs: starting scheduled device archiving for: [%s]."
                      ,confinfo->device_id); 
        a_logmsg("%s: starting scheduled archiver thread.",confinfo->device_id);

        if(pthread_create(&scheduled_thread, &thread_attr, a_archive_single, (void *)confinfo))
         {
          a_logmsg("fatal! cannot create scheduled archiver thread!");
          a_debug_info2(DEBUGLVL5,"a_check_and_run_jobs: cannot create scheduled archiver thread!");
         }

        usleep(500);

       }

      a_job_reschedule(G_cronjobs[i]);

    }
   }
}



/* end of scheduler.c  */

