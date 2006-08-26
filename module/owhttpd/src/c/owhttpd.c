/*
$Id$
    OW_HTML -- OWFS used for the web
    OW -- One-Wire filesystem

    Written 2003 Paul H Alfille

 * based on chttpd/1.0
 * main program. Somewhat adapted from dhttpd
 * Copyright (c) 0x7d0 <noop@nwonknu.org>
 * Some parts
 * Copyright (c) 0x7cd David A. Bartold
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "owhttpd.h" // httpd-specific

/*
 * Default port to listen too. If you aren't root, you'll need it to
 * be > 1024 unless you plan on using a config file
 */
#define DEFAULTPORT    80

static void Acceptor( int listenfd ) ;

#if OW_MT
pthread_t main_threadid ;
#define IS_MAINTHREAD (main_threadid == pthread_self())
#else
#define IS_MAINTHREAD 1
#endif

static void ow_exit( int e ) {
    if(IS_MAINTHREAD) {
        LibClose() ;
    }
    /* Process never die on WRT54G router with uClibc if exit() is used */
    _exit( e ) ;
}

static void exit_handler(int i) {
    return ow_exit( ((i<0) ? 1 : 0) ) ;
}

int main(int argc, char *argv[]) {
    int c ;

    /* grab our executable name */
    if (argc > 0) progname = strdup(argv[0]);

    /* Set up owlib */
    LibSetup() ;

    while ( (c=getopt_long(argc,argv,OWLIB_OPT,owopts_long,NULL)) != -1 ) {
        switch (c) {
        case 'V':
            fprintf(stderr,
            "%s version:\n\t" VERSION "\n",argv[0] ) ;
            break ;
        }
        if ( owopt(c,optarg,opt_httpd) ) ow_exit(0) ; /* rest of message */
    }

    /* non-option arguments */
    while ( optind < argc ) {
        OW_ArgGeneric(argv[optind]) ;
        ++optind ;
    }

    if ( outdevices==0 ) {
        LEVEL_DEFAULT("No TCP port specified (-p)\n%s -h for help\n",argv[0])
        ow_exit(1);
    }

    set_signal_handlers(exit_handler);

    /*
     * Now we drop privledges and become a daemon.
     */
    if ( LibStart() ) ow_exit(1) ;
#if OW_MT
    main_threadid = pthread_self() ;
#endif

    ServerProcess( Acceptor, opt_httpd, ow_exit ) ;
    ow_exit(0) ;
    return 0 ;
}

static void Acceptor( int listenfd ) {
    FILE * fp = fdopen(listenfd, "w+");
    if (fp) {
        handle_socket( fp ) ;
        fflush(fp);
        fclose(fp);
    }
}
