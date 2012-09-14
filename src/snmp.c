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
*    snmp.c - SNMP config download procedures
*/

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "defs.h"
#include "archivist_config.h"


int a_get_using_snmp
/*
* SNMP method for getting config
*
*/
(char *device_name, char *device_type, char *device_auth_set_name)
{

  int result;
  char result_file[MAXPATH];
  struct stat outfile;
  auth_set_t *device_auth_set;
  char *community_string;

  if((device_auth_set = a_auth_set_search(G_auth_set_list,device_auth_set_name)) == NULL)
   {
    a_debug_info2(DEBUGLVL3,"a_get_using_snmp: ERROR: auth set %s not found for device %s. not archiving!",
                  device_auth_set_name,device_name);
    return -1;
   }

  community_string = device_auth_set->login;

  snprintf(result_file,2048,"%s.new",device_name);

  /* dispatch SNMP request according to the device type (add your own SNMP methods here): */

  if(strstr(device_type,"cisco"))
    result = a_SNMP_IOS_get_config(device_name,community_string);  /*In SNMP config download we use
							            *first field in the auth_set as
							            *SNMP community string.*/
  else
    {
     a_logmsg("%s: %s device type is not supported by SNMP method.",device_name,device_type);
     return -1;
    }

  /* dispatch end. now we should have <hostname>.new file in CWD */

  if(stat(result_file,&outfile) == -1)
   {
    a_logmsg("%s: SNMP config get method: no downloaded config file found.",device_name);
    return -1; /* no config file after snmp config get - fail. */
   }
  else if(outfile.st_size < 10)
   {
    a_logmsg("%s: SNMP config get method: device config file is empty.",device_name);
    remove(result_file);
    return -1; /* config file empty or a few bytes long - fail. */
   }
  else
   {
    a_debug_info2(DEBUGLVL5,"a_get_using_snmp: re-formatting device config file.");

    pthread_mutex_lock (&G_perl_running_mutex);

    if(a_cleanup_config_file_p(result_file,device_type) == -1) /* re-format output config file */
     {
      pthread_mutex_unlock(&G_perl_running_mutex);
      a_logmsg("%s: SNMP method: post-processing config file failed.",device_name);
      remove(result_file);
      return -1;
     }

    pthread_mutex_unlock(&G_perl_running_mutex);

   }

  return result;

}



long a_snmpget_int
(char *host,char *object,char *community)
/*
* get integer value from SNMP path
*
*/
{
  char buf[SPRINT_MAX_LEN];
  char *error;
  struct snmp_session session,*sptr;
  void *sessp;
  struct snmp_pdu *pdu;
  struct snmp_pdu *response;
  struct variable_list *vars;
  struct tree *subtree;
  oid name[MAX_OID_LEN];
  size_t name_length = MAX_OID_LEN;
  int status;
  char type;
  char *as;
  long result;

  if (!read_objid(object,name,&name_length)) 
   {
    a_debug_info2(DEBUGLVL5,"a_snmpget_int: invalid ObjectID");
    return -1;
   }

  snmp_sess_init(&session);

  session.version = SNMP_VERSION_1;
  session.peername = host;
  session.community = community;

  session.community_len = strlen(session.community);

  sessp = snmp_sess_open(&session);


  if (sessp == NULL) 
   {
    snmp_sess_error(&session,NULL,NULL,&error); 
    a_logmsg("%s: failed to open SNMP session!",host);
    a_debug_info2(DEBUGLVL5,"a_snmpget_int: %s: failed to open SNMP session!",host);
    return -1;
   }
  
  sptr = snmp_sess_session(sessp);

  pdu = snmp_pdu_create(SNMP_MSG_GET);

  if (!snmp_parse_oid(object,name,&name_length)) 
   {
    a_debug_info2(DEBUGLVL5,"a_snmpget_int: %s: failed to parse OID!",host);
    return -1;
   } 
  else 
    snmp_add_null_var(pdu,name,name_length);

  status = snmp_sess_synch_response(sessp,pdu,&response);

  char val_buf[128];

  if (status == STAT_SUCCESS) 
   {
    if (response->errstat == SNMP_ERR_NOERROR) 
     {
      for(vars = response->variables; vars; vars = vars->next_variable) 
       {
        result = (long) *(vars->val.integer);
       }
     } 
    else 
     {
      a_debug_info2(DEBUGLVL1,"a_snmpget_int: %s: error in SNMP Packet: %s",
                    host,snmp_errstring(response->errstat));
      a_logmsg("%s: Error in SNMP Packet: %s",host,snmp_errstring(response->errstat));
      return -1;
     }
    } 
   else if (status == STAT_TIMEOUT) 
    {
     a_logmsg("%s: SNMP Timed out. (Check community string)",host);
     snmp_sess_close(sessp);
     return -1;
    } 
  else 
   {
    snmp_sess_error(&sessp,NULL,NULL,&error);
    snmp_sess_close(sessp);
    return -1;
   }

  if (response)
   snmp_free_pdu(response);

  snmp_sess_close(sessp);
  return result;
}



int a_SNMP_IOS_wr_net
(char *host,char *destination,char *community) 
/*
* trigger configuration upload to TFTP server (method for Cisco IOS)
*
*/
{
  char buf[SPRINT_MAX_LEN];
  char *error;
  struct snmp_session session,*sptr;
  void *sessp;
  struct snmp_pdu *pdu;
  struct snmp_pdu *response;
  struct variable_list *vars;
  struct tree *subtree;

  oid protocol_OID[MAX_OID_LEN];
  size_t protocol_OID_length = MAX_OID_LEN;

  oid src_file_type_OID[MAX_OID_LEN];
  size_t src_file_type_OID_length = MAX_OID_LEN;

  oid dst_file_type_OID[MAX_OID_LEN];
  size_t dst_file_type_OID_length = MAX_OID_LEN;

  oid tftp_addr_OID[MAX_OID_LEN];
  size_t tftp_addr_OID_length = MAX_OID_LEN;

  oid filename_OID[MAX_OID_LEN];
  size_t filename_OID_length = MAX_OID_LEN;

  oid transfer_start_OID[MAX_OID_LEN];
  size_t transfer_start_OID_length = MAX_OID_LEN;

  int status;

  char *protocol_OID_str = ".1.3.6.1.4.1.9.9.96.1.1.1.1.2.1";
  char *src_file_type_OID_str = ".1.3.6.1.4.1.9.9.96.1.1.1.1.3.1";
  char *dst_file_type_OID_str = ".1.3.6.1.4.1.9.9.96.1.1.1.1.4.1";
  char *tftp_addr_OID_str = ".1.3.6.1.4.1.9.9.96.1.1.1.1.5.1";
  char *filename_OID_str = ".1.3.6.1.4.1.9.9.96.1.1.1.1.6.1";
  char *transfer_start_OID_str = ".1.3.6.1.4.1.9.9.96.1.1.1.1.14.1";

  read_objid(protocol_OID_str,protocol_OID,&protocol_OID_length);
  read_objid(src_file_type_OID_str,src_file_type_OID,&src_file_type_OID_length);
  read_objid(dst_file_type_OID_str,dst_file_type_OID,&dst_file_type_OID_length);
  read_objid(tftp_addr_OID_str,tftp_addr_OID,&tftp_addr_OID_length);
  read_objid(filename_OID_str,filename_OID,&filename_OID_length);
  read_objid(transfer_start_OID_str,transfer_start_OID,&transfer_start_OID_length);

  snmp_sess_init(&session);

  session.version = SNMP_VERSION_1;
  session.peername = host;
  session.community = community;
  
  session.community_len = strlen(session.community);

  sessp = snmp_sess_open(&session);

  if (sessp == NULL) 
   {
    snmp_sess_error(sessp,NULL,NULL,&error);
    a_logmsg("%s: cannot open snmp session!",host);
    return 0;
   }

  sptr = snmp_sess_session(sessp);

  pdu = snmp_pdu_create(SNMP_MSG_SET);

  char dst_filename[MAXPATH];
  snprintf(dst_filename,MAXPATH,"%s.new",host);
 
  if (!snmp_parse_oid(protocol_OID_str,protocol_OID,&protocol_OID_length)) 
   {
    a_debug_info2(DEBUGLVL1,"a_SNMP_IOS_wr_net: %s: cannot parse OID! (%s)",host,protocol_OID_str);
    return 0;
   } 
  else 
   {
     if (snmp_add_var(pdu,protocol_OID,protocol_OID_length,'i',"1")) 
        {a_debug_info2(DEBUGLVL1,"a_SNMP_IOS_wr_net: cannot add var to pdu!\n"); return;}
     if (snmp_add_var(pdu,src_file_type_OID,src_file_type_OID_length,'i',"4"))
        {a_debug_info2(DEBUGLVL1,"a_SNMP_IOS_wr_net: cannot add var to pdu!\n"); return;}
     if (snmp_add_var(pdu,dst_file_type_OID,dst_file_type_OID_length,'i',"1")) 
        {a_debug_info2(DEBUGLVL1,"a_SNMP_IOS_wr_net: cannot add var to pdu!\n"); return;}
     if (snmp_add_var(pdu,tftp_addr_OID,tftp_addr_OID_length,'a',destination)) 
        {a_debug_info2(DEBUGLVL1,"a_SNMP_IOS_wr_net: cannot add var to pdu!\n"); return;}
     if (snmp_add_var(pdu,filename_OID,filename_OID_length,'s',dst_filename))
        {a_debug_info2(DEBUGLVL1,"a_SNMP_IOS_wr_net: cannot add var to pdu!\n"); return;}
     if (snmp_add_var(pdu,transfer_start_OID,transfer_start_OID_length,'i',"4")) 
        {a_debug_info2(DEBUGLVL1,"a_SNMP_IOS_wr_net: cannot add var to pdu!\n"); return;}
  }

  status = snmp_sess_synch_response(sessp,pdu,&response);

  if (status == STAT_SUCCESS) 
   {
    if (response->errstat != SNMP_ERR_NOERROR) 
     {
      a_logmsg("%s: Error in SNMP Packet: %s",host,snmp_errstring(response->errstat));
      return 0;
     }
   } 
  else if (status == STAT_TIMEOUT) 
   {
    a_logmsg("%s: SNMP Timed out. (Check community string)",host);
    snmp_sess_close(sessp);
    return 0;
   } 
  else 
   {
    snmp_sess_error(sessp,NULL,NULL,&error);
    snmp_sess_close(sessp);
    return 0;
   }

  if (response)
   snmp_free_pdu(response);

  snmp_sess_close(sessp);
  return 1;
}


int a_SNMP_IOS_reset_wr_net
(char *host,char *community)
/*
* reset upload procedure on IOS device
*
*/
{
  char buf[SPRINT_MAX_LEN];
  char *error;
  struct snmp_session session,*sptr;
  void *sessp;
  struct snmp_pdu *pdu;
  struct snmp_pdu *response;
  struct variable_list *vars;
  struct tree *subtree;

  oid reset_OID[MAX_OID_LEN];
  size_t reset_OID_length = MAX_OID_LEN;

  int status;

  char *reset_OID_str = ".1.3.6.1.4.1.9.9.96.1.1.1.1.14.1";

  read_objid(reset_OID_str,reset_OID,&reset_OID_length);

  snmp_sess_init(&session);

  session.version = SNMP_VERSION_1;
  session.peername = host;
  session.community = community;

  session.community_len = strlen(session.community);

  sessp = snmp_sess_open(&session);


  if (sessp == NULL) 
   {
    snmp_sess_error(sessp,NULL,NULL,&error);
    a_logmsg("%s: cannot open snmp session!",host);
    return 0;
   }

  sptr = snmp_sess_session(sessp);

  pdu = snmp_pdu_create(SNMP_MSG_SET);

  if (!snmp_parse_oid(reset_OID_str,reset_OID,&reset_OID_length)) {
    a_debug_info2(DEBUGLVL1,"a_SNMP_IOS_reset_wr_net: %s: cannot parse OID! (%s)",host,reset_OID_str);
    return 0;
  } else {
     if (snmp_add_var(pdu,reset_OID,reset_OID_length,'i',"6"))
        {a_debug_info2(DEBUGLVL1,"a_SNMP_IOS_reset_wr_net: %s: cannot add var to pdu!",host); return 0;}
  }

  status = snmp_sess_synch_response(sessp,pdu,&response);

  if (status == STAT_SUCCESS) {
    if (response->errstat != SNMP_ERR_NOERROR) {
      a_logmsg("%s: Error in SNMP Packet: %s",host,snmp_errstring(response->errstat));
    }
  } else if (status == STAT_TIMEOUT) {
    snmp_sess_close(sessp);
    a_logmsg("%s: SNMP Timed out. (Check community string)",host);
    return 0;
  } else {
    snmp_sess_error(sessp,NULL,NULL,&error);
    snmp_sess_close(sessp);
  }

  if (response)
   snmp_free_pdu(response);

  snmp_sess_close(sessp);
  return 1;
}


int a_SNMP_IOS_get_config
/*
* SNMP-trigger configuration upload to TFTP server (method for Cisco IOS)
*
*/
(char *hostname, char *community)
{
  char tftp_path[MAXPATH];
  char dst_filename[MAXPATH];

  
  /* build up paths and filenames for download procedure */

  snprintf(dst_filename,MAXPATH,"%s.new",hostname);
  snprintf(tftp_path,MAXPATH,"%s/%s",G_config_info.tftp_dir,dst_filename);

  a_ftouch(tftp_path);

  chmod(tftp_path,0000777);

  snmp_disable_log();

  a_SNMP_IOS_reset_wr_net(hostname,community);

  usleep(500);

  /* this will overwrite old file of the same name */

  if(a_SNMP_IOS_wr_net(hostname,G_config_info.tftp_ip,community)) 
   {
    int finished = 0,timeout = 0;
    while((finished != 3) && (timeout < 20)) /* wait 20 sec for the device */
     {
      finished = a_snmpget_int(hostname,".1.3.6.1.4.1.9.9.96.1.1.1.1.10.1",community);
      sleep(1);
      timeout++;
     }
    if(timeout >= 20) return 0;

    a_debug_info2(DEBUGLVL5,"SNMP_IOS_get_config: about to move %s to %s",tftp_path,dst_filename);

    if((a_rename(tftp_path,dst_filename)) == -1)
     {
      a_debug_info2(DEBUGLVL3,"SNMP_IOS_get_config: cannot rename working config file (%d)!",errno);
      return 0;
     }
    return 1;
   }

  a_logmsg("%s: SNMP config get failed!",hostname);

  return 0;
}

/* end of snmp.c */
