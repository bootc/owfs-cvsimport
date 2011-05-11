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

#include "owhttpd.h"			// httpd-specific

/*
 * Default port to listen too. If you aren't root, you'll need it to
 * be > 1024 unless you plan on using a config file
 */
#define DEFAULTPORT    80

static void Acceptor(int listenfd);

int main(int argc, char *argv[])
{
	int c;

	/* Set up owlib */
	LibSetup(opt_httpd);

	/* grab our executable name */
	if (argc > 0) {
		Globals.progname = owstrdup(argv[0]);
	}

	while ((c = getopt_long(argc, argv, OWLIB_OPT, owopts_long, NULL)) != -1) {
		switch (c) {
		case 'V':
			fprintf(stderr, "%s version:\n\t" VERSION "\n", argv[0]);
			break;
		default:
			break;
		}
		if ( BAD( owopt(c, optarg) ) ) {
			ow_exit(0);			/* rest of message */
		}
	}

	/* non-option arguments */
	while (optind < argc) {
		ARG_Generic(argv[optind]);
		++optind;
	}

	if (Outbound_Control.active == 0) {
		if (Globals.zero == zero_none) {
			LEVEL_DEFAULT("%s would be \"locked in\" so will quit.\nBonjour and Avahi not available.", argv[0]);
			ow_exit(1);
		} else {
			LEVEL_CONNECT("%s will use an ephemeral port", argv[0]) ;
		}
		ARG_Server(NULL);		// make an ephemeral assignment
	}

	/* become a daemon if not told otherwise */
	if ( BAD(EnterBackground()) ) {
		ow_exit(1);
	}

	/* Set up adapters */
	if ( BAD(LibStart()) ) {
		ow_exit(1);
	}

	set_exit_signal_handlers(exit_handler);
	set_signal_handlers(NULL);

	ServerProcess(Acceptor);

	LEVEL_DEBUG("ServerProcess done");
	ow_exit(0);
	LEVEL_DEBUG("owhttpd done");
	return 0;
}

static void Acceptor(int listenfd)
{
	FILE *fp = fdopen(listenfd, "w+");
	if (fp) {
		handle_socket(fp);
		fflush(fp);
		fclose(fp);
	}
}
