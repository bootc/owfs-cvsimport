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

#include "owfs_config.h"
#include "ow.h" // for libow
#include "owhttpd.h" // httpd-specific

/*
 * Default port to listen too. If you aren't root, you'll need it to
 * be > 1024 unless you plan on using a config file
 */
#define DEFAULTPORT    80

static void ow_exit( int e ) ;
static void Acceptor( int listenfd ) ;

void handle_sighup(int unused) {
    (void) unused ;
    exit(0);
}

void handle_sigterm(int unused) {
    (void) unused ;
    exit(0);
}

void handle_sigchild(int unused) {
    (void) unused ;
    signal(SIGCHLD, handle_sigchild);
}

int main(int argc, char *argv[]) {
    char c ;

    LibSetup() ;
    if ( argc>0 ) progname = strdup(argv[0]) ;

    while ( (c=getopt_long(argc,argv,OWLIB_OPT,owopts_long,NULL)) != -1 ) {
        switch (c) {
        case 'h':
            fprintf(stderr,
            "Usage: %s ttyDevice -p tcpPort [options] \n"
            "   or: %s [options] -d ttyDevice -p tcpPort \n"
            "    -p port   -- tcp port for web serving process (e.g. 3001)\n" ,
            argv[0],argv[0] ) ;
            break ;
        case 'V':
            fprintf(stderr,
            "%s version:\n\t" VERSION "\n",argv[0] ) ;
            break ;
        }
        if ( owopt(c,optarg) ) ow_exit(0) ; /* rest of message */
    }

    /* non-option arguments */
    while ( optind < argc ) {
        OW_ArgGeneric(argv[optind]) ;
        ++optind ;
    }

    if ( outdevices==0 ) {
        fprintf(stderr, "No TCP port specified (-p)\n%s -h for help\n",argv[0]);
        ow_exit(1);
    }

    /*
     * Now we drop privledges and become a daemon.
     */
    if ( LibStart() ) ow_exit(1) ;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, handle_sigchild);
    signal(SIGHUP, handle_sighup);
    signal(SIGTERM, handle_sigterm);

    ServerProcess( Acceptor, ow_exit ) ;
}

static void Acceptor( int listenfd ) {
    FILE * fp = fdopen(listenfd, "w+");
    if (fp) {
        handle_socket( fp ) ;
        fflush(fp);
    }
}

static void ow_exit( int e ) {
    LibClose() ;
    closelog() ;
    exit( e ) ;
}
