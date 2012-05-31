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

static void Browse_close(struct connection_in *in);
static GOOD_OR_BAD browse_in_use(const struct connection_in * in_selected) ;

/* Device-specific functions */
GOOD_OR_BAD Browse_detect(struct port_in *pin)
{
	struct connection_in * in = pin->first ;
#if OW_ZERO
	in->iroutines.detect = Browse_detect;
	in->Adapter = adapter_browse_monitor;	/* OWFS assigned value */
	in->iroutines.reset = NO_RESET_ROUTINE;
	in->iroutines.next_both = NO_NEXT_BOTH_ROUTINE;
	in->iroutines.PowerByte = NO_POWERBYTE_ROUTINE;
	in->iroutines.ProgramPulse = NO_PROGRAMPULSE_ROUTINE;
	in->iroutines.sendback_data = NO_SENDBACKDATA_ROUTINE;
	in->iroutines.sendback_bits = NO_SENDBACKBITS_ROUTINE;
	in->iroutines.select = NO_SELECT_ROUTINE;
	in->iroutines.select_and_sendback = NO_SELECTANDSENDBACK_ROUTINE;
	in->iroutines.reconnect = NO_RECONNECT_ROUTINE;
	in->iroutines.close = Browse_close;
	in->iroutines.flags = ADAP_FLAG_sham;
	in->adapter_name = "ZeroConf monitor";
	in->busmode = bus_browse ;
	
	RETURN_BAD_IF_BAD( browse_in_use(in) ) ;

	in->master.browse.bonjour_browse = 0 ;

#if !OW_CYGWIN	
	in->master.browse.avahi_browser = NULL ;
	in->master.browse.avahi_poll    = NULL ;
	in->master.browse.avahi_client  = NULL ;
#endif /* !OW_CYGWIN */

	if (Globals.zero == zero_none ) {
		LEVEL_DEFAULT("Zeroconf/Bonjour is disabled since Bonjour or Avahi library wasn't found.");
		return gbBAD;
	} else {
		OW_Browse(in);
	}
	return gbGOOD ;
#else /* OW_ZERO */
	(void) in ;
	return gbBAD ;
#endif /* OW_ZERO */
}

static GOOD_OR_BAD browse_in_use(const struct connection_in * in_selected)
{
	struct port_in * pin ;
	
	for ( pin = Inbound_Control.head_port ; pin ; pin = pin->next ) {
		struct connection_in *cin;

		for (cin = pin->first; cin != NO_CONNECTION; cin = cin->next) {
			if ( cin == in_selected ) {
				continue ;
			}
			if ( cin->busmode != bus_browse ) {
				continue ;
			}
			return gbBAD ;
		}
	}
	return gbGOOD;					// not found in the current inbound list
}

static void Browse_close(struct connection_in *in)
{
#if OW_ZERO
	if (in->master.browse.bonjour_browse && (libdnssd != NULL)) {
		DNSServiceRefDeallocate(in->master.browse.bonjour_browse);
		in->master.browse.bonjour_browse = 0 ;
	}
#if !OW_CYGWIN	
	if ( in->master.browse.avahi_poll != NULL ) {
		// Signal avahi loop to quit (in ow_avahi_browse.c)
		// and clean up for itself
		avahi_simple_poll_quit(in->master.browse.avahi_poll);
	}
#endif /* !OW_CYGWIN */
#else /* OW_ZERO */
	(void) in ;
#endif /* OW_ZERO */
}
