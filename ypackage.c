/* Libypk
 *
 * Copyright (c) 2011-2012 Ylmf OS
 *
 * Written by: 0o0<0o0zzyz@gmail.com>
 * Version: 0.1
 * Date: 2012.7.20
 */
#define LIBYPK 1
#include "ypackage.h"

int libypk_errno;

YPackageManager *packages_manager_init()
{
    char *config_file;
    YPackageManager *pm;

    pm = malloc( sizeof(YPackageManager) );

    if( !pm )
        return NULL;

    if( access( DB_NAME, R_OK ) )
    {
        free( pm );
        return NULL;
    }
    pm->db_name = strdup( DB_NAME );

    config_file = CONFIG_FILE;

    if( !access( config_file, R_OK ) )
    {
        pm->source_uri = util_get_config( config_file, "YPPATH_URI" );
        if( !pm->source_uri )
            pm->source_uri = strdup( DEFAULT_URI );

        pm->accept_repo = util_get_config( config_file, "ACCEPT_REPO" );
        if( !pm->accept_repo )
            pm->accept_repo = strdup( DEFAULT_REPO );

        pm->package_dest = util_get_config( config_file, "YPPATH_PKGDEST" );
        if( !pm->package_dest )
            pm->package_dest = strdup( DEFAULT_PKGDEST );

        pm->log = util_get_config( config_file, "LOG" );
        if( !pm->log )
            pm->log = strdup( LOG_FILE );
    }
    else
    {
        pm->source_uri = strdup( DEFAULT_URI );

        pm->accept_repo = strdup( DEFAULT_REPO );

        pm->package_dest = strdup( DEFAULT_PKGDEST );

        pm->log = strdup( LOG_FILE );
    }


    return pm;
}

void packages_manager_cleanup( YPackageManager *pm )
{
    if( !pm )
        return;

    if( pm->source_uri )
        free( pm->source_uri );

    if( pm->accept_repo )
        free( pm->accept_repo );

    if( pm->package_dest )
        free( pm->package_dest );

    if( pm->db_name )
        free( pm->db_name );

    if( pm->log )
        free( pm->log );

    free( pm );
}

YPackageManager *packages_manager_init2( int type )
{
    int             ret;
    char            *config_file;
    YPackageManager *pm;


    if( type == 1 )
    {
        ret = packages_read_lock();
    }
    else if( type == 2 )
    {
        ret = packages_write_lock();
    }
    else
    {
        libypk_errno = ARGS_INCORRECT;
        return NULL;
    }

    if( ret < 0 )
    {
        libypk_errno = LOCK_ERROR;
        return NULL;
    }

    pm = malloc( sizeof(YPackageManager) );

    if( !pm )
    {
        libypk_errno = OTHER_FAILURES;
        return NULL;
    }

    pm->lock_fd = ret;


    if( access( DB_NAME, R_OK ) )
    {
        libypk_errno = MISSING_DB;
        free( pm );
        return NULL;
    }
    pm->db_name = strdup( DB_NAME );

    config_file = CONFIG_FILE;
    if( !access( config_file, R_OK ) )
    {
        pm->source_uri = util_get_config( config_file, "YPPATH_URI" );
        if( !pm->source_uri )
            pm->source_uri = strdup( DEFAULT_URI );

        pm->accept_repo = util_get_config( config_file, "ACCEPT_REPO" );
        if( !pm->accept_repo )
            pm->accept_repo = strdup( DEFAULT_REPO );

        pm->package_dest = util_get_config( config_file, "YPPATH_PKGDEST" );
        if( !pm->package_dest )
            pm->package_dest = strdup( DEFAULT_PKGDEST );

        pm->log = util_get_config( config_file, "LOG" );
        if( !pm->log )
            pm->log = strdup( LOG_FILE );
    }
    else
    {
        pm->source_uri = strdup( DEFAULT_URI );

        pm->accept_repo = strdup( DEFAULT_REPO );

        pm->package_dest = strdup( DEFAULT_PKGDEST );

        pm->log = strdup( LOG_FILE );
    }

    return pm;
}

void packages_manager_cleanup2( YPackageManager *pm )
{
    if( !pm )
        return;

    packages_unlock( pm->lock_fd );

    if( pm->source_uri )
        free( pm->source_uri );

    if( pm->accept_repo )
        free( pm->accept_repo );

    if( pm->package_dest )
        free( pm->package_dest );

    if( pm->db_name )
        free( pm->db_name );

    if( pm->log )
        free( pm->log );

    free( pm );

}

int packages_upgrade_db( YPackageManager *pm )
{
    size_t              len, cur_version, new_version, begin;
    char                *line, tmp[11];
    FILE                *fp;
    DB                  db;

    if( !pm )
        return -1;

    if( access( DB_UPGRADE, R_OK ) )
        return -1;

    fp = fopen( DB_UPGRADE, "r" );
    if( !fp )
        return -1;

    cur_version = 0;
    new_version = 0;
    begin = 0;


    db_init( &db, pm->db_name, OPEN_WRITE );
    db_query( &db, "select db_version from config", NULL );
    if( db_fetch_assoc( &db ) )
    {
        cur_version = atoi( db_get_value_by_key( &db, "db_version" ) );
    }
    else
    {
        if( db_exec( &db, "alter table config add column db_version text default '0'", NULL ) != SQLITE_DONE )
        {
            goto exception_handler;
        }
    }

    db_exec( &db, "begin", NULL );  

    line = NULL;
    while( getline( &line, &len, fp ) != -1 )
    {
        if( !strncmp( line, "Version:", 8 ) )
        {
            strncpy( tmp, line+8, 10 );
            tmp[10] = 0;
            new_version = atoi( tmp );
            if( new_version > cur_version )
            {
                cur_version = new_version; 
                begin = 1;
            }
        }
        else if( !strncmp( line, "End", 3 ) )
        {
            if( begin )
            {
                if( db_exec( &db, "update config set db_version=?", tmp, NULL ) != SQLITE_DONE )
                {
                    goto exception_handler;
                }

                begin = 0;
            }
        }
        else
        {
            if( begin && strlen( line ) > 3 )
            {
                //printf( "debug: exec %s\n", line );
                if( db_exec( &db, line, NULL ) != SQLITE_DONE )
                {
                    goto exception_handler;
                }
            }
        }

    }

    db_exec( &db, "end", NULL );  

    if( line )
        free( line );

    fclose( fp );

    db_close( &db );
    return 0;

exception_handler:
    db_exec( &db, "rollback", NULL );  
    db_close( &db );
    return -1;
}

int packages_lock( int type )
{
    int             fd;
    struct flock    lock;

    umask( 0 );
    fd = open( LOCK_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

    if( fd < 0 )
    {
        return -1;
    }

    lock.l_type = type == 2 ? F_WRLCK : F_RDLCK; 
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    lock.l_len = 0;

    if( fcntl( fd, F_SETLK, &lock ) == -1 )
    {
        close( fd );
        return -2;
    }

    return fd;
}

int packages_unlock( int fd )
{
    struct flock    lock;

    if( fd < 1 )
    {
        return -1;
    }

    lock.l_type = F_UNLCK;
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    lock.l_len = 0;

    if( fcntl( fd, F_SETLK, &lock ) == -1 )
    {
        close( fd );
        return -2;
    }

    close( fd );
    return 0;
}

int packages_check_update( YPackageManager *pm )
{
    int             timestamp, last_check;
    char            *target_url, *list_date_file;
    DownloadContent  content;

    last_check = packages_get_last_check_timestamp( pm );
    if( last_check == 1 )
        return 1;

    //download updates date
    list_date_file = UPDATE_DIR "/" LIST_DATE_FILE;
    target_url = util_strcat( pm->source_uri, "/", list_date_file, NULL );
    content.text = malloc(1);
    content.size = 0;
    if( get_content( target_url, &content ) != 0 )
    {
        free(content.text);
        free(target_url);
        target_url = NULL;
        return -1; 
    }

    util_rtrim( content.text, 0 );
    timestamp = atoi( content.text );

    //cmp date
    if( timestamp > last_check )
    {
        packages_set_last_check_timestamp( pm, timestamp );
        free(content.text);
        free(target_url);
        target_url = NULL;
        return 1; 
    }

    free(content.text);
    free(target_url);
    target_url = NULL;
    return 0;
}

int packages_import_local_data( YPackageManager *pm )
{
    int                 xml_ret, i;
    size_t              list_len;
    char                *sql, *sql_testing, *sql_history, *sql_data, *sql_testing_data, *sql_history_data, *sql_filelist, *idx, *data_key,*data_name, *data_format, *data_size, *data_install_size, *data_depend, *data_bdepend, *data_recommended, *data_conflict, *file_path, *file_path_sub, *list_line;
    char                 *saveptr, *package_name, *is_desktop, *version, *repo, *install_time, *install_size, *file_type, *file_file, *file_size, *file_perms, *file_uid, *file_gid, *file_mtime, *file_extra, *last_update;
    char                *xml_attrs[] = {"name", "type", "lang", "id", NULL};
    struct stat         statbuf;
    struct dirent       *entry, *entry_sub;
    DIR                 *dir, *dir_sub;
    FILE                *fp;
    XMLReaderHandle   xml_handle;
    DB                  db;

    //init
    db_init( &db, pm->db_name, OPEN_WRITE );
    db_exec( &db, "begin", NULL );  

    printf( "Start ...\n" );


    printf( "Import universe ...\n" );
    //import universe
    reader_open( LOCAL_UNIVERSE,  &xml_handle );
    sql = "replace into universe (name, generic_name, is_desktop, category, arch, version, priority, install, license, homepage, repo, size, sha, build_date, packager, uri, description, data_count) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sql_testing = "replace into universe_testing (name, generic_name, is_desktop, category, arch, version, priority, install, license, homepage, repo, size, sha, build_date, packager, uri, description, data_count) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sql_history = "replace into universe_history (name, generic_name, is_desktop, category, arch, version, priority, install, license, homepage, repo, size, sha, build_date, packager, uri, description, data_count) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    while( ( xml_ret = reader_fetch_a_row( &xml_handle, 1, xml_attrs ) ) == 1 )
    {
        is_desktop = reader_get_value( &xml_handle, "genericname|desktop|keyword|en" );
        package_name = reader_get_value2( &xml_handle, "name" );
        version = reader_get_value2( &xml_handle, "version" );
        repo = reader_get_value( &xml_handle, "repo" );

        //universe
        if( repo && !strcmp( repo, "stable" ) )
        {
            db_exec( &db, sql,  
                    package_name, //name
                    is_desktop ? reader_get_value2( &xml_handle, "genericname|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "genericname|keyword|en" ), //generic_name
                    is_desktop ? "1" : "0", //desktop
                    reader_get_value2( &xml_handle, "category" ), //category
                    reader_get_value2( &xml_handle, "arch" ), //arch
                    version, //version
                    reader_get_value2( &xml_handle, "priority" ), //priority
                    reader_get_value2( &xml_handle, "install" ), //install
                    reader_get_value2( &xml_handle, "license" ), //license
                    reader_get_value2( &xml_handle, "homepage" ), //homepage
                    reader_get_value2( &xml_handle, "repo" ), //repo
                    reader_get_value2( &xml_handle, "size" ), //size
                    reader_get_value2( &xml_handle, "sha" ), //sha
                    reader_get_value2( &xml_handle, "build_date" ), //build_date
                    reader_get_value2( &xml_handle, "packager" ), //packager
                    reader_get_value2( &xml_handle, "uri" ), //uri
                    is_desktop ? reader_get_value2( &xml_handle, "description|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "description|keyword|en" ), //description
                    reader_get_value2( &xml_handle, "data_count" ), //data_count
                    NULL);
        }

        db_exec( &db, sql_testing,  
                package_name, //name
                is_desktop ? reader_get_value2( &xml_handle, "genericname|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "genericname|keyword|en" ), //generic_name
                is_desktop ? "1" : "0", //desktop
                reader_get_value2( &xml_handle, "category" ), //category
                reader_get_value2( &xml_handle, "arch" ), //arch
                version, //version
                reader_get_value2( &xml_handle, "priority" ), //priority
                reader_get_value2( &xml_handle, "install" ), //install
                reader_get_value2( &xml_handle, "license" ), //license
                reader_get_value2( &xml_handle, "homepage" ), //homepage
                reader_get_value2( &xml_handle, "repo" ), //repo
                reader_get_value2( &xml_handle, "size" ), //size
                reader_get_value2( &xml_handle, "sha" ), //sha
                reader_get_value2( &xml_handle, "build_date" ), //build_date
                reader_get_value2( &xml_handle, "packager" ), //packager
                reader_get_value2( &xml_handle, "uri" ), //uri
                is_desktop ? reader_get_value2( &xml_handle, "description|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "description|keyword|en" ), //description
                reader_get_value2( &xml_handle, "data_count" ), //data_count
                NULL);

        //universe_history
        db_exec( &db, sql_history,  
                package_name, //name
                is_desktop ? reader_get_value2( &xml_handle, "genericname|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "genericname|keyword|en" ), //generic_name
                is_desktop ? "1" : "0", //desktop
                reader_get_value2( &xml_handle, "category" ), //category
                reader_get_value2( &xml_handle, "arch" ), //arch
                version, //version
                reader_get_value2( &xml_handle, "priority" ), //priority
                reader_get_value2( &xml_handle, "install" ), //install
                reader_get_value2( &xml_handle, "license" ), //license
                reader_get_value2( &xml_handle, "homepage" ), //homepage
                reader_get_value2( &xml_handle, "repo" ), //repo
                reader_get_value2( &xml_handle, "size" ), //size
                reader_get_value2( &xml_handle, "sha" ), //sha
                reader_get_value2( &xml_handle, "build_date" ), //build_date
                reader_get_value2( &xml_handle, "packager" ), //packager
                reader_get_value2( &xml_handle, "uri" ), //uri
                is_desktop ? reader_get_value2( &xml_handle, "description|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "description|keyword|en" ), //description
                reader_get_value2( &xml_handle, "data_count" ), //data_count
                NULL);
    
        //universe_data
        if( repo && !strcmp( repo, "stable" ) )
        {
            db_exec( &db, "delete from universe_data where name=?", package_name, NULL );  
        }

        db_exec( &db, "delete from universe_testing_data where name=?", package_name, NULL );  

        db_exec( &db, "delete from universe_history_data where name=? and version=?", package_name, version, NULL );  

        sql_data = "insert into universe_data (name, version, data_name, data_format, data_size, data_install_size, data_depend, data_bdepend, data_recommended, data_conflict) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

        sql_testing_data = "insert into universe_testing_data (name, version, data_name, data_format, data_size, data_install_size, data_depend, data_bdepend, data_recommended, data_conflict) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

        sql_history_data = "insert into universe_history_data (name, version, data_name, data_format, data_size, data_install_size, data_depend, data_bdepend, data_recommended, data_conflict) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
        data_key = (char *)malloc( 32 );
        for( i = 0; ; i++ )
        {
            idx = util_int_to_str( i );
            if( !reader_get_value( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|name", NULL ) ) )
            {
                free( idx );
                break;
            }

            data_name = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|name", NULL ) );  
            data_format  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|format", NULL ) );   
            data_size = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|size", NULL ) );   
            data_install_size  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|install_size", NULL ) );
            data_depend = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|depend", NULL ) );    
            data_bdepend = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|bdepend", NULL ) );    
            data_recommended = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|recommended", NULL ) );    
            data_conflict  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|conflict", NULL ) );   

            if( repo && !strcmp( repo, "stable" ) )
            {
                db_exec( &db, sql_data,  
                        package_name, //name
                        version, //version
                        data_name, //data_name
                        data_format, //data_format
                        data_size, //data_size
                        data_install_size, //data_install_size
                        data_depend, //data_depend
                        data_bdepend, //data_bdepend
                        data_recommended, //data_recommended
                        data_conflict, //data_conflict
                        NULL);
            }

            db_exec( &db, sql_testing_data,  
                    package_name, //name
                    version, //version
                    data_name, //data_name
                    data_format, //data_format
                    data_size, //data_size
                    data_install_size, //data_install_size
                    data_depend, //data_depend
                    data_bdepend, //data_bdepend
                    data_recommended, //data_recommended
                    data_conflict, //data_conflict
                    NULL);

            db_exec( &db, sql_history_data,  
                    package_name, //name
                    version, //version
                    data_name, //data_name
                    data_format, //data_format
                    data_size, //data_size
                    data_install_size, //data_install_size
                    data_depend, //data_depend
                    data_bdepend, //data_bdepend
                    data_recommended, //data_recommended
                    data_conflict, //data_conflict
                    NULL);
            free( idx );
        }
        free( data_key );
    }
    reader_cleanup( &xml_handle );

    //world
    printf( "Import world ...\n" );
    reader_open( LOCAL_WORLD,  &xml_handle );
    sql = "replace into world (name, generic_name, is_desktop, category, arch, version, priority, install, license, homepage, repo, size, sha, build_date, packager, uri, description, data_count) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    //char * sql_test;
    //sql_test = "replace into world (name, generic_name, is_desktop, category, arch, version, priority, install, license, homepage, repo, size, sha, build_date, packager, uri, description, data_count) values ('%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s');";

    while( ( xml_ret = reader_fetch_a_row( &xml_handle, 1, xml_attrs ) ) == 1 )
    {
        is_desktop = reader_get_value( &xml_handle, "genericname|desktop|keyword|en" );
        package_name = reader_get_value2( &xml_handle, "name" );
        version = reader_get_value2( &xml_handle, "version" );

        //world
        db_exec( &db, sql,  
                package_name, //name
                is_desktop ? reader_get_value2( &xml_handle, "genericname|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "genericname|keyword|en" ), //generic_name
                is_desktop ? "1" : "0", //desktop
                reader_get_value2( &xml_handle, "category" ), //category
                reader_get_value2( &xml_handle, "arch" ), //arch
                version, //version
                reader_get_value2( &xml_handle, "priority" ), //priority
                reader_get_value2( &xml_handle, "install" ), //install
                reader_get_value2( &xml_handle, "license" ), //license
                reader_get_value2( &xml_handle, "homepage" ), //homepage
                reader_get_value2( &xml_handle, "repo" ), //repo
                reader_get_value2( &xml_handle, "size" ), //size
                reader_get_value2( &xml_handle, "sha" ), //sha
                reader_get_value2( &xml_handle, "build_date" ), //build_date
                reader_get_value2( &xml_handle, "packager" ), //packager
                reader_get_value2( &xml_handle, "uri" ), //uri
                is_desktop ? reader_get_value2( &xml_handle, "description|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "description|keyword|en" ), //description
                reader_get_value2( &xml_handle, "data_count" ), //data_count
                NULL);

        //world_data
        db_exec( &db, "delete from world_data where name=?", package_name, NULL );  

        sql_data = "insert into world_data (name, version, data_name, data_format, data_size, data_install_size, data_depend, data_bdepend, data_recommended, data_conflict) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
        data_key = (char *)malloc( 32 );
        for( i = 0; ; i++ )
        {
            idx = util_int_to_str( i );
            if( !reader_get_value( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|name", NULL ) ) )
            {
                free( idx );
                break;
            }

            data_name = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|name", NULL ) );  
            data_format  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|format", NULL ) );   
            data_size = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|size", NULL ) );   
            data_install_size  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|install_size", NULL ) );
            data_depend = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|depend", NULL ) );    
            data_bdepend = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|bdepend", NULL ) );    
            data_recommended = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|recommended", NULL ) );    
            data_conflict  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|conflict", NULL ) );   

            db_exec( &db, sql_data,  
                    package_name, //name
                    version, //version
                    data_name, //data_name
                    data_format, //data_format
                    data_size, //data_size
                    data_install_size, //data_install_size
                    data_depend, //data_depend
                    data_bdepend, //data_bdepend
                    data_recommended, //data_recommended
                    data_conflict, //data_conflict
                    NULL);
            free( idx );
        }
        free( data_key );

        //update universe
        db_exec( &db, "update universe set installed=1  where name=?", package_name, NULL );  
        db_exec( &db, "update universe_testing set installed=1  where name=?", package_name, NULL );  
    }
    reader_cleanup( &xml_handle );


    //world_file
    dir = opendir( PACKAGE_DB_DIR );
    if( !dir )
        return -1;

    while( (entry = readdir( dir )) )
    {
        if( !strcmp( entry->d_name, "." ) || !strcmp( entry->d_name, ".." ) )
        {
            continue;
        }

        package_name = entry->d_name;
        //printf( "%s\n", package_name );
        file_path = util_strcat( PACKAGE_DB_DIR, "/", entry->d_name, NULL );
        if( !stat( file_path, &statbuf ) && S_ISDIR( statbuf.st_mode ) )
        {
            //sub dir
            dir_sub = opendir( file_path );
            if( dir_sub )
            {
                while( (entry_sub = readdir( dir_sub )) )
                {
                    if( strstr(entry_sub->d_name, ".desc") )
                    {
                        file_path_sub = util_strcat( file_path, "/", entry_sub->d_name, NULL );
                        install_time = util_get_config( file_path_sub, "INSTALL_TIME" );
                        install_size = util_get_config( file_path_sub, "INSTALL_SIZE" );
                        if( install_time )
                        {
                            db_exec( &db, "update world set install_time=?, size=? where name=?", install_time, install_size ? install_size : "0", package_name, NULL );  
                        }
                        else
                        {
                            db_exec( &db, "update world set install_time=strftime('%s','now'), size=? where name=?", package_name,  install_size ? install_size : "0", NULL );  
                        }

                        if( install_time )
                            free( install_time );

                        if( install_size )
                            free( install_size );

                        free( file_path_sub );
                    }

                    if( strstr(entry_sub->d_name, ".list") )
                    {
                        file_path_sub = util_strcat( file_path, "/", entry_sub->d_name, NULL );

                        db_exec( &db, "delete from world_file where name=?", package_name, NULL );  
                        sql_filelist = "insert into world_file (name, type, file, size, perms, uid, gid, mtime, extra) values (?, ?, ?, ?, ?, ?, ?, ?, ?)"; 

                        fp = fopen( file_path_sub, "r" );
                        list_line = NULL;
                        while( getline( &list_line, &list_len, fp ) != -1 )
                        {
                            if( list_line[0] != 'I' )
                            {
                                file_type = strtok_r( list_line, " ,", &saveptr );
                                file_file = strtok_r( NULL, " ,", &saveptr );
                                file_size = strtok_r( NULL, " ,", &saveptr );
                                file_perms = strtok_r( NULL, " ,", &saveptr );
                                file_uid = strtok_r( NULL, " ,", &saveptr );
                                file_gid = strtok_r( NULL, " ,", &saveptr );
                                file_mtime = strtok_r( NULL, " ,", &saveptr );
                                file_extra = strtok_r( NULL, " ,", &saveptr );

                                db_exec( &db, sql_filelist, 
                                        package_name,
                                        file_type ? file_type : "",
                                        file_file ? file_file : "",
                                        file_size ? file_size : "",
                                        file_perms ? file_perms : "",
                                        file_uid ? file_uid : "",
                                        file_gid ? file_gid : "",
                                        file_mtime ? file_mtime : "",
                                        file_extra ? file_extra : "",
                                        NULL );
                            }

                        }
                        if( list_line )
                            free( list_line );

                        fclose( fp );
                        free( file_path_sub );
                    }
                }
                closedir( dir_sub );
            }
        }
        free( file_path );
    }
    closedir( dir );

    //update config
    db_query( &db, "select max(build_date) from universe_testing", NULL );
    db_fetch_num( &db );
    last_update = db_get_value_by_index( &db, 0 );
    db_cleanup( &db );
    db_exec( &db, "update config set last_update=?, last_check=?", last_update, last_update, NULL );  

    db_exec( &db, "end", NULL );  
    printf( "Done!\n" );
    //clean up
    db_close( &db );
    return 0;
}

int packages_update( YPackageManager *pm, ypk_progress_callback cb, void *cb_arg )
{
    int             timestamp, last_update, cnt;
    char            *target_url, *list_file, *list_line, xml_file[32],  sum[48];
    DownloadContent content;

    if( !pm )
        return -1;


    //download updates list
    if( cb )
    {
        cb( cb_arg, "", 0, 1, "Synchronizing new packages list ...\n" );
        cb( cb_arg, "", 1, -1, "Initializing ...\n" );
    }

    list_file = UPDATE_DIR "/" LIST_FILE;
    target_url = util_strcat( pm->source_uri, "/", list_file, NULL );
    content.text = malloc(1);
    content.size = 0;
    if( get_content(target_url, &content) != 0 )
    {
        free(content.text);
        free(target_url);
        target_url = NULL;
        if( cb )
        {
            cb( cb_arg, "", 1, 1, "Connection fails, check your network and configuration.\n" );
        }
        return -1; 
    }

    if( cb )
    {
        cb( cb_arg, "", 1, 1, NULL );
    }

    list_line = util_mem_gets( content.text );
    last_update = packages_get_last_update_timestamp( pm );
    cnt = 0;
    while( list_line )
    {
        util_rtrim( list_line, 0 );
        memset( xml_file, '\0', 32 );
        memset( sum, '\0', 48 );
        if( sscanf( list_line, "%s %d %s", xml_file, &timestamp, sum ) == 3 )
        {
            if( timestamp > last_update )
            {
                packages_update_single_xml( pm, xml_file, sum, cb, cb_arg  );
                last_update = timestamp;
                cnt++;
            }
        }
        free( list_line );
        list_line = util_mem_gets( NULL );
    }
    packages_set_last_update_timestamp( pm, last_update );

    free(content.text);
    free(target_url);
    target_url = NULL;

    if( cb )
    {
        cb( cb_arg, "", 9, 1, NULL );
    }

    return cnt;
}

/*
 * packages_get_upgrade_list
 */
YPackageChangeList *packages_get_upgrade_list( YPackageManager *pm )
{    
    int             pkg_count, len, i, wildcards[] = { 2, 0 };
    char            *package_name, *version, *can_upgrade;
    char            *keys[] = { "can_update", NULL }, *keywords[] = { "%1", NULL };
    YPackageList    *pkg_list;
    YPackageChangeList     *list, *cur_pkg;

    list = NULL;

    pkg_count = packages_get_count( pm,  keys, keywords, wildcards, 0 );

    if( pkg_count > 0 )
    {
        pkg_list = packages_get_list( pm, pkg_count, 0, keys, keywords, wildcards, 0 );
        if( pkg_list )
        {
            for( i = 0; i < pkg_list->cnt; i++ )
            {
                package_name = packages_get_list_attr( pkg_list, i, "name" );
                version = packages_get_list_attr( pkg_list, i, "version" );
                can_upgrade = packages_get_list_attr( pkg_list, i, "can_update" );
                cur_pkg =  (YPackageChangeList *)malloc( sizeof( YPackageChangeList ) );
                len = strlen( package_name );
                cur_pkg->name = (char *)malloc( len + 1 );
                strncpy( cur_pkg->name, package_name, len );
                cur_pkg->name[len] = 0;

                if( version)
                {
                    len = strlen( version );
                    cur_pkg->version = (char *)malloc( len + 1 );
                    strncpy( cur_pkg->version, version, len );
                    cur_pkg->version[len] = 0;
                }
                else
                {
                    version = NULL;
                }

                if( can_upgrade && can_upgrade[0] )
                {
                    cur_pkg->type = can_upgrade[0] == '1' ? 4 : 5;
                }
                else
                {
                    cur_pkg->type = 4;
                }

                cur_pkg->prev = list;
                list = cur_pkg;
            }
            packages_free_list( pkg_list );
        }
    }

    return list;

}

int packages_upgrade_list( YPackageManager *pm, YPackageChangeList *list, ypk_progress_callback cb, void *cb_arg   )
{
    YPackageChangeList    *cur_pkg;

    if( !list )
        return -1;


    while( list )
    {
        cur_pkg = list;
        
        //printf("upgrading %s ...\n", cur_pkg->name );
        packages_install_package( pm, cur_pkg->name, cur_pkg->version, cb, cb_arg  );
        list = list->prev;
    }
    return 0;
}

void packages_free_upgrade_list( YPackageChangeList *list )
{
    YPackageChangeList    *cur_pkg;

    if( !list )
        return;


    while( list )
    {
        cur_pkg = list;
        list = list->prev;
        free( cur_pkg->name );
        if( cur_pkg->version )
            free( cur_pkg->version );
        free( cur_pkg );
    }
}

int packages_update_single_xml( YPackageManager *pm, char *xml_file, char *sum, ypk_progress_callback cb, void *cb_arg )
{
    int                 i, xml_ret, db_ret, cmp_ret, do_replace;
    char                *xml_sha, *target_url, *msg, *sql, *sql_data, *sql_testing, *sql_testing_data, *sql_history, *sql_history_data, *package_name, *version, *is_desktop, *repo, *delete, *idx, *data_key,*data_name, *data_format, *data_size, *data_install_size, *data_depend, *data_bdepend, *data_recommended, *data_conflict, *data_replace, *installed, *old_version, *old_repo, *can_update, *can_update2;
    char                tmp_bz2[] = "/tmp/tmp_bz2.XXXXXX";
    char                tmp_xml[] = "/tmp/tmp_xml.XXXXXX";
    char                *xml_attrs[] = {"name", "type", "lang", "id", NULL};
    DownloadFile        file;
    XMLReaderHandle     xml_handle;
    DB                  db;

                    
    if( !pm || !xml_file || !sum )
        return -1;


    //donload xml
    target_url = util_strcat( pm->source_uri, "/", UPDATE_DIR, "/", xml_file, NULL );

    if( cb )
    {
        msg = util_strcat( "Get: ", target_url, NULL );

        cb( cb_arg, "*", 2, -1, msg ? msg : NULL );

        if( msg )
        {
            free( msg );
            msg = NULL;
        }
    }

    mkstemp(tmp_bz2);
    file.file = tmp_bz2;
    file.stream = NULL;
    file.cb = NULL;
    file.cb_arg = NULL;
    if( download_file( target_url, &file ) != 0 )
    {
        if( cb )
        {
            cb( cb_arg, "Download failed.\n", 2, 1, NULL );
        }
        remove(tmp_bz2);
        return -1; 
    }
    fclose(file.stream);
    free(target_url);
    target_url = NULL;


    //compare sum
    xml_sha = util_sha1( tmp_bz2 );
    if( strncmp( xml_sha, sum, 41 ) != 0 )
    {
        if( cb )
        {
            cb( cb_arg, "Checksum mismatched.\n", 2, 1, NULL );
        }
        return -1;
    }

    if( cb )
    {
        cb( cb_arg, "*", 2, 1, NULL );
    }

    //unzip
    if( cb )
    {
        msg = strdup( "Extracting information ..." );

        cb( cb_arg, "*", 4, -1, msg ? msg : NULL );

        if( msg )
        {
            free( msg );
            msg = NULL;
        }
    }

    mkstemp(tmp_xml);
    if( archive_extract_file( file.file, "update.xml", tmp_xml ) == -1 )
    {
        remove(tmp_bz2);
        remove(tmp_xml);
        return -1;
    }

    if( cb )
    {
        cb( cb_arg, "*", 4, 1, NULL );
    }


    if( cb )
    {
        msg = strdup( "Updating database ..." );

        cb( cb_arg, "*", 8, -1, msg ? msg : NULL );

        if( msg )
        {
            free( msg );
            msg = NULL;
        }
    }

    //parse xml
    reader_open( tmp_xml,  &xml_handle );
    db_init( &db, pm->db_name, OPEN_WRITE );
    db_exec( &db, "begin", NULL );  

    sql = "replace into universe (name, exec, generic_name, is_desktop, category, arch, version, priority, install, license, homepage, repo, size, sha, build_date, packager, uri, description, data_count, can_update, installed ) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sql_testing = "replace into universe_testing (name, exec, generic_name, is_desktop, category, arch, version, priority, install, license, homepage, repo, size, sha, build_date, packager, uri, description, data_count, can_update, installed ) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";


    sql_history = "replace into universe_history (name, exec, generic_name, is_desktop, category, arch, version, priority, install, license, homepage, repo, size, sha, build_date, packager, uri, description, data_count) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sql_data = "insert into universe_data (name, version, data_name, data_format, data_size, data_install_size, data_depend, data_bdepend, data_recommended, data_conflict, data_replace) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sql_testing_data = "insert into universe_testing_data (name, version, data_name, data_format, data_size, data_install_size, data_depend, data_bdepend, data_recommended, data_conflict, data_replace) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sql_history_data = "insert into universe_history_data (name, version, data_name, data_format, data_size, data_install_size, data_depend, data_bdepend, data_recommended, data_conflict, data_replace) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    while( ( xml_ret = reader_fetch_a_row( &xml_handle, 1, xml_attrs ) ) == 1 )
    {
        package_name = reader_get_value2( &xml_handle, "name" );
        version = reader_get_value2( &xml_handle, "version" );
        delete = reader_get_value( &xml_handle, "delete" );

        if( delete )
        {
            if( strncmp( delete, "all", 3 ) == 0 )
            {
                db_exec( &db, "delete from universe where name=?", package_name, NULL );  

                db_exec( &db, "delete from universe_data where name=?", package_name, NULL );  

                db_exec( &db, "delete from universe_testing where name=?", package_name, NULL );  

                db_exec( &db, "delete from universe_testing_data where name=?", package_name, NULL );  

                db_exec( &db, "delete from universe_history where name=?", package_name, NULL );  

                db_exec( &db, "delete from universe_history_data where name=?", package_name, NULL );  
            }
            else if( strncmp( delete, "single", 6 ) == 0 )
            {
                db_exec( &db, "delete from universe where name=? and version=?", package_name, version, NULL );  

                db_exec( &db, "delete from universe_data where name=? and version=?", package_name, version, NULL );  

                db_exec( &db, "delete from universe_testing where name=? and version=?", package_name, version, NULL );  

                db_exec( &db, "delete from universe_testing_data where name=? and version=?", package_name, version, NULL );  

                db_exec( &db, "delete from universe_history where name=? and version=?", package_name, version, NULL );  

                db_exec( &db, "delete from universe_history_data where name=? and version=?", package_name, version, NULL );  
            }
        }
        else
        {
            is_desktop = reader_get_value( &xml_handle, "genericname|desktop|keyword|en" );
            repo = reader_get_value( &xml_handle, "repo" );

            //get original value
            do_replace = 0;
            can_update = "0";
            can_update2 = "0";
            installed = "0";

            db_query( &db, "select version, repo from world where name=?", package_name, NULL);
            if( db_fetch_assoc( &db ) )
            {
                old_version = db_get_value_by_key( &db, "version" );
                old_repo = db_get_value_by_key( &db, "repo" );

                if( version && (strlen( version ) > 0) && old_version && (strlen( old_version ) > 0)  )
                {
                        cmp_ret = packages_compare_version( version, old_version );
                        if( cmp_ret == 1 )
                        {
                            can_update = "1"; //can upgrade
                            can_update2 = "1"; //can upgrade
                        }
                        else if( cmp_ret == -1 )
                        {
                            if( repo && old_repo )
                            { 
                                if( old_repo[0] == 't' )
                                {
                                    if( repo[0] == 's' )
                                    {
                                        can_update = "-1";
                                    }
                                }
                                else
                                {
                                    can_update2 = "-1";
                                }

                            }
                        }
                }

                installed = "1";
                do_replace = 1;
                db_cleanup( &db );
            }
            else
            {
                do_replace = 1;
            }

            //universe
            if( do_replace )
            {

                if( repo && !strcmp( repo, "stable" ) )
                {
                    db_ret = db_exec( &db, sql,  
                            package_name, //name
                            reader_get_value2( &xml_handle, "exec" ), //exec
                            is_desktop ? reader_get_value2( &xml_handle, "genericname|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "genericname|keyword|en" ), //generic_name
                            is_desktop ? "1" : "0", //desktop
                            reader_get_value2( &xml_handle, "category" ), //category
                            reader_get_value2( &xml_handle, "arch" ), //arch
                            version, //version
                            reader_get_value2( &xml_handle, "priority" ), //priority
                            reader_get_value2( &xml_handle, "install" ), //install
                            reader_get_value2( &xml_handle, "license" ), //license
                            reader_get_value2( &xml_handle, "homepage" ), //homepage
                            reader_get_value2( &xml_handle, "repo" ), //repo
                            reader_get_value2( &xml_handle, "size" ), //size
                            reader_get_value2( &xml_handle, "sha" ), //sha
                            reader_get_value2( &xml_handle, "build_date" ), //build_date
                            reader_get_value2( &xml_handle, "packager" ), //packager
                            reader_get_value2( &xml_handle, "uri" ), //uri
                            is_desktop ? reader_get_value2( &xml_handle, "description|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "description|keyword|en" ), //description
                            reader_get_value2( &xml_handle, "data_count" ), //data_count
                            can_update,
                            installed,
                            NULL);
                }

                db_ret = db_exec( &db, sql_testing,  
                        package_name, //name
                        reader_get_value2( &xml_handle, "exec" ), //exec
                        is_desktop ? reader_get_value2( &xml_handle, "genericname|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "genericname|keyword|en" ), //generic_name
                        is_desktop ? "1" : "0", //desktop
                        reader_get_value2( &xml_handle, "category" ), //category
                        reader_get_value2( &xml_handle, "arch" ), //arch
                        version, //version
                        reader_get_value2( &xml_handle, "priority" ), //priority
                        reader_get_value2( &xml_handle, "install" ), //install
                        reader_get_value2( &xml_handle, "license" ), //license
                        reader_get_value2( &xml_handle, "homepage" ), //homepage
                        reader_get_value2( &xml_handle, "repo" ), //repo
                        reader_get_value2( &xml_handle, "size" ), //size
                        reader_get_value2( &xml_handle, "sha" ), //sha
                        reader_get_value2( &xml_handle, "build_date" ), //build_date
                        reader_get_value2( &xml_handle, "packager" ), //packager
                        reader_get_value2( &xml_handle, "uri" ), //uri
                        is_desktop ? reader_get_value2( &xml_handle, "description|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "description|keyword|en" ), //description
                        reader_get_value2( &xml_handle, "data_count" ), //data_count
                        can_update2,
                        installed,
                        NULL);

                db_ret = db_exec( &db, sql_history,  
                        package_name, //name
                        reader_get_value2( &xml_handle, "exec" ), //exec
                        is_desktop ? reader_get_value2( &xml_handle, "genericname|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "genericname|keyword|en" ), //generic_name
                        is_desktop ? "1" : "0", //desktop
                        reader_get_value2( &xml_handle, "category" ), //category
                        reader_get_value2( &xml_handle, "arch" ), //arch
                        version, //version
                        reader_get_value2( &xml_handle, "priority" ), //priority
                        reader_get_value2( &xml_handle, "install" ), //install
                        reader_get_value2( &xml_handle, "license" ), //license
                        reader_get_value2( &xml_handle, "homepage" ), //homepage
                        reader_get_value2( &xml_handle, "repo" ), //repo
                        reader_get_value2( &xml_handle, "size" ), //size
                        reader_get_value2( &xml_handle, "sha" ), //sha
                        reader_get_value2( &xml_handle, "build_date" ), //build_date
                        reader_get_value2( &xml_handle, "packager" ), //packager
                        reader_get_value2( &xml_handle, "uri" ), //uri
                        is_desktop ? reader_get_value2( &xml_handle, "description|desktop|keyword|en" ) : reader_get_value2( &xml_handle, "description|keyword|en" ), //description
                        reader_get_value2( &xml_handle, "data_count" ), //data_count
                        NULL);
            
                //universe_data
                if( repo && !strcmp( repo, "stable" ) )
                {
                    db_exec( &db, "delete from universe_data where name=?", package_name, NULL );  
                }

                db_exec( &db, "delete from universe_testing_data where name=?", package_name, NULL );  

                db_exec( &db, "delete from universe_history_data where name=? and version=?", package_name, version, NULL );  

                data_key = (char *)malloc( 32 );
                for( i = 0; ; i++ )
                {
                    idx = util_int_to_str( i );
                    if( !reader_get_value( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|name", NULL ) ) )
                    {
                        free( idx );
                        break;
                    }

                    data_name = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|name", NULL ) );  
                    data_format  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|format", NULL ) );   
                    data_size = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|size", NULL ) );   
                    data_install_size  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|install_size", NULL ) );
                    data_depend = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|depend", NULL ) );    
                    data_bdepend = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|bdepend", NULL ) );    
                    data_recommended = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|recommended", NULL ) );    
                    data_conflict  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|conflict", NULL ) );   
                    data_replace  = reader_get_value2( &xml_handle, util_strcat2( data_key, 32, "data|", idx, "|replace", NULL ) );   

                    if( repo && !strcmp( repo, "stable" ) )
                    {
                        db_exec( &db, sql_data,  
                                package_name, //name
                                version, //version
                                data_name, //data_name
                                data_format, //data_format
                                data_size, //data_size
                                data_install_size, //data_install_size
                                data_depend, //data_depend
                                data_bdepend, //data_bdepend
                                data_recommended, //data_recommended
                                data_conflict, //data_conflict
                                data_replace, //data_replace
                                NULL);
                    }

                    db_exec( &db, sql_testing_data,  
                            package_name, //name
                            version, //version
                            data_name, //data_name
                            data_format, //data_format
                            data_size, //data_size
                            data_install_size, //data_install_size
                            data_depend, //data_depend
                            data_bdepend, //data_bdepend
                            data_recommended, //data_recommended
                            data_conflict, //data_conflict
                            data_replace, //data_replace
                            NULL);

                    db_exec( &db, sql_history_data,  
                            package_name, //name
                            version, //version
                            data_name, //data_name
                            data_format, //data_format
                            data_size, //data_size
                            data_install_size, //data_install_size
                            data_depend, //data_depend
                            data_bdepend, //data_bdepend
                            data_recommended, //data_recommended
                            data_conflict, //data_conflict
                            data_replace, //data_replace
                            NULL);

                    free( idx );
                }//for
                free( data_key );
            }//if

        }
    }//while

    db_ret = db_exec( &db, "commit", NULL );  
    if( db_ret == SQLITE_BUSY )
    {
        db_ret = db_exec( &db, "commit", NULL );  
        //printf( "db_exec commit:%d\n", db_ret);
    }

    if( db_ret == SQLITE_BUSY )
    {
        db_ret = db_exec( &db, "rollback", NULL );  
        //printf( "rollback, db_ret:%d\n", db_ret );
    }
    //clean up
    db_close( &db );
    reader_cleanup( &xml_handle );

    if( cb )
    {
        cb( cb_arg, "*", 8, 1, NULL );
    }

    remove(tmp_bz2);
    remove(tmp_xml);
    return 0;
}

int packages_get_last_check_timestamp( YPackageManager *pm )
{
    int     last_check, has_new;
    DB      db;

    if( !pm )
        return -1;

    db_init( &db, pm->db_name, OPEN_READ );
    db_query( &db, "select has_new, last_check from config limit 1", NULL );
    if( db_fetch_row( &db, RESULT_NUM ) == 0 )
    {
        db_close( &db );
        return -1;
    }

    has_new = atoi( db.crow[0] );
    if( has_new )
    {
        db_close( &db );
        return 1;
    }

    last_check = atoi( db.crow[1] );
    db_close( &db );
    return last_check;
}


int packages_set_last_check_timestamp( YPackageManager *pm, int last_check )
{
    int     ret = -1;
    char    *timestamp;
    DB      db;

    if( !pm || last_check < 0 )
        return ret;

    db_init( &db, pm->db_name, OPEN_WRITE );
    timestamp = util_int_to_str( last_check );
    if( db_exec( &db, "update config set has_new = 1, last_check = ?", timestamp, NULL ) )
    {
        ret = 0;
    }

    free( timestamp );
    db_close( &db );
    return ret;
}

int packages_get_last_update_timestamp( YPackageManager *pm )
{
    DB      db;
    int     last_update;

    if( !pm )
        return -1;

    db_init( &db, pm->db_name, OPEN_READ );
    db_query( &db, "select last_update from config limit 1", NULL );
    if( db_fetch_row( &db, RESULT_NUM ) == 0 )
    {
        db_close( &db );
        return -1;
    }
    last_update = atoi( db.crow[0] );
    db_close(&db);

    return last_update;
}

int packages_set_last_update_timestamp( YPackageManager *pm, int last_update )
{
    DB      db;
    char    timestamp[11];

    if( !pm || last_update < 0 )
        return -1;

    db_init( &db, pm->db_name, OPEN_WRITE );
    snprintf( timestamp, 11, "%d", last_update);
    timestamp[10] = '\0';
    if( db_exec( &db, "update config set has_new = 0, last_update = ?", timestamp, NULL ) )
    {
        db_close( &db );
        return 0;
    }

    db_close( &db );
    return -1;
}

int packages_get_count( YPackageManager *pm, char *keys[], char *keywords[], int wildcards[], int installed  )
{
    int     count, repo_testing;
    char    *sql, *where_str, *table, *tmp;
    DB      db;

    if( !pm )
        return -1;

    if( pm->accept_repo && !strcmp( pm->accept_repo, "testing" ) )
        repo_testing =1;
    else
        repo_testing =0;

    table = installed ? "world" : repo_testing ? "universe_testing" : "universe";

    db_init( &db, pm->db_name, OPEN_READ );
    if( !keys || !keywords || !wildcards || !(*keys) || !(*keywords) || !(*wildcards) )
    {
        sql = util_strcat( "select count(*) from ", table, NULL );
        db_query( &db, sql, NULL);
        free( sql );
    }
    else
    {
        where_str = NULL;

        while( *keys && *keywords && *wildcards )
        {
            tmp = NULL;
            if( (*keywords) && ((*keys)[0] == '*') && (*wildcards == 2) )
            {
                if( where_str )
                    tmp = where_str;

                where_str = util_strcat( tmp ? tmp : "", tmp ? " and " : "",  "(", table, ".name like '%", *keywords, "%' or ", table, ".generic_name like  '%", *keywords, "%' or ", table, ".description like  '%", *keywords, "%' or keywords.kw_name like  '%", *keywords, "%' or keywords.kw_generic_name like  '%", *keywords, "%' or keywords.kw_fullname like  '%", *keywords, "%' or keywords.kw_comment like '%", *keywords, "%')", NULL );
                if( tmp )
                    free( tmp );
            }
            else if( (*keywords) && ((*keys)[0] == '*') && (*wildcards == 1) )
            {
                if( where_str )
                    tmp = where_str;

                where_str = util_strcat( tmp ? tmp : "", tmp ? " and " : "",  "(" , table, ".name = '", *keywords, "' or ", table, ".generic_name = '", *keywords, "' or keywords.kw_name = '", *keywords, "' or keywords.kw_generic_name = '", *keywords, "' or keywords.kw_fullname = '", *keywords, "')", NULL );
                if( tmp )
                    free( tmp );
            }
            else if( *keywords && *keys && *wildcards )
            {
                if( where_str )
                    tmp = where_str;

                if( strncmp( *keys, "kw_", 3 ) && strcmp( *keys, "language" ) )
                {
                    where_str = util_strcat( tmp ? tmp : "", tmp ? " and (" : "(", table, ".", *keys, *wildcards == 2 ? " like '%" : " = '", *keywords, *wildcards == 2 ? "%')" : "')", NULL );
                }
                else
                {
                    where_str = util_strcat( tmp ? tmp : "", tmp ? " and (keywords." : "(keywords.", *keys, *wildcards == 2 ? " like '%" : " = '", *keywords, *wildcards == 2 ? "%')" : "')", NULL );
                }

                if( tmp )
                    free( tmp );
            }
            else
            {
                break;
            }
            keys++;
            keywords++;
            wildcards++;
        }

        if( where_str )
        {
            sql = util_strcat( "select count(distinct ", table, ".name) from ", table, " left join keywords on ", table, ".name=keywords.name where ", where_str, NULL );
            db_query( &db, sql, NULL );
            //printf( "%s\n", sql);

            free( where_str );
            free( sql );
        }
    }

    db_fetch_num( &db );
    count = atoi( db_get_value_by_index( &db, 0 ) );
    db_close( &db );

    return count;
}

int packages_has_installed( YPackageManager *pm, char *name, char *version )
{
    int     count, ret;
    char    *version_installed;
    DB      db;

    if( !pm || !name )
        return -1;

    db_init( &db, pm->db_name, OPEN_READ );

    if( version )
    {
        version_installed = NULL;

        db_query( &db, "select version from world where name=?", name, NULL);

        if( db_fetch_num( &db ) )
        {
            version_installed = db_get_value_by_index( &db, 0 );
            if( version[0] == '>' || version[0] == '=' || version[0] == '!' || version[0] == '<')
            {
                if( version[1] == '=' )
                {
                    ret = packages_compare_version( version_installed, version + 2 );
                    switch( version[0] )
                    {
                        case '>':
                            ret = ret != -1;
                            break;

                        case '!':
                            ret = ret != 0;
                            break;

                        case '<':
                            ret = ret != 1;
                            break;

                        default:
                            ret = ret == 0;
                    }
                }
                else
                {
                    ret = packages_compare_version( version_installed, version + 1 );
                    switch( version[0] )
                    {
                        case '=':
                            ret = ret == 0;
                            break;

                        case '>':
                            ret = ret == 1;
                            break;

                        case '<':
                            ret = ret == -1;
                            break;

                        default:
                            ret = ret == 0;
                    }
                }
            }
            else
            {
                ret = packages_compare_version( version, version_installed ) == 0;
            }
        }
        else
        {
            ret = 0;
        }

    }
    else
    {
        db_query( &db, "select count(*) from world where name=?", name, NULL);
        db_fetch_num( &db );
        count = atoi( db_get_value_by_index( &db, 0 ) );
        ret = count > 0;
    }


    db_close( &db );

    return ret;
}

int packages_exists( YPackageManager *pm, char *name, char *version )
{
    int     count, ret, repo_testing;
    char    *table, *sql, *version_installed;
    DB      db;

    if( !pm || !name )
        return -1;

    if( pm->accept_repo && !strcmp( pm->accept_repo, "testing" ) )
        repo_testing =1;
    else
        repo_testing =0;

    table = repo_testing ? "universe_testing" : "universe";

    db_init( &db, pm->db_name, OPEN_READ );

    if( version )
    {
        version_installed = NULL;

        sql = util_strcat( "select version from ", table, " where name=?", NULL );
        db_query( &db, sql, name, NULL);
        free( sql );

        if( db_fetch_num( &db ) )
        {
            version_installed = db_get_value_by_index( &db, 0 );
            if( version[0] == '>' || version[0] == '=' || version[0] == '!' || version[0] == '<')
            {
                if( version[1] == '=' )
                {
                    ret = packages_compare_version( version_installed, version + 2 );
                    switch( version[0] )
                    {
                        case '>':
                            ret = ret != -1;
                            break;

                        case '!':
                            ret = ret != 0;
                            break;

                        case '<':
                            ret = ret != 1;
                            break;

                        default:
                            ret = ret == 0;
                    }
                }
                else
                {
                    ret = packages_compare_version( version_installed, version + 1 );
                    switch( version[0] )
                    {
                        case '=':
                            ret = ret == 0;
                            break;

                        case '>':
                            ret = ret == 1;
                            break;

                        case '<':
                            ret = ret == -1;
                            break;

                        default:
                            ret = ret == 0;
                    }
                }
            }
            else
            {
                ret = packages_compare_version( version, version_installed ) == 0;
            }
        }
        else
        {
            ret = 0;
        }

    }
    else
    {
        //db_query( &db, "select count(*) from universe where name=?", name, NULL);

        sql = util_strcat( "select count(*) from ", table, " where name=?", NULL );
        db_query( &db, sql, name, NULL);
        free( sql );

        db_fetch_num( &db );
        count = atoi( db_get_value_by_index( &db, 0 ) );
        ret = count > 0;
    }


    db_close( &db );

    return ret;
}

YPackage *packages_get_repo_package( YPackageManager *pm, char *name, int installed, char *repo )
{
    int                     repo_testing;
    char                    *sql, *cur_key, *cur_value, **attr_keys_offset;
    char                    *attr_keys[] = { "name", "generic_name", "category", "priority", "version", "license", "homepage", "description", "uri", "sha", "size", "repo", "arch", "install", "exec", "data_count", "installed", "install_time", "can_update", "build_date", "packager", NULL  }; 
    DB                      db;
    YPackage                 *pkg = NULL;

    if( !pm || !name )
    {
        return NULL;
    }

    if( repo && !strcmp( repo, "testing" ) )
        repo_testing =1;
    else
        repo_testing =0;

    db_init( &db, pm->db_name, OPEN_READ );
    sql = util_strcat( "select * from ", installed ? "world" : (repo_testing ? "universe_testing" : "universe")," where name=? limit 1", NULL );
    db_query( &db, sql, name, NULL);
    free( sql );

    if( db_fetch_assoc( &db ) )
    {
        pkg = (YPackage *)malloc( sizeof( YPackage ) );
        pkg->ht = hash_table_init( );

        attr_keys_offset = attr_keys;
        while( (cur_key = *attr_keys_offset++) )
        {
            cur_value = db_get_value_by_key( &db, cur_key );
            hash_table_add_data( pkg->ht, cur_key, cur_value );
        }
    }

    db_close( &db );

    return pkg;
}

YPackage *packages_get_package( YPackageManager *pm, char *name, int installed )
{
    char                    *repo;

    if( !pm || !name )
    {
        return NULL;
    }

    if( pm->accept_repo && !strcmp( pm->accept_repo, "testing" ) )
        repo = "testing";
    else
        repo = "stable";

    return packages_get_repo_package( pm, name, installed, repo );
}

/*
 * packages_get_package_from_ypk
 */
int packages_get_package_from_ypk( char *ypk_path, YPackage **package, YPackageData **package_data )
{
    char                tmp_ypk_desktop[] = "/tmp/ypkdesktop.XXXXXX"; //for ypki2

    return packages_get_info_from_ypk( ypk_path, package, package_data, NULL, NULL, tmp_ypk_desktop );
}


/*
 * packages_get_info_from_ypk
 */
int packages_get_info_from_ypk( char *ypk_path, YPackage **package, YPackageData **package_data, YPackageFile **package_file, char *install_script, char *desktop_file )
{
    int                 i, ret, return_code = 0, cur_data_index, data_count;
    void                *pkginfo = NULL, *control = NULL, *filelist = NULL;
    size_t              pkginfo_len = 0, control_len = 0, filelist_len = 0;
    char                *cur_key, *cur_value, *cur_xpath, **attr_keys_offset, **attr_xpath_offset, *idx, *data_key, *package_name, *is_desktop, *tmp;
    char                *attr_keys[] = { "name", "exec", "generic_name", "category", "arch", "priority", "version", "install", "license", "homepage", "repo", "description", "sha", "size", "build_date", "packager", "uri", "data_count", "is_desktop", NULL  }; 
    char                *attr_xpath[] = { "//Package/@name", "//exec", "//genericname/keyword", "//category", "//arch", "//priority", "//version", "//install", "//license", "//homepage", "//repo", "//description/keyword", "//sha", "//size", "//build_date", "//packager", "//uri", "//data_count", "//genericname[@type='desktop']", NULL  }; 
    char                *data_attr_keys[] = { "data_name", "data_format", "data_size", "data_install_size", "data_depend", "data_bdepend", "data_recommended", "data_conflict", "data_replace", NULL  }; 
    char                *data_attr_xpath[] = { "name", "format", "size", "install_size", "depend", "bdepend", "recommended", "conflict", "replace", NULL  }; 
    xmlDocPtr           xmldoc = NULL;
    YPackage            *pkg = NULL;
    YPackageData        *pkg_data = NULL;
    YPackageFile        *pkg_file = NULL;

    if( access( ypk_path, R_OK ) )
        return -1;

    //unzip pkginfo
    ret = archive_extract_file2( ypk_path, "pkginfo", &pkginfo, &pkginfo_len );
    if( ret == -1 || pkginfo_len == 0 )
    {
        return_code = -1;
        goto exception_handler;
    }

    //get control info
    ret = archive_extract_file4( pkginfo, pkginfo_len, "control.xml", &control, &control_len );
    if( ret == -1 ||  control_len == 0 )
    {
        return_code = -2;
        goto exception_handler;
    }

	if( ( xmldoc = xpath_open2( control, control_len ) ) == (xmlDocPtr)-1 )
    {
        return_code = -3;
        goto exception_handler;
    }

    if( control )
    {
        free( control );
        control = NULL;
    }

    if( package || install_script || desktop_file )
    {
        pkg = (YPackage *)malloc( sizeof( YPackage ) );
        pkg->ht = hash_table_init( );

        attr_keys_offset = attr_keys;
        attr_xpath_offset = attr_xpath;
        while( (cur_key = *attr_keys_offset++) )
        {
            cur_xpath = *attr_xpath_offset++;
            cur_value =  xpath_get_node( xmldoc, (xmlChar *)cur_xpath );

            if( strcmp( cur_key, "is_desktop" ) == 0 )
            {
                hash_table_add_data( pkg->ht, cur_key, cur_value ? "1" : "0" );
            }
            else
            {
                hash_table_add_data( pkg->ht, cur_key, cur_value );
            }

            if( cur_value )
                free( cur_value );
        }
        if( package )
            *package = pkg;
    }

    if( package_data )
    {
        if( package && packages_get_package_attr( (*package), "data_count") )
        {
            cur_value = packages_get_package_attr( (*package), "data_count");
            data_count =  cur_value ? atoi( cur_value ) : 1;
        }
        else
        {
            cur_value = xpath_get_node( xmldoc, (xmlChar *)"//data_count" );
            data_count =  cur_value ? atoi( cur_value ) : 1;
            free( cur_value );
        }
        data_count = data_count ? data_count : 1;

        pkg_data = (YPackageData *)malloc( sizeof( YPackageData ) );
        pkg_data->htl = hash_table_list_init( data_count );
        
        cur_data_index = 0;
        pkg_data->cnt = 0;

        data_key = (char *)malloc( 32 );
        for( i = 0; ; i++ )
        {
            idx = util_int_to_str( i );
            cur_value = xpath_get_node( xmldoc, (xmlChar *)util_strcat2( data_key, 32, "//data[@id='", idx, "']", NULL ) );
            if( !cur_value )
            {
                free( idx );
                break;
            }
            free( cur_value );

            attr_keys_offset = data_attr_keys;
            attr_xpath_offset = data_attr_xpath;
            while( (cur_key = *attr_keys_offset++) )
            {
                cur_xpath = *attr_xpath_offset++;
                cur_value = xpath_get_node( xmldoc, (xmlChar *)util_strcat2( data_key, 32, "//data[@id='", idx, "']/", cur_xpath, NULL ) );
                hash_table_list_add_data( pkg_data->htl, cur_data_index, cur_key, cur_value );
                if( cur_value  )
                    free( cur_value );
            }
            free( idx );
            cur_data_index++;
        }
        free( data_key );
        pkg_data->cnt = cur_data_index;

        if( pkg_data->cnt == 0 )
        {
            hash_table_list_cleanup( pkg_data->htl );
            free( pkg_data );
            *package_data =  NULL;
        }
        else
        {
            *package_data = pkg_data;
        }
    }

    //get filelist
    if( package_file )
    {
        ret = archive_extract_file4( pkginfo, pkginfo_len, "filelist", &filelist, &filelist_len );
        if( ret == -1 ||  filelist_len == 0)
        {
            return_code = -4;
            goto exception_handler;
        }

        pkg_file = packages_get_package_file_from_str( filelist );

        *package_file = pkg_file;
    }

    //extract install script
    if( install_script )
    {
        tmp = packages_get_package_attr( (*package), "install" );
        if( tmp )
        {
            ret = archive_extract_file3( pkginfo, pkginfo_len, tmp, install_script );
            tmp = NULL;
            if( ret == -1 )
            {
                return_code = -5;
                goto exception_handler;
            }
        }
    }

    //extract desktop file
    if( desktop_file )
    {
        is_desktop = packages_get_package_attr( (*package), "is_desktop" );
        if( is_desktop && is_desktop[0] == '1' )
        {
            package_name = packages_get_package_attr( (*package), "name");
            tmp = util_strcat( package_name, ".desktop", NULL );
            ret = archive_extract_file3( pkginfo, pkginfo_len, tmp, desktop_file );
            free( tmp );
            tmp = NULL;
            if( ret == -1 )
            {
                return_code = -6;
                goto exception_handler;
            }
        }
    }


    //clean up
    if( pkginfo )
    {
        free( pkginfo );
        pkginfo = NULL;
    }

    if( xmldoc )
        xmlFreeDoc( xmldoc );

	xmlCleanupParser();

    if( !package && pkg )
    {
        packages_free_package( pkg );
        pkg = NULL;
    }

    return 0;

exception_handler:
    if( pkginfo )
        free( pkginfo );

    if( control )
        free( control );

    if( xmldoc )
        xmlFreeDoc( xmldoc );

	xmlCleanupParser();

    if( pkg )
    {
        packages_free_package( pkg );
        pkg = NULL;
    }

    if( pkg_data )
    {
        packages_free_package_data( pkg_data );
        pkg_data = NULL;
    }


    if( pkg_file )
    {
        packages_free_package_file( pkg_file );
        pkg_file = NULL;
    }

    if( package )
        *package = NULL;

    if( package_data )
        *package_data = NULL;

    if( package_file )
        *package_file = NULL;

    return return_code;
}

/*
 * packages_get_package_attr
 */
char *packages_get_package_attr( YPackage *pkg, char *key )
{
    if( !pkg || !key )
        return NULL;

    return hash_table_get_data( pkg->ht, key );
}

char *packages_get_package_attr2( YPackage *pkg, char *key )
{
    char *result;

    if( !pkg || !key )
        return NULL;

    result = hash_table_get_data( pkg->ht, key );
    return result ? result : "";
}

/*
 * packages_free_package
 */
void packages_free_package( YPackage *pkg )
{
    if( !pkg )
        return;

    hash_table_cleanup( pkg->ht );
    free( pkg );
}

/*
 * packages_get_package_data
 */
YPackageData *packages_get_package_data( YPackageManager *pm, char *name, int installed )
{
    int                     data_count, cur_data_index, repo_testing;
    char                    *sql, *cur_key, *cur_value, **attr_keys_offset;
    char                    *attr_keys[] = { "name", "data_name", "data_format", "data_size", "data_install_size", "data_depend", "data_bdepend", "data_recommended", "data_conflict", "data_replace", NULL  }; 
    DB                      db;
    YPackageData            *pkg_data = NULL;

    if( !pm || !name )
    {
        return NULL;
    }

    if( pm->accept_repo && !strcmp( pm->accept_repo, "testing" ) )
        repo_testing =1;
    else
        repo_testing =0;

    db_init( &db, pm->db_name, OPEN_READ );
    //get file count
    sql = util_strcat( "select count(*) from ", installed ? "world_data" : repo_testing ? "universe_testing_data" : "universe_data"," where name=?", NULL );
    db_query( &db, sql, name, NULL);
    free( sql );
    db_fetch_num( &db );
    data_count = atoi( db_get_value_by_index( &db, 0 ) );
    db_cleanup( &db );

    //get data
    sql = util_strcat( "select * from ", installed ? "world_data" : repo_testing ? "universe_testing_data" : "universe_data"," where name=?", NULL );
    db_query( &db, sql, name, NULL);
    free( sql );

    pkg_data = (YPackageData *)malloc( sizeof( YPackageData ) );
    pkg_data->htl = hash_table_list_init( data_count );

    cur_data_index = 0;
    pkg_data->cnt = 0;

    while( db_fetch_assoc( &db ) )
    {

        attr_keys_offset = attr_keys;
        while( (cur_key = *attr_keys_offset++) )
        {
            cur_value = db_get_value_by_key( &db, cur_key );
            hash_table_list_add_data( pkg_data->htl, cur_data_index, cur_key, cur_value );
        }
        cur_data_index++;
    }
    pkg_data->cnt = cur_data_index;

    db_close( &db );
    if( pkg_data->cnt == 0 )
    {
        hash_table_list_cleanup( pkg_data->htl );
        free( pkg_data );
        return NULL;
    }

    return pkg_data;
}

/*
 * packages_get_package_data_attr
 */
char *packages_get_package_data_attr( YPackageData *pkg_data, int index, char *key )
{
    return hash_table_list_get_data( pkg_data->htl, index, key );
}

char *packages_get_package_data_attr2( YPackageData *pkg_data, int index, char *key )
{
    char *result;

    result = hash_table_list_get_data( pkg_data->htl, index, key );
    return result ? result : "";
}
/*
 * packages_free_package_data
 */
void packages_free_package_data( YPackageData *pkg_data )
{
    if( !pkg_data )
        return;

    hash_table_list_cleanup( pkg_data->htl );
    free( pkg_data );
}

/*
 * packages_get_package_file
 */
YPackageFile *packages_get_package_file( YPackageManager *pm, char *name )
{
    int                     file_count, file_type, cur_file_index;
    char                    *cur_key, *cur_value, **attr_keys_offset;
    char                    *attr_keys[] = { "name", "file", "type", "size", "perms", "uid", "gid", "mtime", "replace", "replace_with", NULL  }; 
    DB                      db;
    YPackageFile            *pkg_file = NULL;

    if( !pm || !name )
    {
        return NULL;
    }


    db_init( &db, pm->db_name, OPEN_READ );

    //get file count
    db_query( &db, "select count(*) from world_file where name=?", name, NULL);
    db_fetch_num( &db );
    file_count = atoi( db_get_value_by_index( &db, 0 ) );
    db_cleanup( &db );

    //get file info
    db_query( &db, "select * from world_file where name=?", name, NULL);

    pkg_file = (YPackageFile *)malloc( sizeof( YPackageFile ) );
    pkg_file->htl = hash_table_list_init( file_count );

    cur_file_index = 0;
    pkg_file->cnt = 0;
    pkg_file->cnt_file = 0;
    pkg_file->cnt_dir = 0;
    pkg_file->cnt_link = 0;
    pkg_file->size = 0;

    while( db_fetch_assoc( &db ) )
    {
        attr_keys_offset = attr_keys;
        while( (cur_key = *attr_keys_offset++) )
        {
            cur_value = db_get_value_by_key( &db, cur_key );
            hash_table_list_add_data( pkg_file->htl, cur_file_index, cur_key, cur_value );

            file_type = 1;
            //counter
            if( cur_key[0] == 't' && cur_value ) //type
            {
                switch( cur_value[0] )
                {
                    case 'F':
                        pkg_file->cnt_file++;
                        file_type = 1;
                        break;
                    case 'D':
                        pkg_file->cnt_dir++;
                        file_type = 2;
                        break;
                    case 'S':
                        pkg_file->cnt_link++;
                        file_type = 3;
                        break;
                }
            }
            else if( cur_key[0] == 's' && file_type == 1 ) //size
            {
                pkg_file->size += atoi( cur_value );
            }
        }
        cur_file_index++;
    }
    pkg_file->cnt = cur_file_index;

    db_close( &db );
    if( pkg_file->cnt == 0 )
    {
        hash_table_list_cleanup( pkg_file->htl );
        free( pkg_file );
        return NULL;
    }

    pkg_file->size = pkg_file->size / 1024;
    return pkg_file;
}

/*
 * packages_get_package_file_from_str
 */
YPackageFile *packages_get_package_file_from_str( char *filelist )
{
    int                 file_count, cur_file_index;
    char                *saveptr;
    char                *pos, *list_line, *cur_key, *cur_value, **attr_keys_offset;
    char                *attr_keys[] = { "type", "file", "size", "perms", "uid", "gid", "mtime", "extra", NULL  }; 
    YPackageFile        *pkg_file = NULL;

    if( !filelist )
        return NULL;

    pos = strrchr( filelist, '\n' );
    if( !pos )
        return NULL;

    if( !pos[1] )
    {
        *pos = 0;
        pos = strrchr( filelist, '\n' );
        if( !pos )
            return NULL;
    }

    if( pos[1] != 'I' )
    {
        return NULL;
    }

    pkg_file = (YPackageFile *)malloc( sizeof( YPackageFile ) );

    strtok_r( pos + 1, ",", &saveptr );
    pkg_file->cnt_file = atoi( strtok_r( NULL, ",", &saveptr ) );
    pkg_file->cnt_dir = atoi( strtok_r( NULL, ",", &saveptr ) );
    pkg_file->cnt_link = atoi( strtok_r( NULL, ",", &saveptr ) );
    strtok_r( NULL, ",", &saveptr );
    pkg_file->size = atoi( strtok_r( NULL, ",", &saveptr ) );

    file_count = pkg_file->cnt_file + pkg_file->cnt_dir + pkg_file->cnt_link;
    pkg_file->htl = hash_table_list_init( file_count );

    cur_file_index = 0;
    pkg_file->cnt = 0;

    list_line = util_mem_gets( filelist );
    while( list_line )
    {
        if( cur_file_index + 1 > file_count  || list_line[0] == 'I' )
        {
            free( list_line );
            break;
        }

        if( list_line[0] == 'F' ||  list_line[0] == 'D' ||  list_line[0] == 'S'  )
        {
            attr_keys_offset = attr_keys;
            cur_value = strtok_r( list_line, ",", &saveptr );
            while( (cur_key = *attr_keys_offset++) )
            {
                hash_table_list_add_data( pkg_file->htl, cur_file_index, cur_key, cur_value );
                cur_value =  strtok_r( NULL, ",", &saveptr );
            }
            cur_file_index++;
        }

        free( list_line );
        list_line = util_mem_gets( NULL );
    }
    pkg_file->cnt = cur_file_index;

    if( !pkg_file )
        return NULL;

    if( pkg_file->cnt == 0 )
    {
        hash_table_list_cleanup( pkg_file->htl );
        free( pkg_file );
        return NULL;
    }

    return pkg_file;
}

/*
 * packages_get_package_file_from_ypk
 */
YPackageFile *packages_get_package_file_from_ypk( char *ypk_path )
{
    int                 ret;
    size_t              pkginfo_len = 0, filelist_len = 0;
    void                *pkginfo = NULL, *filelist = NULL;
    YPackageFile        *pkg_file = NULL;

    if( access( ypk_path, R_OK ) )
        return NULL;


    //unzip info
    ret = archive_extract_file2( ypk_path, "pkginfo", &pkginfo, &pkginfo_len );
    if( ret == -1 || pkginfo_len == 0 )
    {
        goto exception_handler;
    }

    ret = archive_extract_file4( pkginfo, pkginfo_len, "filelist", &filelist, &filelist_len );
    if( ret == -1 ||  filelist_len == 0)
    {
        goto exception_handler;
    }

    pkg_file = packages_get_package_file_from_str( filelist );

exception_handler:
    if( pkginfo )
        free( pkginfo );

    if( filelist )
        free( filelist );

    return pkg_file;
}

/*
 * packages_get_package_file_attr
 */
char *packages_get_package_file_attr( YPackageFile *pkg_file, int index, char *key )
{
    return hash_table_list_get_data( pkg_file->htl, index, key );
}

char *packages_get_package_file_attr2( YPackageFile *pkg_file, int index, char *key )
{
    char *result;

    result = hash_table_list_get_data( pkg_file->htl, index, key );
    return result ? result : "";
}

/*
 * packages_free_package_file
 */
void packages_free_package_file( YPackageFile *pkg_file )
{
    if( !pkg_file )
        return;

    hash_table_list_cleanup( pkg_file->htl );
    free( pkg_file );
}


YPackageReplaceFileList *packages_get_replace_file_list( YPackageManager *pm, YPackageData *pkg_data, YPackageFile *pkg_file )
{
    int                 i, j, k;
    char                *replace, *tmp, *token, *saveptr, *file, *type, *replace_with;
    YPackageFile        *pkg_file2;
    YPackageReplaceFileList *list;

    if( !pkg_data || !pkg_file )
        return NULL;

    list = NULL;

    for( i = 0; i < pkg_data->cnt; i++ )
    {
        tmp = packages_get_package_data_attr( pkg_data, i, "data_replace");
        if( tmp )
        {
            replace = strdup( tmp );
            tmp = NULL;
            token = strtok_r( replace, " ,", &saveptr );
            while( token )
            {
                if( packages_has_installed( pm, token, NULL ) )
                {
                    pkg_file2 = packages_get_package_file( pm, token );

                    for( j = 0; j < pkg_file2->cnt; j++ )
                    {
                        replace_with = packages_get_package_file_attr( pkg_file2, j, "replace_with");
                        if( replace_with && strlen( replace_with ) > 0 )
                            continue;

                        file = packages_get_package_file_attr( pkg_file2, j, "file");
                        type = packages_get_package_file_attr( pkg_file2, j, "type");
                        if( file && type && ( type[0] == 'F' || type[0] == 'S' ) )
                        {
                            for( k = 0; k < pkg_file->cnt; k++ )
                            {
                                if( !strcmp( file, packages_get_package_file_attr( pkg_file, k, "file" ) ) )
                                {

                                    if( !list )
                                    {
                                        list = (YPackageReplaceFileList *)malloc( sizeof( YPackageReplaceFileList ) );
                                        list->max_cnt = pkg_file->cnt;
                                        list->cur_cnt = 0;
                                        list->htl = hash_table_list_init( pkg_file->cnt );
                                    }

                                    if( list->cur_cnt >= list->max_cnt )
                                        break;

                                    hash_table_list_add_data( list->htl, list->cur_cnt, "replace", token );
                                    hash_table_list_add_data( list->htl, list->cur_cnt, "file", file );
                                    list->cur_cnt++;
                                    break;
                                }
                            } //for k
                        }
                    } //for j
                    packages_free_package_file( pkg_file2 );
                    pkg_file2 = NULL;
                }
                token = strtok_r( NULL, " ,", &saveptr );
            }
            free( replace );
        }
    }

    return list;
}

void packages_free_replace_file_list( YPackageReplaceFileList *list )
{
    if( !list )
        return;

    hash_table_list_cleanup( list->htl );
    free( list );
}


/*
 * packages_get_list
 */
YPackageList *packages_get_list( YPackageManager *pm, int limit, int offset, char *keys[], char *keywords[], int wildcards[], int installed )
{
    int                     cur_pkg_index, repo_testing;
    char                    *table, *sql, *where_str, *order_str, *offset_str, *limit_str, *cur_key, *cur_value, **attr_keys_offset, *tmp;
    char                    *attr_keys[] = { "name", "generic_name", "category", "priority", "version", "license", "description", "size", "repo", "exec", "install_time", "installed", "can_update", "homepage", "build_date", "packager", "language", "kw_name", "kw_generic_name", "kw_fullname", "kw_comment", NULL  }; 
    DB                      db;
    YPackageList            *pkg_list;


    if( offset < 0 )
        offset = 0;

    if( limit < 1 )
        limit = 5;

    if( pm->accept_repo && !strcmp( pm->accept_repo, "testing" ) )
        repo_testing =1;
    else
        repo_testing =0;

    where_str = NULL;

    offset_str = util_int_to_str( offset );
    limit_str = util_int_to_str( limit );

    table = installed ? "world" : repo_testing ? "universe_testing" : "universe";
    order_str = installed ? " order by install_time desc " : "";

    db_init( &db, pm->db_name, OPEN_READ );
    if( !keys || !keywords || !wildcards || !(*keys) || !(*keywords) || !(*wildcards) )
    {
        sql = util_strcat( "select * from ", table, order_str, " limit ? offset ?", NULL );
        db_query( &db, sql, limit_str, offset_str, NULL);
        free( sql );
    }
    else
    {
        while( *keys && *keywords && *wildcards )
        {
            tmp = NULL;

            if( (*keywords) && ((*keys)[0] == '*') && (*wildcards == 2) )
            {
                //sql = util_strcat( "select * from ", table, " where name like '%'||?||'%' or generic_name like  '%'||?||'%'  or description like '%'||?||'%' limit ? offset ?", NULL );

                if( where_str )
                    tmp = where_str;

                where_str = util_strcat( tmp ? tmp : "", tmp ? " and " : "",  "(", table, ".name like '%", *keywords, "%' or ", table, ".generic_name like  '%", *keywords, "%' or ", table, ".description like  '%", *keywords, "%' or keywords.kw_name like  '%", *keywords, "%' or keywords.kw_generic_name like  '%", *keywords, "%' or keywords.kw_fullname like  '%", *keywords, "%' or keywords.kw_comment like '%", *keywords, "%')", NULL );
                if( tmp )
                    free( tmp );


                //db_query( &db, sql, keyword, keyword, keyword, limit_str, offset_str, NULL);
                //free( sql );
            }
            else if( (*keywords) && ((*keys)[0] == '*') && (*wildcards == 1) )
            {
                //sql = util_strcat( "select * from ", table, " where name = ? or generic_name = ? or description = ? limit ? offset ?", NULL );

                if( where_str )
                    tmp = where_str;

                where_str = util_strcat( tmp ? tmp : "", tmp ? " and " : "",  "(" , table, ".name = '", *keywords, "' or ", table, ".generic_name = '", *keywords, "' or keywords.kw_name = '", *keywords, "' or keywords.kw_generic_name = '", *keywords, "' or keywords.kw_fullname = '", *keywords, "')", NULL );
                if( tmp )
                    free( tmp );

                //db_query( &db, sql, keyword, keyword, keyword, limit_str, offset_str, NULL);
                //free( sql );
            }
            else if( *keywords && *keys && *wildcards )
            {
                //sql = util_strcat( "select * from ", table, " where ", key, wildcards ? " like '%'||?||'%' limit ? offset ?" : " = ? limit ? offset ?", NULL );

                if( where_str )
                    tmp = where_str;

                if( strncmp( *keys, "kw_", 3 ) && strcmp( *keys, "language" ) )
                {
                    where_str = util_strcat( tmp ? tmp : "", tmp ? " and (" : "(", table, ".", *keys, *wildcards == 2 ? " like '%" : " = '", *keywords, *wildcards == 2 ? "%')" : "')", NULL );
                }
                else
                {
                    where_str = util_strcat( tmp ? tmp : "", tmp ? " and (keywords." : "(keywords.", *keys, *wildcards == 2 ? " like '%" : " = '", *keywords, *wildcards == 2 ? "%')" : "')", NULL );
                }

                if( tmp )
                    free( tmp );

                //db_query( &db, sql, keyword, limit_str, offset_str, NULL );
                //free( sql );
            }
            else
            {
                break;
            }

            keys++;
            keywords++;
            wildcards++;
        }

        if( where_str )
        {
            sql = util_strcat( "select * from ", table, " left join keywords on ", table, ".name=keywords.name where ", where_str, " group by ", table, ".name ", order_str, " limit ? offset ?", NULL );
            //printf( "%s\n", sql );
            db_query( &db, sql, limit_str, offset_str, NULL );

            free( where_str );
            free( sql );
        }
    }
    free( limit_str );
    free( offset_str );

    pkg_list = (YPackageList *)malloc( sizeof( YPackageList ) );
    pkg_list->htl = hash_table_list_init( limit );

    cur_pkg_index = 0;
    pkg_list->cnt = 0;

    while( db_fetch_assoc( &db ) )
    {

        attr_keys_offset = attr_keys;
        while( (cur_key = *attr_keys_offset++) )
        {
            cur_value = db_get_value_by_key( &db, cur_key );
            hash_table_list_add_data( pkg_list->htl, cur_pkg_index, cur_key, cur_value );
        }
        cur_pkg_index++;
    }
    pkg_list->cnt = cur_pkg_index;

    db_close( &db );

    if( pkg_list->cnt == 0 )
    {
        hash_table_list_cleanup( pkg_list->htl );
        free( pkg_list );
        return NULL;
    }

    return pkg_list;
}

/*
 * packages_get_list2
 */
YPackageList *packages_get_list2( YPackageManager *pm, int page_size, int page_no, char *keys[], char *keywords[], int wildcards[], int installed )
{
    int                     offset;
    YPackageList            *pkg_list;


    if( page_size < 1 )
        page_size = 5;

    if( page_no < 1 )
        page_no = 1;

    offset = ( page_no - 1 ) * page_size;

    pkg_list = packages_get_list( pm, page_size, offset, keys, keywords, wildcards, installed );

    return pkg_list;
}

/*
 * packages_get_list_with_data
 */
YPackageList *packages_get_list_with_data( YPackageManager *pm, int limit, int offset, char *key, char *keyword, int installed )
{    
    int                     cur_pkg_index, repo_testing;
    char                    *table, *table_data, *sql, *offset_str, *limit_str, *cur_key, *cur_value, **attr_keys_offset;
    char                    *attr_keys[] = { "name", "generic_name", "category", "priority", "version", "license", "description", "size", NULL  }; 
    DB                      db;
    YPackageList            *pkg_list;

    if( !key || !keyword )
        return NULL;

    if( offset < 0 )
        offset = 0;

    if( limit < 1 )
        limit = 5;

    if( pm->accept_repo && !strcmp( pm->accept_repo, "testing" ) )
        repo_testing =1;
    else
        repo_testing =0;

    offset_str = util_int_to_str( offset );
    limit_str = util_int_to_str( limit );

    table = installed ? "world" : repo_testing ? "universe_testing" : "universe";
    table_data = installed ? "world_data" : repo_testing ? "universe_testing_data" : "universe_data";

    db_init( &db, pm->db_name, OPEN_READ );
    sql = util_strcat( "select ", table, ".* from ", table, ",", table_data, " where ", table, ".name=", table_data, ".name and ", key, " = ? ", "union select ", table, ".* from ", table, ",", table_data, " where ", table, ".name=", table_data, ".name and ", key, " like ?||',%'", "union select ", table, ".* from ", table, ",", table_data, " where ", table, ".name=", table_data, ".name and ", key, " like '%,'||?", "union select ", table, ".* from ", table, ",", table_data, " where ", table, ".name=", table_data, ".name and ", key, " like '%,'||?||',%'", "union select ", table, ".* from ", table, ",", table_data, " where ", table, ".name=", table_data, ".name and ", key, " like ?||'(%'", "union select ", table, ".* from ", table, ",", table_data, " where ", table, ".name=", table_data, ".name and ", key, " like '%,'||?||'(%'", " limit ? offset ?", NULL );
    db_query( &db, sql, keyword, keyword, keyword, keyword, keyword, keyword, limit_str, offset_str, NULL);
    free( sql );
    free( limit_str );
    free( offset_str );

    pkg_list = (YPackageList *)malloc( sizeof( YPackageList ) );
    pkg_list->htl = hash_table_list_init( limit );

    cur_pkg_index = 0;
    pkg_list->cnt = 0;

    while( db_fetch_assoc( &db ) )
    {
        attr_keys_offset = attr_keys;
        while( (cur_key = *attr_keys_offset++) )
        {
            cur_value = db_get_value_by_key( &db, cur_key );
            hash_table_list_add_data( pkg_list->htl, cur_pkg_index, cur_key, cur_value );
        }
        cur_pkg_index++;
    }
    pkg_list->cnt = cur_pkg_index;

    db_close( &db );

    if( pkg_list->cnt == 0 )
    {
        hash_table_list_cleanup( pkg_list->htl );
        free( pkg_list );
        return NULL;
    }

    return pkg_list;

}

/*
 * packages_get_list_by_recommended
 */
YPackageList *packages_get_list_by_recommended( YPackageManager *pm, int limit, int offset, char *recommended, int installed )
{
    return packages_get_list_with_data( pm, limit, offset, "data_recommended", recommended, installed );
}

/*
 * packages_get_list_by_conflict
 */
YPackageList *packages_get_list_by_conflict( YPackageManager *pm, int limit, int offset, char *conflict, int installed )
{
    return packages_get_list_with_data( pm, limit, offset, "data_conflict", conflict,installed );
}

/*
 * packages_get_list_by_depend
 */
YPackageList *packages_get_list_by_depend( YPackageManager *pm, int limit, int offset, char *depend, int installed )
{
    return packages_get_list_with_data( pm, limit, offset, "data_depend", depend,installed );
}

/*
 * packages_get_list_by_bdepend
 */
YPackageList *packages_get_list_by_bdepend( YPackageManager *pm, int limit, int offset, char *bdepend, int installed )
{
    return packages_get_list_with_data( pm, limit, offset, "data_bdepend", bdepend,installed );
}

/*
 * packages_get_list_by_file
 */
YPackageList *packages_get_list_by_file( YPackageManager *pm, int limit, int offset, char *file )
{    
    int                     cur_pkg_index;
    char                    *sql, *offset_str, *limit_str, *cur_key, *cur_value, **attr_keys_offset;
    char                    *attr_keys[] = { "name", "generic_name", "category", "priority", "version", "license", "description", "size", "file", "type", "extra", NULL  }; 
    DB                      db;
    YPackageList            *pkg_list;

    if( !file )
        return NULL;

    if( offset < 0 )
        offset = 0;

    if( limit < 1 )
        limit = 5;

    offset_str = util_int_to_str( offset );
    limit_str = util_int_to_str( limit );

    db_init( &db, pm->db_name, OPEN_READ );
    sql = util_strcat( "select distinct * from world,world_file where world.name=world_file.name  and file like ", file[0] == '/' ? "'%'" : "'%/'", "||? limit ? offset ?", NULL );
    db_query( &db, sql, file, limit_str, offset_str, NULL);
    free( sql );
    free( limit_str );
    free( offset_str );

    pkg_list = (YPackageList *)malloc( sizeof( YPackageList ) );
    pkg_list->htl = hash_table_list_init( limit );

    cur_pkg_index = 0;
    pkg_list->cnt = 0;
    while( db_fetch_assoc( &db ) )
    {
        attr_keys_offset = attr_keys;
        while( (cur_key = *attr_keys_offset++) )
        {
            cur_value = db_get_value_by_key( &db, cur_key );
            hash_table_list_add_data( pkg_list->htl, cur_pkg_index, cur_key, cur_value );
        }
        cur_pkg_index++;
    }
    pkg_list->cnt = cur_pkg_index;

    db_close( &db );

    if( pkg_list->cnt == 0 )
    {
        hash_table_list_cleanup( pkg_list->htl );
        free( pkg_list );
        return NULL;
    }

    return pkg_list;
}

/*
 * packages_free_list
 */
void packages_free_list( YPackageList *pkg_list )
{
    if( !pkg_list )
        return;

    hash_table_list_cleanup( pkg_list->htl );
    free( pkg_list );
}

/*
 * packages_get_list_attr
 */
char *packages_get_list_attr( YPackageList *pkg_list, int index, char *key )
{
    return hash_table_list_get_data( pkg_list->htl, index, key );
}

char *packages_get_list_attr2( YPackageList *pkg_list, int index, char *key )
{
    char *result;

    result = hash_table_list_get_data( pkg_list->htl, index, key );
    return result ? result : "";
}


int packages_download_progress_callback( void *arg,double dltotal, double dlnow, double ultotal, double ulnow )
{
    YPackageDCB         *cb;

    if( !arg )
        return 0;

    cb = (YPackageDCB *)arg;
    if( cb->dcb )
        return cb->dcb( cb->dcb_arg, cb->package_name, dltotal, dlnow );
    else if( cb->pcb )
        return cb->pcb( cb->pcb_arg, cb->package_name, 2, dlnow / dltotal, NULL );
    else
        return 0;

    //return cb->cb( dcb->arg, dltotal, dlnow );
}

/*
 * packages_download_package
 */
int packages_download_package( YPackageManager *pm, char *package_name, char *url, char *dest, int force, ypk_download_callback dcb, void *dcb_arg, ypk_progress_callback pcb, void *pcb_arg  )
{
    int                 return_code;
    char                *target_url, *package_url, *package_path;
    DownloadFile        file;
    YPackage            *pkg;
    YPackageDCB         *cb = NULL;

    if( (!url || !dest) && (!pm || !package_name) )
        return -1;

    return_code = 0;
    target_url = NULL;
    package_path = NULL;
    if( url && dest )
    {
        target_url = strdup( url );
        package_path = strdup( dest );
        pkg = NULL;
    }
    else
    {
        pkg = packages_get_package( pm, package_name, 0 );
        if( !pkg )
        {
            return -2;
        }


        package_url = packages_get_package_attr( pkg, "uri" );
        if( !package_url )
        {
            return_code = -3;
            goto exception_handler;
        }

        package_path = util_strcat( pm->package_dest, "/", basename( package_url ), NULL );
        target_url = util_strcat( pm->source_uri, "/", package_url, NULL );
    }
        printf( "Path: %s\n", package_path );

    if( force || access( package_path, R_OK ) )
    {
        file.file = package_path;
        file.stream = NULL;
        cb = NULL;
        if( dcb || pcb )
        {
            cb = (YPackageDCB *)malloc( sizeof( YPackageDCB ) );
            if( cb )
            {
                memset( cb, 0, sizeof( YPackageDCB ) );

                cb->package_name = package_name;
                if( dcb )
                {
                    cb->dcb = dcb;
                    cb->dcb_arg = dcb_arg;
                }

                if( pcb )
                {
                    cb->pcb = pcb;
                    cb->pcb_arg = pcb_arg;
                }


                file.cb = packages_download_progress_callback;
                file.cb_arg = (void *)cb;
            }
            else
            {
                file.cb = NULL;
                file.cb_arg = NULL;
            }
        }
        else
        {
            file.cb = NULL;
            file.cb_arg = NULL;
        }

        if( download_file( target_url, &file ) != 0 )
        {
            if( file.stream )
                fclose( file.stream );
            return_code = -4;
            goto exception_handler;
        }
        fclose( file.stream );
        if( cb )
        {
            free( cb );
            cb = NULL;
        }
    }

exception_handler:
    if( target_url )
        free( target_url );

    if( package_path )
        free( package_path );

    if( pkg )
        packages_free_package( pkg );

    if( cb )
        free( cb );
    return return_code;
}


/*
 * packages_install_package
 */
int packages_install_package( YPackageManager *pm, char *package_name, char *version, ypk_progress_callback cb, void *cb_arg  )
{
    int                 return_code;
    char                *target_url = NULL, *package_url = NULL, *package_path = NULL, *pkg_sha = NULL, *ypk_sha = NULL;
    YPackage            *pkg;

    if( !package_name )
        return -1;

    if( cb )
    {
        cb( cb_arg, package_name, 0, 2, "Install\n" );
        cb( cb_arg, package_name, 1, -1, "Initializing\n" );
    }

    return_code = 0;
    if( !packages_exists( pm, package_name, version ) )
    {
        return -2;
    }

    pkg = packages_get_package( pm, package_name, 0 );
    if( !pkg )
    {
        return -2;
    }

    package_url = packages_get_package_attr( pkg, "uri" );
    if( !package_url )
    {
        return_code = -3;
        goto exception_handler;
    }

    if( cb )
    {
        cb( cb_arg, package_name, 1, 1, NULL );
    }

    package_path = util_strcat( pm->package_dest, "/", basename( package_url ), NULL );
    target_url = util_strcat( pm->source_uri, "/", package_url, NULL );
    pkg_sha = packages_get_package_attr( pkg, "sha" );

    if( !access( package_path, F_OK ) )
    {
        ypk_sha = util_sha1( package_path );

        if( ypk_sha && strncmp( pkg_sha, ypk_sha, 41 ) )
        {
            if( packages_download_package( NULL, package_name, target_url, package_path, 1, NULL, NULL, cb, cb_arg  ) < 0 )
            {
                return -4;
                goto exception_handler;
            }

            free( ypk_sha );

            ypk_sha = util_sha1( package_path );
            if( ypk_sha && strncmp( pkg_sha, ypk_sha, 41 ) )
            {
                return -4;
                goto exception_handler;
            }
        }
    }
    else
    {
        if( packages_download_package( NULL, package_name, target_url, package_path, 0, NULL, NULL, cb, cb_arg  ) < 0 )
        {
            return -4;
            goto exception_handler;
        }

        ypk_sha = util_sha1( package_path );
        if( ypk_sha && strncmp( pkg_sha, ypk_sha, 41 ) )
        {
            return -4;
            goto exception_handler;
        }
    }

    if( packages_install_local_package( pm, package_path, "/", 0, cb, cb_arg ) )
    {
        return_code = -5;
    }

exception_handler:
    if( package_path )
        free( package_path );

    if( target_url )
        free( target_url );

    if( ypk_sha )
        free( ypk_sha );

    packages_free_package( pkg );
    return return_code;
}


YPackageChangeList *packages_get_depend_list( YPackageManager *pm, char *package_name, char *version )
{
    int             i, len;
    char            *token, *saveptr, *depend, *version2, *tmp, *tmp2;
    YPackageData    *pkg_data;
    YPackageChangeList    *list, *cur_pkg, *sub_list, *tmp_list;

    /*
    if( packages_has_installed( pm, package_name, version ) )
    {
        return NULL;
    }
    */
    
    list = NULL;

    if( (pkg_data = packages_get_package_data( pm, package_name, 0 )) )
    {
        for( i = 0; i < pkg_data->cnt; i++ )
        {
            depend = packages_get_package_data_attr( pkg_data, i, "data_depend");
            if( depend )
            {
                version2 = NULL;
                token = strtok_r( depend, " ,", &saveptr );
                while( token )
                {
                    tmp = util_strcat( token, NULL );
                    if( (version2 = strchr( tmp, '(' )) )
                    {
                        *version2++ = 0;

                        while( (*version2 == ' ') )
                            version2++;

                        if( (tmp2 = strchr( version2, ')' )) )
                            *tmp2 = 0;
                    }

                    if( !packages_has_installed( pm, tmp, version2 ) )
                    {
                        cur_pkg =  (YPackageChangeList *)malloc( sizeof( YPackageChangeList ) );
                        len = strlen( tmp );
                        cur_pkg->name = (char *)malloc( len + 1 );
                        strncpy( cur_pkg->name, tmp, len );
                        cur_pkg->name[len] = 0;
                        
                        if( version2 )
                        {
                            len = strlen( version2 );
                            cur_pkg->version = (char *)malloc( len + 1 );
                            strncpy( cur_pkg->version, version2, len );
                            cur_pkg->version[len] = 0;
                        }
                        else
                        {
                            cur_pkg->version = NULL;
                        }

                        cur_pkg->type = 2;
                        cur_pkg->prev = list;
                        list = cur_pkg;

                        sub_list = packages_get_depend_list( pm, cur_pkg->name, cur_pkg->version );
                        if( sub_list )
                        {
                            tmp_list = sub_list;
                            while( tmp_list->prev )
                                tmp_list = tmp_list->prev;
                            tmp_list->prev = list;
                            list = sub_list;
                        }
                    }

                    free( tmp );
                    version2 = NULL;
                    token = strtok_r( NULL, " ,", &saveptr );
                }
            }

        }
        packages_free_package_data( pkg_data );
    }

    packages_clist_remove_duplicate_item( list );
    return list;
}

YPackageChangeList *packages_get_recommended_list( YPackageManager *pm, char *package_name, char *version )
{
    int             i, len;
    char            *token, *saveptr, *recommended, *version2, *tmp, *tmp2;
    YPackageData    *pkg_data;
    YPackageChangeList    *list, *cur_pkg, *sub_list, *tmp_list;

    /*
    if( packages_has_installed( pm, package_name, NULL ) )
    {
        return NULL;
    }
    */
    
    list = NULL;

    if( (pkg_data = packages_get_package_data( pm, package_name, 0 )) )
    {
        for( i = 0; i < pkg_data->cnt; i++ )
        {
            recommended = packages_get_package_data_attr( pkg_data, i, "data_recommended");
            if( recommended )
            {
                version2 = NULL;
                token = strtok_r( recommended, " ,", &saveptr );
                while( token )
                {
                    tmp = util_strcat( token, NULL );
                    if( (version2 = strchr( tmp, '(' )) )
                    {
                        *version2++ = 0;

                        while( *version2 == ' ' )
                            version2++;

                        if( (tmp2 = strchr( version2, ')' )) )
                            *tmp2 = 0;
                    }

                    if( !packages_has_installed( pm, tmp, version2 ) )
                    {
                        cur_pkg =  (YPackageChangeList *)malloc( sizeof( YPackageChangeList ) );
                        len = strlen( tmp );
                        cur_pkg->name = (char *)malloc( len + 1 );
                        strncpy( cur_pkg->name, tmp, len );
                        cur_pkg->name[len] = 0;

                        if( version2 )
                        {
                            len = strlen( version2 );
                            cur_pkg->version = (char *)malloc( len + 1 );
                            strncpy( cur_pkg->version, version2, len );
                            cur_pkg->version[len] = 0;
                        }
                        else
                        {
                            cur_pkg->version = NULL;
                        }

                        cur_pkg->type = 3;
                        cur_pkg->prev = list;
                        list = cur_pkg;

                        sub_list = packages_get_recommended_list( pm, cur_pkg->name, cur_pkg->version );
                        if( sub_list )
                        {
                            tmp_list = sub_list;
                            while( tmp_list->prev )
                                tmp_list = tmp_list->prev;
                            tmp_list->prev = list;
                            list = sub_list;
                        }
                    }
                    free( tmp );
                    version2 = NULL;
                    token = strtok_r( NULL, " ,", &saveptr );
                }
            }
        }
        packages_free_package_data( pkg_data );
    }
    packages_clist_remove_duplicate_item( list );
    return list;
}

YPackageChangeList *packages_clist_remove_duplicate_item( YPackageChangeList *change_list )
{
    YPackageChangeList    *cur_pkg, *cmp_pkg, *tmp_pkg;

    cur_pkg = change_list;
    cmp_pkg = cur_pkg;

    while( cur_pkg )
    {
        while( cmp_pkg )
        {
            if( cmp_pkg->prev && (!strcmp( cur_pkg->name, cmp_pkg->prev->name ) ) )
            {
                tmp_pkg = cmp_pkg->prev;
                cmp_pkg->prev = cmp_pkg->prev->prev;
                free( tmp_pkg->name );
                if( tmp_pkg->version )
                    free( tmp_pkg->version );
                free( tmp_pkg );
            }
            cmp_pkg = cmp_pkg->prev;
        }

        cur_pkg = cur_pkg->prev;
        cmp_pkg = cur_pkg;
    }

    return change_list;
}

YPackageChangeList *packages_clist_append( YPackageChangeList *list_s, YPackageChangeList *list_d )
{
    YPackageChangeList      *cur_pkg;

    cur_pkg = list_d;
    while( cur_pkg->prev )
    {
        cur_pkg = cur_pkg->prev;
    }

    cur_pkg->prev = list_s;

    return list_d;
}

YPackageChangeList *packages_get_bdepend_list( YPackageManager *pm, char *package_name, char *version )
{
    int                     i, len;
    char                    *token, *version2, *saveptr, *depend, *dev, *bdepend, *tmp;
    YPackageData            *pkg_data;
    YPackageChangeList      *list, *cur_pkg;

    list = NULL;


    if( (pkg_data = packages_get_package_data( pm, package_name, 0 )) )
    {
        for( i = 0; i < pkg_data->cnt; i++ )
        {
            depend = packages_get_package_data_attr( pkg_data, i, "data_depend");
            if( depend )
            {
                token = strtok_r( depend, " ,", &saveptr );
                while( token )
                {
                    tmp = util_strcat( token, NULL );
                    if( (version2 = strchr( tmp, '(' )) )
                    {
                        *version2 = 0;
                    }
                    

                    dev = util_strcat( tmp, "-dev", NULL );
                    free( tmp );
                    if( !packages_exists( pm, dev, NULL ) )
                    {
                        free( dev );
                        token = strtok_r( NULL, " ,", &saveptr );
                        continue;
                    }

                    if( !packages_has_installed( pm, dev, NULL ) )
                    {
                        cur_pkg =  (YPackageChangeList *)malloc( sizeof( YPackageChangeList ) );
                        len = strlen( dev );
                        cur_pkg->name = (char *)malloc( len + 1 );
                        strncpy( cur_pkg->name, dev, len );
                        cur_pkg->name[len] = 0;
                        cur_pkg->version = NULL;
                        cur_pkg->type = 2;
                        cur_pkg->prev = list;
                        list = cur_pkg;
                    }
                    free( dev );
                    token = strtok_r( NULL, " ,", &saveptr );
                }
            }

            bdepend = packages_get_package_data_attr( pkg_data, i, "data_bdepend");
            if( bdepend )
            {
                token = strtok_r( bdepend, " ,", &saveptr );
                while( token )
                {
                    if( !packages_has_installed( pm, token, NULL ) )
                    {
                        cur_pkg =  (YPackageChangeList *)malloc( sizeof( YPackageChangeList ) );
                        len = strlen( token );
                        cur_pkg->name = (char *)malloc( len + 1 );
                        strncpy( cur_pkg->name, token, len );
                        cur_pkg->name[len] = 0;
                        cur_pkg->version = NULL;
                        cur_pkg->type = 2;
                        cur_pkg->prev = list;
                        list = cur_pkg;
                    }
                    token = strtok_r( NULL, " ,", &saveptr );
                }
            }

        }
        packages_free_package_data( pkg_data );
    }

    //build-essential
    if( !packages_has_installed( pm, "build-essential", NULL ) )
    {
        cur_pkg =  (YPackageChangeList *)malloc( sizeof( YPackageChangeList ) );
        len = 15;
        cur_pkg->name = (char *)malloc( len + 1 );
        strncpy( cur_pkg->name, "build-essential", len );
        cur_pkg->name[len] = 0;
        cur_pkg->version = NULL;
        cur_pkg->type = 2;
        cur_pkg->prev = list;
        list = cur_pkg;
    }

    packages_clist_remove_duplicate_item( list );
    return list;
}

void packages_free_change_list( YPackageChangeList *list )
{
    YPackageChangeList    *cur_pkg;

    if( !list )
        return;


    while( list )
    {
        cur_pkg = list;
        list = list->prev;

        free( cur_pkg->name );

        if( cur_pkg->version )
            free( cur_pkg->version );

        free( cur_pkg );
    }
}

/*
 * packages_get_install_list
 */
YPackageChangeList *packages_get_install_list( YPackageManager *pm, char *package_name, char *version )
{
    int             len;
    YPackageChangeList    *list, *depend, *cur_pkg;

    if( packages_has_installed( pm, package_name, version ) )
    {
        return NULL;
    }
    
    list = NULL;

    depend = packages_get_recommended_list( pm, package_name, version );
    if( depend )
    {
        cur_pkg = depend;
        while( cur_pkg->prev )
            cur_pkg = cur_pkg->prev;
        cur_pkg->prev = list;
        list = depend;
    }
    
    cur_pkg =  (YPackageChangeList *)malloc( sizeof( YPackageChangeList ) );
    len = strlen( package_name );
    cur_pkg->name = (char *)malloc( len + 1 );
    strncpy( cur_pkg->name, package_name, len );
    cur_pkg->name[len] = 0;

    if( version )
    {
        len = strlen( version );
        cur_pkg->version = (char *)malloc( len + 1 );
        strncpy( cur_pkg->version, version, len );
        cur_pkg->version[len] = 0;
    }
    else
    {
        cur_pkg->version = NULL;
    }

    cur_pkg->type = 1;
    cur_pkg->prev = list;
    list = cur_pkg;

    depend = packages_get_depend_list( pm, package_name, version );
    if( depend )
    {
        cur_pkg = depend;
        while( cur_pkg->prev )
            cur_pkg = cur_pkg->prev;
        cur_pkg->prev = list;
        list = depend;
    }


    return list;
}

void packages_free_install_list( YPackageChangeList *list )
{
    packages_free_change_list( list );
}

/*
 * packages_get_dev_list
 */
YPackageChangeList *packages_get_dev_list( YPackageManager *pm, char *package_name, char *version )
{
    return packages_get_bdepend_list( pm, package_name, version );
}

void packages_free_dev_list( YPackageChangeList *list )
{
    packages_free_change_list( list );
}


/*
 * packages_install_list
 */
int packages_install_list( YPackageManager *pm, YPackageChangeList *list, ypk_progress_callback cb, void *cb_arg )
{
    int                     ret;
    YPackageChangeList      *cur_pkg;

    if( !list )
        return 0;


    while( list )
    {
        cur_pkg = list;
        
        ret = packages_install_package( pm, cur_pkg->name,cur_pkg->version, cb, cb_arg );
        if( ret )
        {
            return -1;
        }
        list = list->prev;
    }

    return 0;
}


/*
 * package_get_remove_list
 */
YPackageChangeList *packages_get_remove_list( YPackageManager *pm, char *package_name, int depth )
{
    int                     i, len;
    char                    *name, *size;
    YPackageList            *pkg_list;
    YPackageChangeList      *list, *sub_list, *cur_pkg, *tmp_pkg;


    if( depth > 4 )
        return NULL;

    if( !packages_has_installed( pm, package_name, NULL ) )
    {
        return NULL;
    }

    list = NULL;

    cur_pkg =  (YPackageChangeList *)malloc( sizeof( YPackageChangeList ) );
    len = strlen( package_name );
    cur_pkg->name = (char *)malloc( len + 1 );
    strncpy( cur_pkg->name, package_name, len );
    cur_pkg->name[len] = 0;
    cur_pkg->version = NULL;
    cur_pkg->type = depth ? 2 : 1;
    cur_pkg->prev = NULL;
    list = cur_pkg;

    pkg_list = packages_get_list_by_depend( pm, 2000, 0, package_name, 1 );
    if( pkg_list )
    {
        for( i = 0; i < pkg_list->cnt; i++ )
        {
            name =  packages_get_list_attr( pkg_list, i, "name");

            if( !packages_has_installed( pm, name, NULL ) )
            {
                continue;
            }

            size =  packages_get_list_attr( pkg_list, i, "size");
            cur_pkg =  (YPackageChangeList *)malloc( sizeof( YPackageChangeList ) );
            len = strlen( name );
            cur_pkg->name = (char *)malloc( len + 1 );
            strncpy( cur_pkg->name, name, len );
            cur_pkg->name[len] = 0;
            cur_pkg->version = NULL;
            cur_pkg->type = 2;
            cur_pkg->size = atoi( size );
            cur_pkg->prev = list;
            list = cur_pkg;
        }
        packages_free_list( pkg_list );
    }

    if( list )
    {
        cur_pkg = list;
        while( cur_pkg && cur_pkg->prev )
        {
            sub_list = packages_get_remove_list( pm, cur_pkg->name, depth+1 );
            if( sub_list )
            {
                tmp_pkg = cur_pkg->prev;
                packages_clist_append( tmp_pkg, sub_list );
                cur_pkg->prev = sub_list;
                cur_pkg = tmp_pkg;
            }
            else
            {
                cur_pkg = cur_pkg->prev;
            }
        }
    }

    packages_clist_remove_duplicate_item( list );

    return list;
}


/*
 * packages_remove_list
 */
int packages_remove_list( YPackageManager *pm, YPackageChangeList *list, ypk_progress_callback cb, void *cb_arg  )
{
    YPackageChangeList    *cur_pkg;

    if( !list )
        return -1;


    while( list )
    {
        cur_pkg = list;
        
        //printf("removing %s ...\n", cur_pkg->name );
        packages_remove_package( pm, cur_pkg->name, cb, cb_arg );
        list = list->prev;
    }
    return 0;
}

void packages_free_remove_list( YPackageChangeList *list )
{
    packages_free_change_list( list );
}

int packages_check_depend( YPackageManager *pm, YPackageData *pkg_data, char *extra, int extra_max_len )
{
    int                 i, len, offset = 0, ret = 0;
    char                *depend, *token, *saveptr, *version, *tmp, *tmp2;

    if( !pkg_data )
        return -1;

    for( i = 0; i < pkg_data->cnt; i++ )
    {
        tmp = packages_get_package_data_attr( pkg_data, i, "data_depend");
        if( tmp )
        {
            depend = strdup( tmp );
            tmp = NULL;
            version = NULL;
            token = strtok_r( depend, " ,", &saveptr );
            while( token )
            {
                tmp = util_strcat( token, NULL );
                if( (version = strchr( tmp, '(' )) )
                {
                    *version++ = 0;

                    while( *version == ' ' )
                        version++;

                    if( (tmp2 = strchr( version, ')' )) )
                        *tmp2 = 0;
                }

                if( !packages_has_installed( pm, tmp, version ) )
                {
                    if( extra && extra_max_len > 0 )
                    {
                        len = strlen( token );
                        if( extra_max_len > offset + len + 2 )
                        {
                            strncpy( extra + offset, token, len );
                            offset += len;
                            extra[offset] = ' ';
                            extra[++offset] = 0;
                        }

                    }

                    ret = -1;
                }

                free( tmp );
                version = NULL;
                token = strtok_r( NULL, " ,", &saveptr );
            }
            free( depend );
        }
    }

    return ret;
}

int packages_check_conflict( YPackageManager *pm, YPackageData *pkg_data, char *extra, int extra_max_len )
{
    int                 i, len, offset = 0, ret = 0;
    char                *conflict, *token, *saveptr, *version, *tmp, *tmp2;

    if( !pkg_data )
        return -1;

    for( i = 0; i < pkg_data->cnt; i++ )
    {
        tmp = packages_get_package_data_attr( pkg_data, i, "data_conflict");
        if( tmp )
        {
            conflict = strdup( tmp );
            tmp = NULL;
            version = NULL;
            token = strtok_r( conflict, " ,", &saveptr );
            while( token )
            {
                tmp = util_strcat( token, NULL );
                if( (version = strchr( tmp, '(' )) )
                {
                    *version++ = 0;

                    while( *version == ' ' )
                        version++;

                    if( (tmp2 = strchr( version, ')' )) )
                        *tmp2 = 0;
                }

                if( packages_has_installed( pm, tmp, version ) )
                {
                    if( extra && extra_max_len > 0 )
                    {
                        len = strlen( token );
                        if( extra_max_len > offset + len + 2 )
                        {
                            strncpy( extra + offset, token, len );
                            offset += len;
                            extra[offset] = ' ';
                            extra[++offset] = 0;
                        }
                    }

                    ret = -1;
                }
                free( tmp );
                version = NULL;
                token = strtok_r( NULL, " ,", &saveptr );
            }
            free( conflict );
        }
    }

    return ret;
}

/*
 * packages_check_package
 */
int packages_check_package( YPackageManager *pm, char *ypk_path, char *extra, int extra_max_len )
{
    int                 ret;
    YPackage            *pkg = NULL;
    YPackageData        *pkg_data = NULL;
    YPackageFile        *pkg_file = NULL;

    if( !ypk_path || access( ypk_path, R_OK ) )
        return -1;

    //get ypk's infomations
    if( packages_get_info_from_ypk( ypk_path, &pkg, &pkg_data, &pkg_file, NULL, NULL ) != 0 )
    {
        return -1;
    }
    packages_free_package_file( pkg_file );

    ret = packages_check_package2( pm, pkg, pkg_data, extra, extra_max_len );

    if( pkg )
        packages_free_package( pkg );

    if( pkg_data )
        packages_free_package_data( pkg_data );

    return ret;
}


int packages_check_package2( YPackageManager *pm, YPackage *pkg, YPackageData *pkg_data, char *extra, int extra_max_len )
{
    int                 ret, return_code = 0;
    char                *package_name, *arch, *version, *version2, *repo, *repo2;
    struct utsname      buf;
    YPackage            *pkg2 = NULL;


    if( !pkg || !pkg_data )
        return -1;

    package_name = packages_get_package_attr( pkg, "name" );
    arch = packages_get_package_attr( pkg, "arch" );
    version = packages_get_package_attr( pkg, "version" );
    repo = packages_get_package_attr( pkg, "repo" );

    //check arch
    if( arch && ( arch[0] != 'a' || arch[1] != 'n' || arch[2] != 'y' ) )
    {
        if( !uname( &buf ) )
        {
            if( strcmp( buf.machine, arch ) )
            {
                return_code = -2;

                if( extra && extra_max_len > 0 )
                {
                    strncpy( extra, buf.machine, extra_max_len );
                }

                goto exception_handler;
            }
        }
    }

    if( packages_check_depend( pm, pkg_data, extra, extra_max_len ) == -1 )
    {
        return_code = -3; 
        goto exception_handler;
    }


    if( packages_check_conflict( pm, pkg_data, extra, extra_max_len ) == -1 )
    {
        return_code = -4; 
        goto exception_handler;
    }

    //check installed
    if( (pkg2 = packages_get_package( pm, package_name, 1 ))  )
    {
        version2 = packages_get_package_attr( pkg2, "version" );
        repo2 = packages_get_package_attr( pkg2, "repo" );
        if( version && (strlen( version ) > 0) && version2 && (strlen( version2 ) > 0) )
        {
            ret = packages_compare_version( version, version2 );
            if( ret > 0 )
            {
                /*
                if( repo && repo2 && strcmp( repo, repo2 ) )
                {
                    return_code = 3; 
                }
                else
                {
                    return_code = 2; 
                }
                */

                    return_code = 3; 
            }
            else if( ret == 0 )
            {
                return_code = 2; 
            }
            else
            {
                if( repo && repo2 && strcmp( repo, repo2 ) )
                {
                    return_code = 1; 
                }
                else
                {
                    return_code = 2; 
                }
            }

            if( extra && extra_max_len > 0 )
            {
                strncpy( extra, version2, extra_max_len );
            }

            goto exception_handler;
        }
    }

exception_handler:
    if( pkg2 )
        packages_free_package( pkg2 );

    return return_code;
}

/*
 * packages_unpack_package
 *
 * params: unzip_info - 0 data, 1 data & info, 2 info
 */
int packages_unpack_package( char *ypk_path, char *dest_dir, int unzip_info, char *suffix )
{
    char                tmp_ypk_data[] = "/tmp/ypkdata.XXXXXX", tmp_ypk_info[] = "/tmp/ypkinfo.XXXXXX";
    char                *info_dest_dir;

    if( !ypk_path )
        return -1;

    if( !dest_dir )
        dest_dir = "./";

    //unzip pkgdata
    if( unzip_info < 2 )
    {
        mkstemp( tmp_ypk_data );
        if( archive_extract_file( ypk_path, "pkgdata", tmp_ypk_data ) == -1 )
        {
            return -2;
        }

        //copy files 
        //printf( "unpacking files ...\n");
        if( archive_extract_all( tmp_ypk_data, dest_dir, suffix ) == -1 )
        {
            remove( tmp_ypk_data );
            return -3;
        }
        
        remove( tmp_ypk_data );
    }

    //unzip pkginfo
    if( unzip_info )
    {
        mkstemp( tmp_ypk_info );
        if( archive_extract_file( ypk_path, "pkginfo", tmp_ypk_info ) == -1 )
        {
            return -4;
        }

        util_rtrim( dest_dir, '/' );
        info_dest_dir = util_strcat( dest_dir, "/YLMFOS", NULL );

        if( archive_extract_all( tmp_ypk_info, info_dest_dir, suffix ) == -1 )
        {
            free(info_dest_dir);
            remove( tmp_ypk_info );
            return -5;
        }
        
        free(info_dest_dir);
        remove( tmp_ypk_info );
    }

    return 0;
}

/*
 * packages_pack_package
 */
int packages_pack_package( char *source_dir, char *ypk_path, ypk_progress_callback cb, void *cb_arg )
{
    int                 ret, data_size, control_xml_size, del_var;
    time_t              now;
    char                *pkginfo, *pkgdata, *info_path, *control_xml, *control_xml_content, *filelist, *data_size_str, *time_str, *uri_str, *install, *install_script, *install_script_dest, *package_name, *version, *arch, *msg, *tmp;
    char                tmp_ypk_dir[] = "/tmp/ypkdir.XXXXXX";
    char                *exclude[] = { "YLMFOS", NULL };
    FILE                *fp_xml;
    xmlDocPtr           xmldoc = NULL;


    if( !source_dir )
        return -1;

    if( access( source_dir, R_OK | X_OK ) == -1 )
        return -1;

    if( !mkdtemp( tmp_ypk_dir ) )
        return -1;

    ret = 0;
    del_var = 0;
    pkginfo = NULL; 
    pkgdata = NULL; 
    info_path = NULL; 
    control_xml = NULL;
    filelist = NULL;
    install_script = NULL; 
    install_script_dest = NULL; 
    package_name = NULL; 
    version = NULL;
    arch = NULL;
    time_str = NULL;
    data_size_str = NULL;
    msg = NULL;

    //check control.xml
    control_xml = util_strcat( source_dir, "/YLMFOS/control.xml", NULL );
    if( access( control_xml, R_OK | W_OK ) == -1 )
    {
        ret = -1;
        goto cleanup;
    }

	if( ( xmldoc = xpath_open( control_xml ) ) == (xmlDocPtr)-1 )
    {
        ret = -2;
        goto cleanup;
    }

    package_name =  xpath_get_node( xmldoc, (xmlChar *)"//Package/@name" );
    if( !package_name )
    {
        ret = -2;
        goto cleanup;
    }

    version =  xpath_get_node( xmldoc, (xmlChar *)"//version" );
    if( !version )
    {
        ret = -2;
        goto cleanup;
    }

    arch =  xpath_get_node( xmldoc, (xmlChar *)"//arch" );
    if( !arch )
    {
        arch = strdup( "i686" );
    }

    install = xpath_get_node( xmldoc, (xmlChar *)"//install" );

    if( cb )
    {
        msg = util_strcat( "building package  `", package_name, ":", version, ":", arch, "`  in `", package_name, "_", version, "-", arch, ".ypk'\n", NULL );
        cb( cb_arg, "*", 0, 1, msg );
    }

    filelist = util_strcat( source_dir, "/YLMFOS/filelist", NULL );
    if( access( filelist, R_OK ) == -1 )
    {
        ret = -3;
        goto cleanup;
    }

    //copy install script
    if( install )
    {
        install_script = util_strcat( source_dir, "/YLMFOS/", install, NULL );
        if( access( install_script, R_OK ) != -1 )
        {
            install_script_dest = util_strcat( source_dir, "/var", NULL );
            if( access( install_script_dest, F_OK ) ) //not exists
            {
                del_var = 1;
            }
            free( install_script_dest );

            install_script_dest = util_strcat( source_dir, "/var/ypkg/db/", package_name, NULL );
            if( !util_mkdir( install_script_dest ) )
            {
                free( install_script_dest );
                install_script_dest = util_strcat( source_dir, "/var/ypkg/db/", package_name, "/", install, NULL );
                util_copy_file( install_script, install_script_dest );
            }
            free( install_script_dest );

        }
        free( install_script );
    }

    //pack pkgdata
    pkgdata = util_strcat( tmp_ypk_dir, "/", "pkgdata", NULL );
    ret = archive_create( pkgdata, 'J', 'c',  source_dir, exclude );

    if( del_var )
    {
        install_script_dest = util_strcat( source_dir, "/var", NULL );
    }
    else
    {
        install_script_dest = util_strcat( source_dir, "/var/ypkg", NULL );
    }
    util_remove_dir( install_script_dest );
    free( install_script_dest );

    if( ret == -1 )
    {
        free( pkgdata );
        ret = -4;
        goto cleanup;
    }

    //update control.xml
    if( (fp_xml = fopen( control_xml, "r+" )) )
    {
        control_xml_size = util_file_size( control_xml );
        control_xml_content = malloc( control_xml_size + 1 );
        memset( control_xml_content, 0, control_xml_size + 1 );
        if( control_xml_content )
        {
            if( fread( control_xml_content, 1, control_xml_size, fp_xml ) == control_xml_size )
            {
                now = time( NULL );
                if( now )
                {
                    tmp = util_int_to_str( now );
                    time_str = util_strcat( "<build_date>", tmp, "</build_date>", NULL );
                    free( tmp );
                }

                data_size = util_file_size( pkgdata );
                if( data_size )
                {
                    tmp = util_int_to_str( data_size );
                    data_size_str = util_strcat( "<size>", tmp, "</size>", NULL );
                    free( tmp );
                }

                tmp = preg_replace( "<build_date>.*?</build_date>", time_str, control_xml_content, PCRE_CASELESS, 1 );
                free( control_xml_content );
                free( time_str );
                control_xml_content = tmp;

                tmp = preg_replace( "<size>.*?</size>", data_size_str, control_xml_content, PCRE_CASELESS, 1 );
                free( control_xml_content );
                free( data_size_str );
                control_xml_content = tmp;

                uri_str =  xpath_get_node( xmldoc, (xmlChar *)"//uri" );
                if( !uri_str )
                {
                    tmp = strdup( package_name );
                    tmp[1] = 0;
                    uri_str = util_strcat( "<uri>", tmp, "/", package_name, "_", version, "-", arch, ".ypk</uri>", NULL );
                    free( tmp );
                    tmp = preg_replace( "<uri>.*?</uri>", uri_str, control_xml_content, PCRE_CASELESS, 1 );
                    if( tmp )
                    {
                        if( strlen( tmp ) > 1000 )
                        {
                            free( control_xml_content );
                            control_xml_content = tmp;
                        }
                        else
                        {
                            free( tmp );
                        }
                    }
                    free( uri_str );
                }

                tmp = NULL;

                rewind( fp_xml );
                control_xml_size = strlen( control_xml_content );
                fwrite( control_xml_content, control_xml_size, 1, fp_xml );
            }
            free( control_xml_content );
        }
        fclose( fp_xml );
        truncate( control_xml, control_xml_size );
    }

    free( pkgdata );

    //pack pkginfo
    info_path = util_strcat( source_dir, "/YLMFOS", NULL );
    pkginfo = util_strcat( tmp_ypk_dir, "/pkginfo", NULL );
    ret = archive_create( pkginfo, 'j', 't',  info_path, NULL );
    free( pkginfo );
    free( info_path );

    if( ret == -1 )
    {
        ret = -5;
        goto cleanup;
    }

    //pack ypk
    if( !ypk_path )
    {
        tmp = util_strcat( package_name, "_", version, "-", arch, ".ypk", NULL );
        if( archive_create( tmp, 'j', 't', tmp_ypk_dir, NULL ) )
            ret = -6;
        free( tmp );
    }
    else
    {
        if( archive_create( ypk_path, 'j', 't', tmp_ypk_dir, NULL ) )
            ret = -6;
    }

cleanup:
    if( control_xml )
        free( control_xml );

    if( filelist )
        free( filelist );

    if( package_name )
        free( package_name );

    if( version )
        free( version );

    if( arch )
        free( arch );

    util_remove_dir( tmp_ypk_dir );

    if( xmldoc )
    {
        xmlFreeDoc( xmldoc );
        xmlCleanupParser();
    }
    return ret;
}

/*
 * compare version
 */
int packages_compare_version( char *version1, char *version2 )
{
    int         buf_size = 64, result;
    char        *pattern = "(?<ver>(.*?)+)(-(?<ver1>.*?))?(-(?<ver2>.*?))?(-(?<ver3>.*?))?$";
    char        buf1[buf_size], buf2[buf_size], *sub_ver1, *sub_ver2;
    PREGInfo    pi1, pi2;

    if( preg_match( &pi1, pattern, version1, 0, 1 ) > 0 &&  preg_match( &pi2, pattern, version2, 0, 1 ) > 0 )
    {
        memset( buf1, 0, buf_size );
        memset( buf2, 0, buf_size );
        if( preg_result2(&pi1, "ver", buf1, buf_size) > 0 && preg_result2(&pi2, "ver", buf2, buf_size) > 0 )
        {
            result = packages_compare_main_version( buf1, buf2 );
            if( !result )
            {
                if( preg_result2(&pi1, "ver1", buf1, buf_size) > 0 )
                    sub_ver1 = buf1;
                else
                    sub_ver1 = NULL;


                if( preg_result2(&pi2, "ver1", buf2, buf_size) > 0 )
                    sub_ver2 = buf2;
                else
                    sub_ver2 = NULL;
                

                result = packages_compare_sub_version( sub_ver1, sub_ver2 );
                if( !result )
                {
                    if( preg_result2(&pi1, "ver2", buf1, buf_size) > 0 )
                        sub_ver1 = buf1;
                    else
                        sub_ver1 = NULL;

                    if( preg_result2(&pi2, "ver2", buf2, buf_size) > 0 )
                        sub_ver2 = buf2;
                    else
                        sub_ver2 = NULL;

                    result = packages_compare_sub_version( sub_ver1, sub_ver2 );
                    if( !result )
                    {
                        if( preg_result2(&pi1, "ver3", buf1, buf_size) > 0 )
                            sub_ver1 = buf1;
                        else
                            sub_ver1 = NULL;

                        if( preg_result2(&pi2, "ver3", buf2, buf_size) > 0 )
                            sub_ver2 = buf2;
                        else
                            sub_ver2 = NULL;

                        result = packages_compare_sub_version( sub_ver1, sub_ver2 );
                    }
                }
            }


            if( result && result != -2 )
            {
                result = result > 0 ? 1 : -1;
            }

        }
        else
        {
            result = -2;
        }
    }
    else
    {
        result = -2;
    }
    preg_free( &pi1 );
    preg_free( &pi2 );

    return result;
}

int packages_compare_main_version( char *version1, char *version2 )
{
    int         ver1, ver2, ret1, ret2, result = 0, buf_size = 16;
    char        *pattern = "(\\d+)";
    char        buf1[buf_size], buf2[buf_size];
    PREGInfo    pi1, pi2;

    ret1 = preg_match( &pi1, pattern, version1, 0, 1 );
    ret2 = preg_match( &pi2, pattern, version2, 0, 1 );

    while( ret1 > 0 &&  ret2 > 0 )
    {
        memset( buf1, 0, buf_size );
        memset( buf2, 0, buf_size );
        preg_result(&pi1, 0, buf1, buf_size); 
        preg_result(&pi2, 0, buf2, buf_size);

        ver1 = atoi( buf1 );
        ver2 = atoi( buf2 );
        if( (result =  ver1 - ver2) )
        {
            result = result > 0 ? 1 : -1;
            break;
        }

        ret1 = preg_match( &pi1, pattern, version1, 0, 0 );
        ret2 = preg_match( &pi2, pattern, version2, 0, 0 );
    }

    if( ret1 > 0 && ret2 < 0 )
        result = 1;
    else if( ret1 < 0 && ret2 > 0 )
        result = -1;

    if( result == 0 )
    {
        result = strcmp( version1, version2 );
    }

    preg_free( &pi1 );
    preg_free( &pi2 );

    return result;
}

int packages_compare_sub_version( char *version1, char *version2 )
{
    int level1, level2;


    if( version1 == NULL )
        level1 = 4;
    else if( isdigit( version1[0] ) )
        level1 = 7;
    else if( version1[0] == 'r' && isdigit( version1[1] ) )
        level1 = 6;
    else if( !strncmp( version1, "ylmf", 4 ) && isdigit( version1[4] ) )
        level1 = 5;
    else if( version1[0] == 'r' && version1[1] == 'c' && isdigit( version1[2] ) )
        level1 = 3;
    else if( !strncmp( version1, "beta", 4 ) && isdigit( version1[4] ) )
        level1 = 2;
    else if( !strncmp( version1, "alpha", 5 ) && isdigit( version1[5] ) )
        level1 = 1;
    else 
        level1 = 0;

    if( version2 == NULL )
        level2 = 4;
    else if( isdigit( version2[0] ) )
        level2 = 7;
    else if( version2[0] == 'r' && isdigit( version2[1] ) )
        level2 = 6;
    else if( !strncmp( version2, "ylmf", 4 ) && isdigit( version2[4] ) )
        level2 = 5;
    else if( version2[0] == 'r' && version2[1] == 'c' && isdigit( version2[2] ) )
        level2 = 3;
    else if( !strncmp( version2, "beta", 4 ) && isdigit( version2[4] ) )
        level2 = 2;
    else if( !strncmp( version2, "alpha", 5 ) && isdigit( version2[5] ) )
        level2 = 1;
    else 
        level2 = 0;


    if( level1 == level2 )
    {
        if( level1 == 4 )
            return 0;
        else if( level1 == 7 )
            return atoi( version1 ) - atoi( version2 ); // 2 > 1
        else if( level1 == 6 )
            return atoi( version1+1 ) - atoi( version2+1 ); // r2 > r1
        else if( level1 == 5 )
            return atoi( version1+4 ) - atoi( version2+4 ); //ylmf
        else if( level1 == 3 )
            return atoi( version1+2 ) - atoi( version2+2 ); //rc
        else if( level1 == 2 )
            return atoi( version1+4 ) - atoi( version2+4 );
        else if( level1 == 1 )
            return atoi( version1+5 ) - atoi( version2+5 );
        else
            return 0;
    }
    else
    {
        return level1 - level2; // foo_1.0-1 > foo_1.0-r1 > foo_1.0-ylmf1 > foo_1.0 > foo_1.0-rc1
    }
}


/*
 * packages_exec_script
 */
int packages_exec_script( char *script, char *package_name, char *version, char *version2, char *action )
{
    int     status;
    char    *cmd;


    if( !script || !package_name || !version || !action )
        return -1;

    if( access( script, R_OK ) != 0 )
        return -1;

    cmd = util_strcat( "source '", script, "'; if type ", action, " >/dev/null 2>&1; then ", action, " ", package_name, " ", version, " ", version2 ? version2 : "", "; fi", NULL );
    //printf( "exec: %s\n", cmd );
    status = system( cmd );
    free( cmd );

    if( WEXITSTATUS( status ) )
    {
        return -1;
    }
    return 0;
}

/*
 * packages_install_local_package
 */
int packages_install_local_package( YPackageManager *pm, char *ypk_path, char *dest_dir, int force, ypk_progress_callback cb, void *cb_arg  )
{
    int                 i, j, installed, upgrade, delete_file, ret, return_code;
    void                *filelist;
    char                *msg, *sql, *sql_data, *sql_filelist;
    char                *package_name, *version, *version2, *repo, *repo2, *install, *file_type, *file_file, *file_size, *file_perms, *file_uid, *file_gid, *file_mtime, *file_extra, *can_update, *tmp_file, *replace_name, *replace_file, *replace_with;
    char                tmp_ypk_install[] = "/tmp/ypkinstall.XXXXXX";
    char                extra[128];
    YPackage            *pkg, *pkg2, *pkg3;
    YPackageData        *pkg_data;
    YPackageFile        *pkg_file, *pkg_file2;
    YPackageReplaceFileList *replace_list;
    DB                  db;


    installed = 0;
    upgrade = 0;
    return_code = 0;
    msg = NULL;
    filelist = NULL;
    pkg = NULL; 
    pkg2 = NULL; 
    pkg3 = NULL; 
    pkg_data = NULL;
    pkg_file = NULL;
    pkg_file2 = NULL;
    replace_list = NULL;
    package_name = NULL;
    install = NULL;
    replace_with = NULL;

    //check
    if( !ypk_path || access( ypk_path, R_OK ) )
        return -1;

    if( cb )
    {
        cb( cb_arg, ypk_path, 4, -1, "Checking dependency tree" );
    }

    mkstemp( tmp_ypk_install );
    ret = packages_get_info_from_ypk( ypk_path, &pkg, &pkg_data, &pkg_file, tmp_ypk_install, NULL );
    if( ret != 0 )
    {
        msg = util_strcat( "Error: invalid format or file not found.\n", NULL );
        cb( cb_arg, ypk_path, 3, 1, msg );
        free( msg );
        msg = NULL;

        return_code = -1;
        goto exception_handler;
    }

    memset( extra, 0, 128 );
    ret = packages_check_package2( pm, pkg, pkg_data, extra, 128 );

    if( cb )
    {
        switch( ret )
        {
            case 1:
            case 2:
                msg = util_strcat( "Newer or same version installed", NULL );
                break;

            case -1:
                msg = util_strcat( "Error: invalid format or file not found", NULL );
                break;

            case -2:
                msg = util_strcat( "Error: architecture mismatched. (",  extra, ")", NULL );
                break;

            case -3:
                msg = util_strcat( "Error: missing runtime dependencies: ", extra, NULL );
                break;

            case -4:
                msg = util_strcat( "Error: conflicting dependencies found: ", extra, NULL );
                break;

        }

        //cb( cb_arg, ypk_path, 3, 1, msg ? msg : "done" );
        cb( cb_arg, ypk_path, 4, 1, NULL );

        if( msg )
        {
            free( msg );
            msg = NULL;
        }
    }


    if( ret == -1 )
    {
        return -1;
    }
    else if( ret < -1 && !force )
    {
        return ret;
    }
    else if( ret == 1 )
    {
        if( !force )
            return 1;

        installed = 1;
        upgrade = -1;
    }
    else if( ret == 2 )
    {
        if( !force )
            return 1;

        installed = 1;
    }
    else if( ret == 3 )
    {
        installed = 1;
        upgrade = 1;
    }



    if( !dest_dir )
        dest_dir = "/";


    //get package infomations
    if( cb )
    {
        cb( cb_arg, ypk_path, 4, -1, "Reading package information" );
    }


    package_name = packages_get_package_attr( pkg, "name" );
    version = packages_get_package_attr( pkg, "version" );
    repo = packages_get_package_attr( pkg, "repo" );
    install = packages_get_package_attr( pkg, "install" );

    if( repo && !strcmp( repo, "stable" ) )
        pkg2 = packages_get_repo_package( pm, package_name, 0, "stable" );

    pkg3 = packages_get_repo_package( pm, package_name, 0, "testing" );

    pkg_file2 = packages_get_package_file( pm, package_name );

    replace_list = packages_get_replace_file_list( pm, pkg_data, pkg_file );

    if( cb )
    {
        cb( cb_arg, ypk_path, 4, 1, NULL );
    }

    //exec pre_x script
    if( cb )
    {
        cb( cb_arg, ypk_path, 5, -1, "Executing pre_install script\n" );
    }

    if( !access( tmp_ypk_install, R_OK ) )
    {
        if( !upgrade )
        {
            if( packages_exec_script( tmp_ypk_install, package_name, version, NULL, "pre_install" ) == -1 )
            {
                return_code = -7; 
                goto exception_handler;
            }
        }
        else if( upgrade == 1 )
        {
            version2 = extra;
            if( packages_exec_script( tmp_ypk_install, package_name, version, version2, "pre_upgrade" ) == -1 )
            {
                return_code = -7; 
                goto exception_handler;
            }
        }
        else //downgrade
        {
            version2 = extra;
            if( packages_exec_script( tmp_ypk_install, package_name, version, version2, "pre_downgrade" ) == -1 )
            {
                return_code = -7; 
                goto exception_handler;
            }
        }
    }

    if( cb )
    {
        cb( cb_arg, ypk_path, 5, 1, NULL );
    }

    //copy files 
    if( cb )
    {
        cb( cb_arg, ypk_path, 6, -1, "Copying files" );
    }

    if( (ret = packages_unpack_package( ypk_path, dest_dir, 0, "~ypk" )) != 0 )
    {
        //printf("unpack ret:%d\n",ret);
        return_code = -8; 
        goto exception_handler;
    }


    if( cb )
    {
        cb( cb_arg, ypk_path, 6, 1, NULL );
    }


    //update db
    if( cb )
    {
        cb( cb_arg, ypk_path, 8, -1, "Updating database" );
    }

    ret = db_init( &db, pm->db_name, OPEN_WRITE );
    db_cleanup( &db );
    db_exec( &db, "begin", NULL );  

    //update world
    sql = "replace into world (name, generic_name, is_desktop, category, arch, version, priority, install, exec, license, homepage, repo, size, sha, build_date, packager, uri, description, data_count, install_time) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, strftime('%s','now'));";


    ret = db_exec( &db, sql,  
            package_name, //name
            packages_get_package_attr2( pkg, "generic_name"), //generic_name
            packages_get_package_attr2( pkg, "is_desktop"), //desktop
            packages_get_package_attr2( pkg, "category" ), //category
            packages_get_package_attr2( pkg, "arch" ), //arch
            version, //version
            packages_get_package_attr2( pkg, "priority" ), //priority
            packages_get_package_attr2( pkg, "install" ), //install
            packages_get_package_attr2( pkg, "exec" ), //exec
            packages_get_package_attr2( pkg, "license" ), //license
            packages_get_package_attr2( pkg, "homepage" ), //homepage
            repo, //repo
            packages_get_package_attr2( pkg, "size" ), //size
            packages_get_package_attr2( pkg, "sha" ), //repo
            packages_get_package_attr2( pkg, "build_date" ), //build_date
            packages_get_package_attr2( pkg, "packager" ), //packager
            packages_get_package_attr2( pkg, "uri" ), //uri
            packages_get_package_attr2( pkg, "description" ), //description
            packages_get_package_attr2( pkg, "data_count" ), //data_count
            NULL);
    

    if( ret != SQLITE_DONE )
    {
        db_exec( &db, "rollback", NULL );  
        //printf( "rollback world, db_ret:%d\n", ret );
        return_code = -10; 
        goto exception_handler;
    }
    
    //update world_data
    ret = db_exec( &db, "delete from world_data where name=?", package_name, NULL );  
    if( ret != SQLITE_DONE )
    {
        ret = db_exec( &db, "rollback", NULL );  
        //printf( "rollback world_data, db_ret:%d\n", ret );
        return_code = -10; 
        goto exception_handler;
    }

    sql_data = "insert into world_data (name, version, data_name, data_format, data_size, data_install_size, data_depend, data_bdepend, data_recommended, data_conflict) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    for( i = 0; i < pkg_data->cnt; i++ )
    {
        ret = db_exec( &db, sql_data,  
                package_name, //name
                version, //version
                packages_get_package_data_attr2( pkg_data, i, "data_name"), //data_name
                packages_get_package_data_attr2( pkg_data, i, "data_format" ), //data_format
                packages_get_package_data_attr2( pkg_data, i, "data_size" ), //data_size
                packages_get_package_data_attr2( pkg_data, i, "data_install_size" ), //data_install_size
                packages_get_package_data_attr2( pkg_data, i, "data_depend" ), //data_depend
                packages_get_package_data_attr2( pkg_data, i, "data_bdepend" ), //data_bdepend
                packages_get_package_data_attr2( pkg_data, i, "data_recommended" ), //data_recommended
                packages_get_package_data_attr2( pkg_data, i, "data_conflict" ), //data_conflict
                NULL);

        if( ret != SQLITE_DONE )
        {
            db_exec( &db, "rollback", NULL );  
            //printf( "rollback world_data, db_ret:%d\n", ret );
            return_code = -10; 
            goto exception_handler;
        }
    }


    //update file list
    ret = db_exec( &db, "delete from world_file where name=?", package_name, NULL );  
    if( ret != SQLITE_DONE )
    {
        db_exec( &db, "rollback", NULL );  
        //printf( "rollback file list, db_ret:%d\n", ret );
        return_code = -10; 
        goto exception_handler;
    }

    sql_filelist = "insert into world_file (name, version, type, file, size, perms, uid, gid, mtime, extra) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"; 

    for( i = 0; i < pkg_file->cnt; i++ )
    {
            file_type = packages_get_package_file_attr( pkg_file, i, "type");
            file_file = packages_get_package_file_attr( pkg_file, i, "file");
            file_size = packages_get_package_file_attr( pkg_file, i, "size");
            file_perms = packages_get_package_file_attr( pkg_file, i, "perms");
            file_uid = packages_get_package_file_attr( pkg_file, i, "uid");
            file_gid = packages_get_package_file_attr( pkg_file, i, "gid");
            file_mtime = packages_get_package_file_attr( pkg_file, i, "mtime");
            file_extra = packages_get_package_file_attr( pkg_file, i, "extra");

            ret = db_exec( &db, sql_filelist, 
                    package_name,
                    version,
                    file_type ? file_type : "",
                    file_file ? file_file : "",
                    file_size ? file_size : "",
                    file_perms ? file_perms : "",
                    file_uid ? file_uid : "",
                    file_gid ? file_gid : "",
                    file_mtime ? file_mtime : "",
                    file_extra ? file_extra : "",
                    NULL );

            if( ret != SQLITE_DONE )
            {
                db_exec( &db, "rollback", NULL );  
                //printf( "rollback file list, db_ret:%d\n", ret );
                return_code = -10; 
                goto exception_handler;
            }
    }

    //replace mark
    if( pkg_file2 )
    {
        for( i = 0; i < pkg_file2->cnt; i++ )
        {
            replace_name = packages_get_package_file_attr( pkg_file2, i, "replace");
            replace_file = packages_get_package_file_attr( pkg_file2, i, "file");
            if( replace_name && replace_file && strlen( replace_name ) > 0 )
            {
                ret = db_exec( &db, "update world_file set replace=?, replace_with='' where name=? and file=?", replace_name, package_name, replace_file, NULL );  
                if( ret != SQLITE_DONE )
                {
                    db_exec( &db, "rollback", NULL );  
                    return_code = -10; 
                    goto exception_handler;
                }
            }
        }
    }

    if( replace_list )
    {
        for( i = 0; i < replace_list->cur_cnt; i++ )
        {
            replace_name = hash_table_list_get_data( replace_list->htl, i, "replace" );
            replace_file = hash_table_list_get_data( replace_list->htl, i, "file" );
            if( replace_name && replace_file && strlen( replace_name ) > 0 )
            {
                ret = db_exec( &db, "update world_file set replace=? where name=? and file=?", replace_name, package_name, replace_file, NULL );  
                if( ret != SQLITE_DONE )
                {
                    db_exec( &db, "rollback", NULL );  
                    return_code = -10; 
                    goto exception_handler;
                }

                ret = db_exec( &db, "update world_file set replace='', replace_with=? where name=? and file=?", package_name, replace_name, replace_file, NULL );  
                if( ret != SQLITE_DONE )
                {
                    db_exec( &db, "rollback", NULL );  
                    return_code = -10; 
                    goto exception_handler;
                }
            }
        }
    }

    //update universe
    can_update = "0";
    if( pkg2 )
    {
        version2 = packages_get_package_attr( pkg2, "version" );
        repo2 = packages_get_package_attr( pkg2, "repo" );
        if( version && (strlen( version ) > 0) && version2 && (strlen( version2 ) > 0) )
        {
            switch( packages_compare_version( version2, version ) )
            {
                case 1:
                    can_update = "1";
                    break;
                case 0:
                    can_update = "0";
                    break;
                case -1:
                    if( repo && repo2 && strcmp( repo, repo2 ) )
                    {
                        can_update = "-1";
                    }
                    else
                    {
                        can_update = "0";
                    }
                    break;
            }
        }
        else
        {
            can_update = "0";
        }

        ret = db_exec( &db, "update universe set installed='1', can_update=? where name=?", can_update, package_name, NULL );  
        if( ret != SQLITE_DONE )
        {
            db_exec( &db, "rollback", NULL );  
            //printf( "rollback universe, db_ret:%d\n", ret );
            return_code = -10; 
            goto exception_handler;
        }
    }

    if( pkg3 )
    {
        version2 = packages_get_package_attr( pkg3, "version" );
        repo2 = packages_get_package_attr( pkg3, "repo" );
        if( version && (strlen( version ) > 0) && version2 && (strlen( version2 ) > 0) )
        {
            switch( packages_compare_version( version2, version ) )
            {
                case 1:
                    can_update = "1";
                    break;
                case 0:
                    can_update = "0";
                    break;
                case -1:
                    if( repo && repo2 && strcmp( repo, repo2 ) )
                    {
                        can_update = "-1";
                    }
                    else
                    {
                        can_update = "0";
                    }
                    break;
            }
        }
        else
        {
            can_update = "0";
        }

        ret = db_exec( &db, "update universe_testing set installed='1', can_update=? where name=?", can_update, package_name, NULL );  
        if( ret != SQLITE_DONE )
        {
            db_exec( &db, "rollback", NULL );  
            //printf( "rollback universe_testing, db_ret:%d\n", ret );
            return_code = -10; 
            goto exception_handler;
        }
    }

    ret = db_exec( &db, "commit", NULL );  
    if( ret != SQLITE_DONE )
    {
        db_exec( &db, "rollback", NULL );  
        //printf( "rollback commit, db_ret:%d\n", ret );
        return_code = -10; 
        goto exception_handler;
    }
    db_close( &db );

    if( cb )
    {
        cb( cb_arg, ypk_path, 8, 1, NULL );
    }

    //rename files
    if( replace_list )
    {
        for( i = 0; i < replace_list->cur_cnt; i++ )
        {
            replace_name = hash_table_list_get_data( replace_list->htl, i, "replace" );
            replace_file = hash_table_list_get_data( replace_list->htl, i, "file" );
            if( replace_name && replace_file && strlen( replace_name ) > 0 )
            {
                tmp_file = util_strcat( replace_file, "~", replace_name, ".orig", NULL );
                if( tmp_file )
                {
                    rename( replace_file, tmp_file );
                    free( tmp_file );
                    tmp_file = NULL;
                }
            }
        }
    }

    for( i = 0; i < pkg_file->cnt; i++ )
    {
        file_file = packages_get_package_file_attr( pkg_file, i, "file");
        if( file_file )
        {
            tmp_file = util_strcat( file_file, "~ypk", NULL );
            if( tmp_file )
            {
                rename( tmp_file, file_file );
                free( tmp_file );
                tmp_file = NULL;
            }
        }
    }

    if( install && strlen( install ) )
    {
        file_file = util_strcat( "/var/ypkg/db/", package_name, "/", install, NULL );
        if( file_file )
        {
            tmp_file = util_strcat( "/var/ypkg/db/", package_name, "/", install, "~ypk", NULL );
            if( tmp_file )
            {
                rename( tmp_file, file_file );
                free( tmp_file );
                tmp_file = NULL;
            }
            free( file_file );
            file_file = NULL;
        }
    }

    //delete the files only in the old version
    if( installed && pkg_file && pkg_file2 )
    {
        for( i = 0; i < pkg_file2->cnt; i++ )
        {
            file_file = packages_get_package_file_attr( pkg_file2, i, "file");
            if( file_file )
            {
                delete_file = 1;
                for( j = 0; j < pkg_file->cnt; j++ )
                {
                    if( !strcmp( file_file, packages_get_package_file_attr( pkg_file, j, "file" ) ) )
                    {
                        delete_file = 0;
                        break;
                    }
                }

                if( delete_file )
                    remove( file_file );
            }

            replace_with = packages_get_package_file_attr( pkg_file2, i, "replace_with");
            if( replace_with && strlen( replace_with ) > 0 )
            {
                tmp_file = util_strcat( file_file, "~", package_name, ".orig", NULL );
                if( tmp_file )
                {
                    remove( tmp_file );
                    free( tmp_file );
                    tmp_file = NULL;
                }
            }
        }
    }


    //exec post_x script
    if( cb )
    {
        cb( cb_arg, ypk_path, 7, -1, "Executing post_install script\n" );
    }

    if( !access( tmp_ypk_install, R_OK ) )
    {
        if( !upgrade )
        {
            if( packages_exec_script( tmp_ypk_install, package_name, version, NULL, "post_install" ) == -1 )
            {
                return_code = -11; 
                goto exception_handler;
            }
        }
        else if( upgrade == 1 )
        {
            version2 = extra;
            if( packages_exec_script( tmp_ypk_install, package_name, version, version2, "post_upgrade" ) == -1 )
            {
                return_code = -11; 
                goto exception_handler;
            }
        }
        else //downgrade
        {
            version2 = extra;
            if( packages_exec_script( tmp_ypk_install, package_name, version, version2, "post_downgrade" ) == -1 )
            {
                return_code = -11; 
                goto exception_handler;
            }
        }
    }

    if( cb )
    {
        cb( cb_arg, ypk_path, 7, 1, NULL );
    }

    if( !strcmp( package_name, "ypkg2" ) )
    {
        packages_upgrade_db( pm );
    }

    if( cb )
    {
        cb( cb_arg, ypk_path, 9, 1, NULL );
    }

exception_handler:

    //rollback
    if( return_code <= -8 && return_code > -11 )
    {
        for( i = 0; i < pkg_file->cnt; i++ )
        {
            tmp_file = util_strcat( packages_get_package_file_attr( pkg_file, i, "file"), "~ypk", NULL);
            if( tmp_file )
            {
                remove( tmp_file );
                free( tmp_file );
                tmp_file = NULL;
            }
        }
    }

    if( install && strlen( install ) )
    {
        tmp_file = util_strcat( "/var/ypkg/db/", package_name, "/", install, "~ypk", NULL );
        if( tmp_file )
        {
            remove( tmp_file );
            free( tmp_file );
            tmp_file = NULL;
        }
    }

    if( return_code <= -7 && return_code > -11 && access( tmp_ypk_install, R_OK ) == 0 )
    {
        if( !upgrade )
        {
            packages_exec_script( tmp_ypk_install, package_name, version, NULL, "pre_install_fallback" );
        }
        else if( upgrade == 1 )
        {
            version2 = extra;
            packages_exec_script( tmp_ypk_install, package_name, version, version2, "pre_upgrade_fallback" );
        }
        else
        {
            version2 = extra;
            packages_exec_script( tmp_ypk_install, package_name, version, version2, "pre_downgrade_fallback" );
        }
    }

    //clean up
    if( filelist )
        free( filelist );

    if( pkg )
        packages_free_package( pkg );

    if( pkg2 )
        packages_free_package( pkg2 );

    if( pkg3 )
        packages_free_package( pkg3 );

    if( pkg_data )
        packages_free_package_data( pkg_data );

    if( pkg_file )
        packages_free_package_file( pkg_file );

    if( pkg_file2 )
        packages_free_package_file( pkg_file2 );

    if( replace_list )
        packages_free_replace_file_list( replace_list );

    remove( tmp_ypk_install );
    return return_code;
}

int packages_remove_package( YPackageManager *pm, char *package_name, ypk_progress_callback cb, void *cb_arg  )
{
    int             i, return_code, repo_testing;
    char            *install_file, *version, *install_file_path = NULL, *table, *sql, *file_path, *file_type, *replace, *replace_with, *tmp_file;
    YPackage        *pkg;
    YPackageFile    *pkg_file;
    DB              db;

    if( !package_name )
        return -1;

    return_code = 0;

    if( pm->accept_repo && !strcmp( pm->accept_repo, "testing" ) )
        repo_testing =1;
    else
        repo_testing =0;

    if( cb )
    {
        cb( cb_arg, package_name, 0, 3, "Remove" );
        cb( cb_arg, package_name, 1, -1, "Initializing" );
    }

    //get info from db
    pkg = packages_get_package( pm, package_name, 1 );
    if( !pkg )
    {
        return -2;
    }

    install_file = packages_get_package_attr( pkg, "install" );
    version = packages_get_package_attr( pkg, "version" );

    pkg_file = packages_get_package_file( pm, package_name );
    if( !pkg_file )
    {
        return_code = -2; 
        goto exception_handler;
    }

    if( cb )
    {
        cb( cb_arg, package_name, 1, 1, NULL );
    }

    if( cb )
    {
        cb( cb_arg, package_name, 5, -1, "Executing pre_remove script\n" );
    }

    //exec pre_remove script
    if( install_file && strlen( install_file ) > 8 )
    {
        install_file_path = util_strcat( PACKAGE_DB_DIR, "/", package_name, "/", install_file, NULL );
        if( !access( install_file_path, R_OK ) )
        {
            //printf( "running pre remove script ...\n" );
            if( packages_exec_script( install_file_path, package_name, version, NULL, "pre_remove" ) == -1 )
            {
                return_code = -3; 
                goto exception_handler;
            }
        }
    }

    if( cb )
    {
        cb( cb_arg, package_name, 5, 1, NULL );
    }

    if( cb )
    {
        cb( cb_arg, package_name, 6, -1, "Deleting files" );
    }

    //delete files
    for( i = 0; i < pkg_file->cnt; i++ )
    {
        file_path = packages_get_package_file_attr( pkg_file, i, "file");
        file_type = packages_get_package_file_attr( pkg_file, i, "type");
        replace = packages_get_package_file_attr( pkg_file, i, "replace");
        replace_with = packages_get_package_file_attr( pkg_file, i, "replace_with");
        if( file_path && file_type && ( file_type[0] == 'F' || file_type[0] == 'S' ) )
        {
            if( replace_with && strlen( replace_with ) > 0 )
            {
                tmp_file = util_strcat( file_path, "~", package_name, ".orig", NULL );
                if( tmp_file )
                {
                    remove( tmp_file );
                    free( tmp_file );
                    tmp_file = NULL;
                }
            }
            else
            {
                remove( file_path );
                if( replace && strlen( replace ) > 0 )
                {
                    tmp_file = util_strcat( file_path, "~", replace, ".orig", NULL );
                    if( tmp_file )
                    {
                        rename( tmp_file, file_path );
                        free( tmp_file );
                        tmp_file = NULL;
                    }
                }
            }

        }
    }

    //delete directories
    for( i = 0; i < pkg_file->cnt; i++ )
    {
        file_path = packages_get_package_file_attr( pkg_file, i, "file");
        file_type = packages_get_package_file_attr( pkg_file, i, "type");
        if( file_path && file_type &&  file_type[0] == 'D' )
        {
            remove( file_path );
        }
    }

    if( cb )
    {
        cb( cb_arg, package_name, 6, 1, NULL );
    }

    if( cb )
    {
        cb( cb_arg, package_name, 7, -1, "Executing post_remove script\n" );
    }

    //exec post_remove script
    if( install_file_path && !access( install_file_path, R_OK ) )
    {
        //printf( "running post remove script ...\n" );
        if( packages_exec_script( install_file_path, package_name, version, NULL, "post_remove" ) == -1 )
        {
            return_code = -5; 
            goto exception_handler;
        }
    }


    //delete /var/ypkg/db/$N
    file_path = util_strcat( PACKAGE_DB_DIR, "/", package_name, NULL );
    //printf( "deleting %s ... \n", file_path );
    util_remove_dir( file_path );
    free( file_path );

    if( cb )
    {
        cb( cb_arg, package_name, 7, 1, NULL );
    }

    if( cb )
    {
        cb( cb_arg, package_name, 8, -1, "Updating database" );
    }

    //update db
    db_init( &db, pm->db_name, OPEN_WRITE );
    db_exec( &db, "begin", NULL );  
    db_exec( &db, "delete from world where name=?", package_name, NULL );  
    db_exec( &db, "delete from world_data where name=?", package_name, NULL );  
    db_exec( &db, "delete from world_file where name=?", package_name, NULL );  
    for( i = 0; i < pkg_file->cnt; i++ )
    {
        file_path = packages_get_package_file_attr( pkg_file, i, "file");
        file_type = packages_get_package_file_attr( pkg_file, i, "type");
        replace = packages_get_package_file_attr( pkg_file, i, "replace");
        replace_with = packages_get_package_file_attr( pkg_file, i, "replace_with");
        if( file_path && file_type && ( file_type[0] == 'F' || file_type[0] == 'S' ) )
        {
            if( replace_with && strlen( replace_with ) > 0  )
            {
                if( replace && strlen( replace ) > 0  )
                {
                    db_exec( &db, "update world_file set replace=? where name=? and file=?", replace, replace_with, file_path, NULL );  

                    db_exec( &db, "update world_file set replace_with=? where name=? and file=?", replace_with, replace, file_path, NULL );  
                }
                else
                {
                    db_exec( &db, "update world_file set replace='' where name=? and file=?", replace_with, file_path, NULL );  
                }
            }
            else
            {
                if( replace && strlen( replace ) > 0  )
                    db_exec( &db, "update world_file set replace_with='' where name=? and file=?", replace, file_path, NULL );  
            }

        }
    }
    //db_exec( &db, "update universe set can_update='0', installed='0' where name=?", package_name, NULL );  
    table = repo_testing ? "universe_testing" : "universe";
    sql = util_strcat( "update ", table, " set installed='0', can_update='0' where name=?", NULL );
    db_exec( &db, sql, package_name, NULL );  
    free( sql );

    db_exec( &db, "end", NULL );  
    db_close( &db );

    if( cb )
    {
        cb( cb_arg, package_name, 8, 1, NULL );
    }

    if( cb )
    {
        cb( cb_arg, package_name, 9, 1, NULL );
    }


exception_handler:
    if( install_file_path )
        free( install_file_path );

    packages_free_package( pkg );

    packages_free_package_file( pkg_file );
    return return_code;
}


/*
 * clean up cache directory
 */
int packages_cleanup_package( YPackageManager *pm )
{
    return util_remove_files( pm->package_dest, ".ypk" );
}


/*
 * log
 */
int packages_log( YPackageManager *pm, char *package_name, char *msg )
{
    char    *time_str, *log;
    time_t  t; 
    FILE    *log_file;

    if( !msg )
        return -1;

    log = pm->log ? pm->log : LOG_FILE;
    log_file = fopen( log, "a" );

    if( log_file )
    {

        if( ( t = time( NULL ) ) == -1 )
        {
            fclose( log_file );
            return -1;
        }

        time_str = util_time_to_str( t );
        fprintf( log_file, "%s %s : %s\n", time_str, package_name ? package_name : "", msg );

        fflush( log_file );
        fclose( log_file );
        return 0;
    }

    return -1;
}
