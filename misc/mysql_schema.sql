--
-- Table structure for table `archivist_config`
--

CREATE TABLE archivist_config (
  instance_id int(11) default NULL,
  working_dir char(255) default NULL,
  logging int(11) default NULL,
  log_filename char(255) default NULL,
  archiving_method char(255) default NULL,
  tftp_dir char(255) default NULL,
  tftp_ip char(255) default NULL,
  script_dir char(255) default NULL,
  rancid_exec_path char(255) default NULL,
  expect_exec_path char(255) default NULL,
  tail_syslog int(11) default NULL,
  syslog_filename char(255) default NULL,
  listen_syslog int(11) default NULL,
  syslog_port int(11) default NULL,
  keep_changelog int(11) default NULL,
  changelog_filename char(255) default NULL,
  router_db_path char(255) default NULL,
  repository_path char(255) default NULL,
  archiver_threads int(11) default NULL
  open_command_socket int(11) default NULL,
  command_socket_path char(255) default NULL,
) TYPE=MyISAM;


--
-- Table structure for table `auth_sets`
--

CREATE TABLE auth_sets (
  set_name char(255) default NULL,
  login char(255) default NULL,
  password1 char(255) default NULL,
  password2 char(255) default NULL
) TYPE=MyISAM;


--
-- Table structure for table `cronjobs`
--

CREATE TABLE cronjobs (
  min char(2) default NULL,
  hour char(2) default NULL,
  dmnth char(2) default NULL,
  month char(2) default NULL,
  dweek char(2) default NULL,
  command char(255) default NULL,
  running int(11) default NULL,
  job_id int(11) default NULL
) TYPE=MyISAM;


--
-- Table structure for table `router_db`
--

CREATE TABLE router_db (
  hostname char(255) default NULL,
  hosttype char(255) default NULL,
  authset char(255) default NULL,
  arch_method char(255) default NULL,
  archived_now int(11) default NULL,
  last_archived datetime,
  failed_archivizations int(11)
) TYPE=MyISAM;


--
-- Table structure for table `eventlog`
--

CREATE TABLE eventlog (
  ev_date datetime,
  ev_class char(255),
  ev_device char(255),
  ev_data char(255)
) TYPE=MyISAM;


--
-- Table structure for table `changelog`
--

CREATE TABLE changelog (
  chg_date datetime,
  chg_author char(255),
  chg_device char(255),
  chg_data char(16384)
) TYPE=MyISAM;


--
-- Table structure for table `config_regexp`
--

CREATE TABLE config_regexp (
  regexp_string char(255),
  username_field_token char(255)
) TYPE=MyISAM;

--
-- Table structure for table `info`
--

CREATE TABLE info (
  archivist_timestamp datetime 
) TYPE=MyISAM;


--
-- load config defaults
--

insert into info values (now());

INSERT INTO archivist_config VALUES (2,'/usr/local/tmp',1,'/usr/local/var/archivist.log','internal','','','/usr/local/share/archivist/helpers','/usr/local/rancid/bin/rancid','/usr/local/bin/expect',1,'/var/log/syslog_info',0,0,1,'/usr/local/var/config_changelog.log','/usr/local/share/archivist/router.db','file:///usr/local/archivist-svn/',20,1,'/tmp/archivist.sock');

INSERT INTO config_regexp VALUES ('SYS-5-CONFIG_I','by');
INSERT INTO config_regexp VALUES ('running-config','by');
INSERT INTO config_regexp VALUES ('UI_DBASE_LOGOUT_EVENT','User');


--
-- schema end
-- 
