/*
 * $Id$
 */
 /* Modified 8/2004 by Paul Alfille for OWFS the 1-Wire Filesystem */

#include "owfs_config.h"
#include "ow.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pwd.h>
#include <syslog.h>
#include <pthread.h>
#include <stdlib.h>
#include "oftpd.h"
#include "ftp_listener.h"
#include "ftp_error.h"


void *connection_acceptor(struct ftp_listener_t *f);

struct ftp_listener_t ftp_listener;
#if OW_MT
pthread_t main_threadid ;
#define IS_MAINTHREAD (main_threadid == pthread_self())
#else
#define IS_MAINTHREAD 1
#endif

static void ow_exit( int e ) {
    if(IS_MAINTHREAD) {
      if(ftp_listener.fd) {
	syslog(LOG_INFO, "Waiting for all connections to finish");
	ftp_listener_stop(&ftp_listener);
      }
      syslog(LOG_INFO, "FTP server exiting, all connections finished");
      LibClose() ;
    }
    exit( e ) ;
}

static void exit_handler(int i) {
  return ow_exit( ((i<0) ? 1 : 0) ) ;
}


int main(int argc, char *argv[]) {
    char c ;
    int max_clients;
    int log_facility;
    char rootdir[128];
    /* hard code the user as "nobody" */
    const char* username = "nobody";
    char *address;
    struct passwd *user_info = NULL;
    error_code_t err;
    sigset_t term_signal;
    int sig;
    int portnum ;
#ifdef OW_MT
    pthread_t thread ;
#ifndef __UCLIBC__
    pthread_attr_t attr ;
    pthread_attr_init(&attr) ;
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) ;
#endif /* __UCLIBC__ */
#endif /* OW_MT */

    memset(&ftp_listener, 0, sizeof(struct ftp_listener_t));

    /* grab our executable name */
    if (argc > 0) progname = strdup(argv[0]);

    /* Set up owlib */
    LibSetup() ;

    /* verify we're running as root */
    if (geteuid() != 0) {
        fprintf(stderr, "%s: program needs root permission to run\n", progname);
        ow_exit(1);
    }

    /* default command-line arguments */
    max_clients = MAX_CLIENTS;
    log_facility = LOG_FTP;

    while ( (c=getopt_long(argc,argv,OWLIB_OPT,owopts_long,NULL)) != -1 ) {
        switch (c) {
        case 'h':
            fprintf(stderr,
            "Usage: %s ttyDevice [options] \n"
            "   or: %s [options] -d ttyDevice \n"
            "    -p port   -- Listen port for ftp server (default %s)\n" ,
		    progname, progname, DEFAULT_PORTNAME) ;
            break ;
        case 'V':
            fprintf(stderr,
            "%s version:\n\t" VERSION "\n",progname ) ;
            break ;
        default:
            break;
        }
        if ( owopt(c,optarg) ) ow_exit(0) ; /* rest of message */
    }

    /* non-option arguments */
    while ( optind < argc-1 ) {
      OW_ArgGeneric(argv[optind]) ;
      ++optind ;
    }

    /* no port was defined, so listen on port 21 as default */
    if(!outdevices) {
      if(OW_ArgServer( DEFAULT_PORTNAME )) {
	fprintf(stderr, "Error using default address\n");
	ow_exit(1);
      }
    }
    if(ServerAddr(outdevice)<0) {
      fprintf(stderr, "Failed to initialize port. Use '-p' argument.\n");
      ow_exit(1);
    }

    portnum = atoi(outdevice->service) ;
    address = outdevice->host ;

    set_signal_handlers(exit_handler);

    /*
     * Now we drop privledges and become a daemon.
     */
    if ( LibStart() ) ow_exit(1) ;

#if OW_MT
    main_threadid = pthread_self() ;
#endif

    /* log the start time */
    openlog(NULL, LOG_NDELAY, log_facility);
    syslog(LOG_INFO,"Starting, version %s, as PID %d", VERSION, getpid());

    /* set user to be as inoffensive as possible */
    if ( user_info ) {
        if (setgid(user_info->pw_gid) != 0) {
            syslog(LOG_ERR, "error changing group; %s", strerror(errno));
            ow_exit(1);
        }
        if (setuid(user_info->pw_uid) != 0) {
            syslog(LOG_ERR, "error changing group; %s", strerror(errno));
            ow_exit(1);
        }
    }

    /* create our main listener */
    if (!ftp_listener_init(&ftp_listener,
                           address,
                           portnum,
                           max_clients,
                           INACTIVITY_TIMEOUT,
                           "/",
                           &err))
    {
        fprintf(stderr, "error initializing FTP listener on port %s:%d; %s\n",
		address,portnum, error_get_desc(&err));
        syslog(LOG_ERR, "error initializing FTP listener on port %s:%d; %s",
	       address, portnum, error_get_desc(&err));
        exit(1);
    }

    connection_acceptor( &ftp_listener ) ;

    ow_exit(0);
}

