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
#include "ow_w1.h"

#ifdef HAVE_LINUX_LIMITS_H
#include <linux/limits.h>
#endif

/* ---------------------------------------------- */
/* raw COM port interface routines                */
/* ---------------------------------------------- */

//open a port (serial or tcp)
GOOD_OR_BAD COM_open(struct connection_in *connection)
{
	struct connection_in * head_in ;
	if (connection == NO_CONNECTION) {
		LEVEL_DEBUG("Attempt to open a NULL serial device");
		return gbBAD;
	}

	head_in = connection->channel_info.head ; // head of multigroup bus

	switch ( SOC(head_in)->state ) {
		case cs_deflowered:
			// Attempt to reopen a good connection?
			COM_close(head_in) ;
			break ;
		case cs_virgin:
			break ;
	}

	switch ( SOC(head_in)->type ) {
		case ct_telnet:
			if ( SOC(head_in)->dev.telnet.telnet_negotiated == completed_negotiation ) {
				 SOC(head_in)->dev.telnet.telnet_negotiated = needs_negotiation ;
			}
			SOC(head_in)->dev.telnet.telnet_supported = 0 ;
			return tcp_open( head_in ) ;		
		case ct_tcp:
			return tcp_open( head_in ) ;
		case ct_netlink:
#if OW_W1
			return w1_bind( connection ) ;
#endif /* OW_W1 */
		case ct_i2c:
		case ct_usb:
			LEVEL_DEBUG("Unimplemented");
			return gbBAD ;
		case ct_serial:
			return serial_open( head_in ) ;
		case ct_unknown:
		case ct_none:
		default:
			LEVEL_DEBUG("Unknown type.");
			return gbBAD ;
	}
}
