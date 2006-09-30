/*
$Id$
     OW -- One-Wire filesystem
    version 0.4 7/2/2003

    Function naming scheme:
    OW -- Generic call to interaface
    LI -- LINK commands
    L1 -- 2480B commands
    FS -- filesystem commands
    UT -- utility functions
    COM - serial port functions
    DS2480 -- DS9097 serial connector

    Written 2003 Paul H Alfille
*/

#include "owshell.h"

/* ---------------------------------------------- */
/* Command line parsing and result generation     */
/* ---------------------------------------------- */
int main(int argc, char *argv[]) {
    int c ;
    int paths_found = 0 ;

    Setup() ;
    /* process command line arguments */
    while ( (c=getopt_long(argc,argv,OWLIB_OPT,owopts_long,NULL)) != -1 ) 
        owopt(c,optarg) ;

    /* non-option arguments */
    while ( optind < argc-1 ) {
        if ( indevice==NULL ) {
            OW_ArgNet(argv[optind]) ;
            ++optind ;
        } else {
            if ( paths_found++ == 0 ) Server_detect() ;
            ServerWrite(argv[optind],argv[optind+1]) ;
            optind += 2 ;
        }
    }
    if ( optind < argc ) {
        fprintf(stderr,"Unpaired <path> <value> entry: %s\n",argv[optind]) ;
        exit(1) ;
    }
    exit(0) ;
}