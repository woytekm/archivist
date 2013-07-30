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
*
*    Author: Wojtek Mitus (woytekm@gmail.com)
*
*    syslog.c - UDP syslog listener + syslog buffer parsing procedures
*/

#include "defs.h"
#include "archivist_config.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>


int a_syslog_socket_setup
(void)
/*
*
* get syslog socket ready
*
*/
{

       struct sockaddr_in si_me, si_other;
       int syslog_socket, i, slen=sizeof(si_other),flags=0;

       if ((syslog_socket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
        {
         fprintf(stderr,"a_syslog_socket_setup: cannot create UDP socket (%d)!",errno); 
         a_cleanup_and_exit();
        }

       memset((char *) &si_me, 0, sizeof(si_me));
       si_me.sin_family = AF_INET;
       si_me.sin_port = htons(G_config_info.syslog_port);
       si_me.sin_addr.s_addr = htonl(INADDR_ANY);
       if (bind(syslog_socket,(struct sockaddr *)&si_me, sizeof(si_me))==-1)
        {
         fprintf(stderr,"FATAL: a_syslog_socket_setup: cannot bind to UDP socket on port %d (%d)!",
                       G_config_info.syslog_port,errno); 
         a_cleanup_and_exit();
        }
       
       fcntl(syslog_socket, F_SETFL, flags | O_NONBLOCK);

       a_debug_info2(DEBUGLVL5,"a_syslog_socket_setup: syslog socket ready. return.");

       return syslog_socket;

}

int a_check_syslog_stream
(int sock, char *buf, char *from)
/*
* receive new data from UDP socket. 
* if there is more that one buf of data on the socket, then receive more, and reassemble.
* return something only if device config event is found in the received data.
* guard bounds of the receive buffer - receive only 8KB of data in one call 
* (8KB limit per syslog message).
*/
{
      struct sockaddr_in si_other;
      char wrkbuf[1024];
      int i,slen=sizeof(si_other),recv_flags=0;
      struct hostent *hostnam;
      char *regexp_test;
      int readed=1024,finished=0,received_data_len=0;

      if(G_stop_all_processing)
       return 1;   /* program is in the state of cleanup&exit - just return. */

      bzero(buf,BUFLEN);
      bzero(from,IPSTRLEN);
 
      while(!finished) {

        if ( (readed == 1024) && (buf[(strlen(buf)-1)] != 0xa) && (received_data_len < BUFLEN) ) {

         bzero(wrkbuf,1024);
         readed = recvfrom(sock,wrkbuf,SOCK_RECVLEN,recv_flags,(struct sockaddr *)&si_other,&slen);

         if(readed>0)                    
          strncat(buf,wrkbuf,readed+1);    /* reassemble syslog stream if longer than 1024 bytes */
          received_data_len += readed;    

         }

        else finished = 1;                 /* nothing more to receive */

        usleep(100);

       } 

       if(received_data_len>0)
        {
         a_debug_info2(DEBUGLVL5,"a_check_syslog_stream: readed %d bytes from network",received_data_len);
         regexp_test = a_config_regexp_match(buf); /* pre-filtering of syslog messages */

         if(regexp_test != NULL)
           {
            hostnam = gethostbyaddr(&si_other.sin_addr,4,AF_INET);
            strcat(from,hostnam->h_name); 
            a_debug_info2(DEBUGLVL5,"a_check_syslog_stream: got something !");
            return received_data_len;
           } 
        }

    return 0; 
}


a_parse_syslog_buffer
(char *syslog_message, char *src_ip)
/*
* parse pre-checked syslog buffer (data got from UDP or syslog file). 
* here, we are sure that we have matched ConfigRegexp in the received buffer.
* find config lines, and pass them to the next level of parsing
*/
{
   
    char localcopy[BUFLEN];
    char *strtok_pointer;
    char *regexp_test;
    char *username_token;

    a_debug_info2(DEBUGLVL5,"a_parse_syslog_buffer: got the following data:\n%s,\n",syslog_message);
    
    strncpy(localcopy,syslog_message,BUFLEN);

    strtok_pointer = (char *)strtok(localcopy,"\n");
    regexp_test = (char *)a_config_regexp_match(strtok_pointer);

     if( (strtok_pointer != NULL) && (regexp_test != NULL) )
      {
       username_token = regexp_test;
       a_parse_config_event(strtok_pointer,src_ip,username_token); 
      }

    while(strtok_pointer != NULL)
     {
      strtok_pointer = (char *)strtok(NULL,"\n");
      regexp_test = (char *)a_config_regexp_match(strtok_pointer);
      if( (strtok_pointer != NULL) && (regexp_test != NULL) ) 
       {
        username_token = regexp_test;
        a_parse_config_event(strtok_pointer,src_ip,username_token);
       }     
     }    

}


a_parse_config_event
(char *config_event_data, char *src_ip, char *username_preceding_token)
/*
* fill in config_event_info_t structure with data from a syslog line,
* and execute config archiving thread with this struct as an argument.
*
*/
{

    char *strtok_pointer;
    char src_ip_from_taillog[IPSTRLEN];
    int field_counter = 1;
    config_event_info_t *conf_event_info;

    conf_event_info = malloc(sizeof(config_event_info_t));

    conf_event_info->configured_by[0] = 0x0;
    conf_event_info->configured_on[0] = 0x0;
    conf_event_info->configured_from[0] = 0x0;

    a_debug_info2(DEBUGLVL5,"a_parse_config_event: allocated new data structure at 0x%p",conf_event_info);

    a_debug_info2(DEBUGLVL5,"a_parse_config_event: starting parsing syslog message...");

    strtok_pointer=(char *)strtok(config_event_data," "); 

    while(strtok_pointer != NULL)
     {
      
      if((strcmp(strtok_pointer,username_preceding_token) != 0) &&   
         (strtok_pointer != NULL))                          

      strtok_pointer = (char *)strtok(NULL," ");                  /* get next token from string */
      if(strtok_pointer != NULL)
       {

        if(strcmp(strtok_pointer,username_preceding_token) == 0) 
          {
           strtok_pointer = (char *)strtok(NULL," ");                /* next token should be username */
           if(strlen(strtok_pointer) > 0)                            /* use it, if it's nonzero length */
             strncpy(conf_event_info->configured_by,strtok_pointer,255);
          }

        }

      field_counter++;

      if((field_counter == G_config_info.hostname_field_in_syslog)&&(src_ip == NULL))  /* this is a line from tailing syslog file */
       { 						                 /* get source address/name from predefined field  */
        strcpy(src_ip_from_taillog,strtok_pointer); 
       }                 

     }

    if(src_ip != NULL) 
     strcpy(conf_event_info->device_id,src_ip);    
    else
     strcpy(conf_event_info->device_id,src_ip_from_taillog);

    if(strlen(conf_event_info->configured_by) == 0)
     {
      a_logmsg("%s: cannot parse config event message! not archived!",conf_event_info->device_id);
      a_debug_info2(DEBUGLVL5,"a_parse_config_event: error: cannot get username from config event message!",conf_event_info->device_id);
      return 0;
     }

    a_remove_quotes(conf_event_info->configured_by);  /* FWSM and JUNOS use quotation around the username */

    /* prepare to start archiver thread */

    while(G_active_archiver_threads >= NUM_ARCH_THREADS)
     {
      a_debug_info2(DEBUGLVL5,"a_parse_config_event: waiting for a thread count to decrease...");
      usleep(300);
     }

    /* start archiver thread for a single device and exit from function */

    a_debug_info2(DEBUGLVL5,"a_parse_config_event: finished parsing. starting archiver thread...");
    a_logmsg("%s: starting syslog-triggered archiver thread (configured by %s).",
             conf_event_info->device_id,conf_event_info->configured_by);
    
    pthread_t triggered_thread;
    pthread_attr_t thread_attr;    
    size_t stacksize = ARCHIVIST_THREAD_STACK_SIZE;

    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr,PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&thread_attr, stacksize);

    if(pthread_create(&triggered_thread, &thread_attr, a_archive_single, (void *)conf_event_info))
     {
      a_logmsg("%s: FATAL: cannot create archiver thread for %s!",
               conf_event_info->device_id);
      a_debug_info2(DEBUGLVL5,"a_parse_config_event: FATAL: cannot create archiver thread for %s!",
                    conf_event_info->device_id);
      free(conf_event_info);
      return 0;
     }

    return 1;    

}

/* end of syslog.c  */

