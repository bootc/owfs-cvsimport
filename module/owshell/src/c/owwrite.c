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

static int HexVal( char c ) ;
static char * HexConvert( char * input_string ) ;

/* ---------------------------------------------- */
/* Command line parsing and result generation     */
/* ---------------------------------------------- */
int main(int argc, char *argv[])
{
	int c;
	int rc = -EINVAL;

	Setup();
	/* process command line arguments */
	while (1) {
		c = getopt_long(argc, argv, OWLIB_OPT, owopts_long, NULL);
		if (c == -1) {
			break;
		}
		owopt(c, optarg);
	}

	DefaultOwserver();
	Server_detect();

	if ( hexflag ) {
		char * hex_convert ;
		/* non-option arguments */
		while (optind < argc - 1) {
			hex_convert = HexConvert(argv[optind + 1]) ;
			rc = ServerWrite(argv[optind], hex_convert);
			optind += 2;
			if ( hex_convert ) {
				free(hex_convert ) ;
			}
		}
	} else {
		/* non-option arguments */
		while (optind < argc - 1) {
			rc = ServerWrite(argv[optind], argv[optind + 1]);
			optind += 2;
		}
	}

	if (optind < argc) {
		fprintf(stderr, "Unpaired <path> <value> entry: %s\n", argv[optind]);
		rc = -EINVAL;
	}
	if ( rc >= 0 ) {
		errno = 0 ;
		Exit(0) ;
	} else {
		errno = -rc ;
		Exit(1) ;
	}
	return 0;					// never reached
}

static int HexVal( char c )
{
	switch ( c ) {
		case '0':           return 0 ;
		case '1':           return 1 ;
		case '2':           return 2 ;
		case '3':           return 3 ;
		case '4':           return 4 ;
		case '5':           return 5 ;
		case '6':           return 6 ;
		case '7':           return 7 ;
		case '8':           return 8 ;
		case '9':           return 9 ;
		case 'A': case 'a': return 10 ;
		case 'B': case 'b': return 11 ;
		case 'C': case 'c': return 12 ;
		case 'D': case 'd': return 13 ;
		case 'E': case 'e': return 14 ;
		case 'F': case 'f': return 15 ;
		default:
			fprintf(stderr,"Unrecognized hex character %c\n",c) ;
			Exit(1) ;
	}
}

static char * HexConvert( char * input_string )
{
	int length = strlen( input_string ) ;
	int pad_first =  ( (length/2)*2 != length ) ;
	char * return_string = malloc( (length + pad_first)/2 ) ;
	int hex_pointer = 0 ;
	int char_pointer = 0 ;

	if ( return_string == NULL ) {
		fprintf(stderr,"Out of memory.\n") ;
		errno = ENOMEM ;
		Exit(1) ;
	}

	if ( pad_first ) {
		return_string[0] = HexVal(input_string[0]) ;
		++char_pointer ;
		++hex_pointer ;
	}

	while ( hex_pointer < length ) {
		return_string[char_pointer] = HexVal(input_string[hex_pointer]) * 16 + HexVal(input_string[hex_pointer+1]) ;
		hex_pointer += 2 ;
		++char_pointer ;
	}

	return return_string ; //freed in calling function
}

		


	