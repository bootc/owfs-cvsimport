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

#include <config.h>
#include "owfs_config.h"
#include "ow.h"
#include "ow_connection.h"

/* Routines for handling a linked list of connections in and out */
/* typical connection in would be gthe serial port or USB */

/* Globals */
struct connection_in * indevice = NULL ;
int indevices = 0 ;

/* Make a new indevice, and place it in the chain */
/* Based on a shallow copy of "in" if not NULL */
struct connection_in * NewIn( void ) {
    size_t len = sizeof(struct connection_in) ;
    struct connection_in * now = (struct connection_in *)malloc( len ) ;
    if (now) {
        memset( now, 0, len ) ;
        now->next = indevice ; /* put in linked list at start */
        indevice = now ;
        now->index = 0 ;
    } else {
        printf(stderr,"Cannot allocate memory for adapter structure,\n") ;
    }
    return now ;
}
