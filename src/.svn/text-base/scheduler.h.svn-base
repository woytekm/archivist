/*                                                                                           */
/* Copyright (c) 1987,1997, 2006, Vrije Universiteit, Amsterdam, The Netherlands             */
/* All rights reserved.                                                                      */
/*                                                                                           */
/* ========================================================================================= */

#include <sys/types.h>
#include <limits.h>

#define JOBMAX 255  /* maximum number of scheduled backups */

#define NEVER		((time_t) ((time_t) -1 < 0 ? LONG_MAX : ULONG_MAX))

typedef unsigned char bitmap_t[8];

typedef struct cronjob {        /* One entry in a crontab file */
        struct cronjob  *next;
        struct crontab  *tab;           /* Associated table file. */
        bitmap_t        min;            /* Minute (0-59) */
        bitmap_t        hour;           /* Hour (0-23) */
        bitmap_t        mday;           /* Day of the month (1-31) */
        bitmap_t        mon;            /* Month (1-12) */
        bitmap_t        wday;           /* Weekday (0-7 with 0 = 7 = Sunday) */
        char            *user;          /* User to run it as (nil = root) */
        char            *cmd;           /* Command to run */
        time_t          rtime;          /* When next to run */
        char            do_mday;        /* True iff mon or mday is not '*' */
        char            do_wday;        /* True iff wday is not '*' */
        char            late;           /* True iff the job is late */
        char            atjob;          /* True iff it is an AT job */
        pid_t           pid;            /* Process-id of job if nonzero */
} cronjob_t;

cronjob_t *G_cronjobs[JOBMAX];            /* All crontabs. */

int G_jobcount;

time_t G_now;     /* current time. */
time_t G_next;     /* current time. */

/* Bitmap operations. */
#define bit_set(map, n)         ((void) ((map)[(n) >> 3] |= (1 << ((n) & 7))))
#define bit_clr(map, n)         ((void) ((map)[(n) >> 3] &= ~(1 << ((n) & 7))))
#define bit_isset(map, n)       (!!((map)[(n) >> 3] & (1 << ((n) & 7))))


