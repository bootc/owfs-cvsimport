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
#include "ow_counters.h"
#include "ow_connection.h"

// static int BUS_transaction_length( const struct transaction_log * tl, const struct parsedname * pn ) ;


/* Bus transaction */
/* Encapsulates communication with a device, including locking the bus, reset and selection */
/* Then a series of bytes is sent and returned, including sending data and reading the return data */
int BUS_transaction( const struct transaction_log * tl, const struct parsedname * pn ) {
    const struct transaction_log * t = tl ;
    int ret ;
    
    BUSLOCK(pn) ;
        do {
            //printf("Transact type=%d\n",t->type) ;
            switch (t->type) {
                case trxn_select:
                    ret = BUS_select(pn) ;
                    //printf("  Transaction select = %d\n",ret) ;
                    break ;
                case trxn_match:
                    ret = BUS_send_data( t->out, t->size, pn ) ;
                    //printf("  Transaction send = %d\n",ret) ;
                    break ;
                case trxn_read:
                    if ( t->out ) {
                        ret = BUS_sendback_data( t->out, t->in, t->size, pn ) ;
                        //printf("  Transaction sendback = %d\n",ret) ;
                    } else {
                        ret = BUS_readin_data( t->in, t->size, pn ) ;
                        //printf("  Transaction readin = %d\n",ret) ;
                    }
                    break ;
                case trxn_power:
                    ret = BUS_PowerByte( t->out[0], t->in, t->size, pn ) ;
                    //printf("  Transaction power = %d\n",ret) ;
                    break ;
                case trxn_program:
                    ret = BUS_ProgramPulse(pn) ;
                    break ;
                case trxn_reset:
                    ret = BUS_reset(pn) ;
                    // fall through
                case trxn_end:
                    t = NULL ;
                    break ;
                case trxn_verify:
                    {
                        struct parsedname pn2 ;
                        memcpy( &pn2, pn, sizeof(struct parsedname) ) ; //shallow copy
                        pn2.dev = NULL ;
                        ret = BUS_select(&pn2) || BUS_verify(t->size,pn) ;
                    }
                    break ;
            }
            if ( t==NULL ) break ;
            ++ t ;
        } while ( ret==0) ;
    BUSUNLOCK(pn) ;

    return ret ;
}
#if 0   /* not yet needed */
/* Bus transaction */
/* length of send/receive buffer
   return length (>=0) */
static int BUS_transaction_length( const struct transaction_log * tl, const struct parsedname * pn ) {
    const struct transaction_log * t = tl ;
    size_t size = 0 ;
    (void) pn ;
    
    while (1) {
        switch (t->type) {
            case trxn_read:
            case trxn_match:
                size += t->size ;
                    //printf("  Transaction send = %d\n",ret) ;
                break ;
            case trxn_power:
                ++size ;
                break ;
            case trxn_select:
            case trxn_program:
                break ;
            case trxn_reset:
            case trxn_end:
                return size ;
                break ;
        }
        ++ t ;
    }
}
#endif /* 0 */
