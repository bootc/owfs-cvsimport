/*
$Id$
    OWFS -- One-Wire filesystem
    OWHTTPD -- One-Wire Web Server
    Written 2003 Paul H Alfille
    email: palfille@earthlink.net
    Released under the GPL
    See the header file: ow.h for full attribution
    1wire/iButton system from Dallas Semiconductor
*/

/* ow_server talks to the server, sending and recieving messages */
/* this is an alternative to direct bus communication */

#include "ownetapi.h"
#include "ow_server.h"

int OWNET_write( OWNET_HANDLE h, const char * onewire_path, const char * value_string, size_t value_length )
{
  return -1;
}
