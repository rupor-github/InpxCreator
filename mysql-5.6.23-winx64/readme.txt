Place embedded mysql-5.6.23-winx64 includes and libraries here. 

I had to build libraries like that:
   cmake -G "Visual Studio 12 2013 Win64" ..\mysql-5.6.23\
   devenv MySQL.sln /build Debug
   devenv MySQL.sln /build Release

+---include
|   |   big_endian.h
|   |   byte_order_generic.h
|   |   byte_order_generic_x86.h
|   |   byte_order_generic_x86_64.h
|   |   decimal.h
|   |   errmsg.h
|   |   keycache.h
|   |   little_endian.h
|   |   mysql.h
|   |   mysqld_ername.h
|   |   mysqld_error.h
|   |   mysql_com.h
|   |   mysql_com_server.h
|   |   mysql_embed.h
|   |   mysql_time.h
|   |   mysql_version.h
|   |   my_alloc.h
|   |   my_attribute.h
|   |   my_byteorder.h
|   |   my_compiler.h
|   |   my_config.h
|   |   my_dbug.h
|   |   my_dir.h
|   |   my_getopt.h
|   |   my_global.h
|   |   my_list.h
|   |   my_net.h
|   |   my_pthread.h
|   |   my_sys.h
|   |   my_xml.h
|   |   m_ctype.h
|   |   m_string.h
|   |   plugin.h
|   |   plugin_audit.h
|   |   plugin_ftparser.h
|   |   plugin_validate_password.h
|   |   sql_common.h
|   |   sql_state.h
|   |   sslopt-case.h
|   |   sslopt-longopts.h
|   |   sslopt-vars.h
|   |   typelib.h
|   |   
|   \---mysql
|       |   client_authentication.h
|       |   client_plugin.h
|       |   client_plugin.h.pp
|       |   get_password.h
|       |   innodb_priv.h
|       |   plugin.h
|       |   plugin_audit.h
|       |   plugin_audit.h.pp
|       |   plugin_auth.h
|       |   plugin_auth.h.pp
|       |   plugin_auth_common.h
|       |   plugin_ftparser.h
|       |   plugin_ftparser.h.pp
|       |   plugin_validate_password.h
|       |   services.h
|       |   service_mysql_string.h
|       |   service_my_plugin_log.h
|       |   service_my_snprintf.h
|       |   service_thd_alloc.h
|       |   service_thd_wait.h
|       |   service_thread_scheduler.h
|       |   thread_pool_priv.h
|       |   
|       \---psi
|               mysql_file.h
|               mysql_idle.h
|               mysql_socket.h
|               mysql_stage.h
|               mysql_statement.h
|               mysql_table.h
|               mysql_thread.h
|               psi.h
|               
\---lib
    |   mysqlserver.lib
    |   
    \---debug
            mysqlserver.lib
