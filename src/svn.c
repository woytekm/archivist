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
*    svn.c - SVN access procedures
*/

#include "defs.h"
#include "archivist_config.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "svn_client.h"
#include "svn_cmdline.h"
#include "svn_pools.h"
#include "svn_config.h"
#include "svn_fs.h"
#include "svn_error.h"
#include "svn_path.h"

#define APR_LOCALE_CHARSET   (const char *)1 


int a_svn_sparse_checkout
(char *repository_path, char *temp_working_dir, apr_pool_t *apr_pool, apr_pool_t *svn_pool)
/*
*
* SVN sparse checkout - check out an _empty_ subversion dir
*
*/
{

    svn_error_t* err;
    int int_err;
    apr_array_header_t *device_arr;
    svn_error_t *svn_err;
    struct stat checked_file_info;

    char temp_svn_path[MAXPATH];

    svn_client_ctx_t* context;

    a_debug_info2(DEBUGLVL5,"a_svn_sparse_checkout: args: [%s], [%s]",repository_path,temp_working_dir);

    /* if we have gotten this far - we are free to remove temporary  */
    /* svn directory for this device (if it exists for some reason). */

    a_remove_directory(temp_working_dir); 
    
    strncpy(temp_svn_path,temp_working_dir,MAXPATH);

    svn_err = svn_client_create_context( &context, svn_pool );

    if(svn_err){a_logmsg("a_svn_sparse_checkout: svn error: %s",svn_err->message);
                a_debug_info2(DEBUGLVL5,"a_svn_sparse_checkout: svn error: %s",svn_err->message);
                return -2;}

    svn_opt_revision_t pegrevision;
    pegrevision.kind = svn_opt_revision_unspecified;

    svn_opt_revision_t revision;
    revision.kind = svn_opt_revision_head;

    svn_err = svn_client_checkout3(NULL,
                            repository_path,
                            temp_svn_path,
                            &pegrevision,
                            &revision,
                            svn_depth_empty,
                            FALSE,
                            FALSE,
                            context,
                            svn_pool
                            );

    if(svn_err){a_logmsg("a_svn_sparse_checkout: svn error: %s",svn_err->message);
                a_debug_info2(DEBUGLVL5,"a_svn_sparse_checkout: svn error: %s",svn_err->message);
                a_debug_info2(DEBUGLVL5,"a_svn_sparse_checkout: svn error code: %d",svn_err->apr_err);
                return -2;}

    return 1;

 }



int a_svn_checkout
(char *device_name, char *repository_path, char *temp_working_dir, apr_pool_t *apr_pool, apr_pool_t *svn_pool)
/*
*
* SVN checkout - check out single file from subversion
*
*/
{

    svn_error_t* err;
    int int_err;
    apr_array_header_t *device_arr;
    svn_error_t *svn_err;
    struct stat checked_file_info;

    char temp_svn_path[MAXPATH];

    svn_client_ctx_t* context1;
    svn_client_ctx_t* context2;

    a_debug_info2(DEBUGLVL5,"a_svn_checkout: args: [%s], [%s], [%s]",
                  device_name,repository_path,temp_working_dir);

    /* if we have gotten this far - we are free to remove temporary  */
    /* svn directory for this device (if it exists for some reason). */

    a_remove_directory(temp_working_dir);  
                                          
    strcpy(temp_svn_path,temp_working_dir);

    svn_err = svn_client_create_context(&context1, svn_pool);

    if(svn_err){a_logmsg("%s: svn error: %s",device_name,svn_err->message);
                a_debug_info2(DEBUGLVL5,"a_svn_checkout: svn error: %s",svn_err->message); 
                return -2;}

    svn_err = svn_client_create_context(&context2, svn_pool);

    if(svn_err){
                a_logmsg("%s: svn error: %s",device_name,svn_err->message);
                a_debug_info2(DEBUGLVL5,"a_svn_checkout: svn error: %s",svn_err->message); 
                return -2;
               }

    svn_opt_revision_t pegrevision;
    pegrevision.kind = svn_opt_revision_unspecified;

    svn_opt_revision_t revision;
    revision.kind = svn_opt_revision_head;

    svn_err = svn_client_checkout3(NULL,
                            repository_path,
                            temp_svn_path,
                            &pegrevision,
                            &revision,
                            svn_depth_empty,
                            FALSE,
                            FALSE,
                            context1,
                            svn_pool
                            );

    if(svn_err)
     {
      a_logmsg("%s: svn error: %s",device_name,svn_err->message);
      a_debug_info2(DEBUGLVL5,"a_svn_checkout: svn error: %s",svn_err->message); 
      a_debug_info2(DEBUGLVL5,"a_svn_checkout: svn error code: %d",svn_err->apr_err);

      /* checked out dir doesn't exist - special action needed: */
      if(strstr(svn_err->message,"doesn't exist")) return -3;
      else return -2;
     }

    device_arr = apr_array_make(apr_pool, 1, sizeof(const char*));

    strcat(temp_svn_path,"/");
    strcat(temp_svn_path,device_name);

    *(const char**)apr_array_push(device_arr) = temp_svn_path;

    svn_err = svn_client_update3(NULL,
                            device_arr,
                            &revision,
                            svn_depth_empty,
                            FALSE,
                            FALSE,
                            FALSE,
                            context2,
                            svn_pool
                            );

    if(svn_err){a_logmsg("%s: svn error: %s",device_name,svn_err->message);
                a_debug_info2(DEBUGLVL5,"a_svn_checkout: svn error: %s",svn_err->message); return -2;}

    if(stat(temp_svn_path,&checked_file_info) != -1) return (int)checked_file_info.st_size; else return -1;

}



int a_svn_diff
(char *device_group, char *device_name, char *configured_by, char *repository_path, 
 char *temp_working_dir, apr_pool_t *apr_pool, apr_pool_t *svn_pool)
/*
*
* diff of a working copy and svn head (for a single file)
*
*/
{

    svn_error_t* err;
    int int_err;
    apr_array_header_t *device_arr;
    apr_file_t *diff_outfile;
    apr_file_t *diff_errfile;
    apr_array_header_t *array;
    struct stat svn_diff_file_info;
    int svn_diff_file_size = 0;

    char temp_svn_path[MAXPATH];
    char full_device_svn_path[MAXPATH];
    char svn_diff_outfname[50]=".";
    char svn_diff_errfname[50]=".";

    svn_client_ctx_t* context;

    svn_error_t *svn_err;

    temp_svn_path[0] = 0x0;
    full_device_svn_path[0] = 0x0;

    strcat(temp_svn_path,temp_working_dir);

    strcat(svn_diff_outfname,device_name);
    strcat(svn_diff_outfname,".diff.tmp");

    strcat(svn_diff_errfname,device_name);
    strcat(svn_diff_errfname,".diff.err.tmp");

    /* if device is in the device group - add group name to the SVN diff path: */

    if(strstr(device_group,"none"))
     snprintf(full_device_svn_path,MAXPATH,"%s/%s",repository_path,device_name);
    else
     snprintf(full_device_svn_path,MAXPATH,"%s/%s/%s",repository_path,device_group,device_name);

    apr_file_open(&diff_outfile, svn_diff_outfname, (APR_READ | APR_WRITE | APR_CREATE ),
                            ( APR_FPROT_UREAD | APR_FPROT_UWRITE |
                              APR_FPROT_GREAD | APR_FPROT_WREAD ),
                                        apr_pool);

    apr_file_open(&diff_errfile, svn_diff_errfname, (APR_READ | APR_WRITE | APR_CREATE ),
                            ( APR_FPROT_UREAD | APR_FPROT_UWRITE |
                              APR_FPROT_GREAD | APR_FPROT_WREAD ),
                                        apr_pool);

    svn_err = svn_client_create_context( &context, svn_pool );
    if(svn_err){a_logmsg("%s: svn error: %s",device_name,svn_err->message);
                a_debug_info2(DEBUGLVL5,"a_svn_diff: svn error: %s",svn_err->message); 
                return -1;}


    svn_opt_revision_t pegrevision;
    pegrevision.kind = svn_opt_revision_unspecified;

    svn_opt_revision_t revision1;
    revision1.kind = svn_opt_revision_working;

    svn_opt_revision_t revision2;
    revision2.kind = svn_opt_revision_head;

    strcat(temp_svn_path,"/");
    strcat(temp_svn_path,device_name);

    array = apr_array_make (apr_pool, 0, sizeof (const char *));

    svn_err = svn_client_diff4(array,
                             svn_path_canonicalize(temp_svn_path,apr_pool),
                             &revision1,
                             svn_path_canonicalize(full_device_svn_path,apr_pool),
                             &revision2,
                             NULL,
                             svn_depth_empty,
                             FALSE,
                             TRUE,
                             FALSE,
                             APR_LOCALE_CHARSET,
                             diff_outfile,
                             diff_errfile,
                             NULL,
                             context,
                             svn_pool);
    if(svn_err){
                a_logmsg("%s: svn error: %s",device_name,svn_err->message);
                a_debug_info2(DEBUGLVL5,"a_svn_diff: svn error: %s",svn_err->message); 
                return -1;
               }


    apr_file_close(diff_errfile);
    remove(svn_diff_errfname);
    apr_file_close(diff_outfile);

    if( (stat(svn_diff_outfname,&svn_diff_file_info)) == -1)
     a_debug_info2(DEBUGLVL5,"a_svn_diff: cannot stat %s!",svn_diff_outfname);

    svn_diff_file_size = (int)svn_diff_file_info.st_size; 

    if((G_config_info.keep_changelog) && (svn_diff_file_size > 0)) 
      if(!a_add_changelog_entry(svn_diff_outfname, svn_diff_file_size, device_name, configured_by))
       {
        a_logmsg("a_svn_diff: %s: cannot write changelog entry!",device_name);
        a_debug_info2(DEBUGLVL5,"a_svn_diff: cannot write changelog entry!",device_name);
       }

    /* what we are returning here, is the length of a diff file */
    /* if it is nonzero - revisions differ: */

    remove(svn_diff_outfname);
    return svn_diff_file_size;  

}



int a_svn_commit
(char *device_name, char *temp_working_dir, char *commit_as, 
 apr_pool_t *apr_pool, apr_pool_t *svn_pool)
/*
* commit changed file to SVN repository
*
*/
{
    svn_error_t* err;
    int int_err;
    apr_array_header_t *device_arr;
    svn_commit_info_t *commit_info = NULL;
    svn_auth_baton_t *auth_baton;
    svn_auth_provider_object_t *provider;
    svn_error_t *svn_err;
    char temp_svn_path[MAXPATH];
    svn_client_ctx_t* context;

    temp_svn_path[0] = 0x0;

    svn_err = svn_client_create_context( &context, svn_pool );

    if(svn_err){
                a_logmsg("%s: svn error: %s",device_name,svn_err->message);
                a_debug_info2(DEBUGLVL5,"a_svn_commit: svn error: %s",svn_err->message); return -1;
               }


    device_arr = apr_array_make(apr_pool, 1, sizeof(const char*));

    snprintf(temp_svn_path,MAXPATH,"%s/%s",temp_working_dir,device_name);

    *(const char**)apr_array_push(device_arr) = temp_svn_path;


    apr_array_header_t *providers = apr_array_make (apr_pool, 1, sizeof (svn_auth_provider_object_t *));

    svn_auth_get_username_provider(&provider, svn_pool);
    APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
    svn_auth_open(&auth_baton, providers, svn_pool);
    svn_auth_set_parameter(auth_baton, SVN_AUTH_PARAM_DEFAULT_USERNAME, commit_as);

    (*context).auth_baton = auth_baton;

    svn_err = svn_client_commit4(&commit_info,
                           device_arr,
                           svn_depth_empty,
                           FALSE,
                           FALSE,
                           NULL,
                           NULL,
                           context,
                           svn_pool);

    if(svn_err)
     {
      a_logmsg("%s: svn error: %s",device_name,svn_err->message);
      a_debug_info2(DEBUGLVL5,"a_svn_commit: svn error: %s",svn_err->message); return -1;
     }

    return 1;


}


int a_svn_add
(char *device_name, char *temp_working_dir, apr_pool_t *apr_pool, apr_pool_t *svn_pool)
/*
* add a new file to SVN repository
*
*/
{
  svn_error_t* svn_err;
  char temp_svn_path[MAXPATH];
  svn_client_ctx_t* context;

  temp_svn_path[0] = 0x0;

  svn_err = svn_client_create_context( &context, svn_pool );
  if(svn_err)
   {
    a_debug_info2(DEBUGLVL3,"a_svn_add: svn error: %s",svn_err->message);
    return -1;
   }

  snprintf(temp_svn_path,MAXPATH,"%s/%s",temp_working_dir,device_name);

  svn_err = svn_client_add4(temp_svn_path,
                            svn_depth_empty,
                            FALSE,
                            FALSE,
                            FALSE,
                            context,
                            svn_pool);

  if(svn_err){
              a_logmsg("%s: svn error: %s",device_name,svn_err->message);
              a_debug_info2(DEBUGLVL5,"a_svn_add: svn error: %s",svn_err->message); return -1;
             }

  return 1;

}


int a_svn_configured_repo_stat
(void)
/*
*
* Initial check if configured SVN repository is accessible
*
*/
 {

   int checkout_status;
   char svn_tmp_dirname[MAXPATH];
   apr_pool_t *svn_pool;
   apr_pool_t *apr_pool;

   svn_pool = svn_pool_create(NULL);
   apr_pool_create(&apr_pool,NULL);

   snprintf(svn_tmp_dirname,MAXPATH,"%s.%d.test",G_svn_tmp_prefix,G_config_info.instance_id);

   checkout_status = a_svn_sparse_checkout(G_config_info.repository_path,svn_tmp_dirname,
                                           apr_pool, svn_pool);

   if(checkout_status < 0)
    {
     a_debug_info2(DEBUGLVL3,"a_svn_configured_repo_stat: initial repository check failed!");
    }

   a_remove_directory(svn_tmp_dirname);

   svn_pool_destroy(svn_pool);
   apr_pool_destroy(apr_pool);

   return checkout_status;

 }

/* end of svn.c  */
