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

/* General Device File format:
    This device file corresponds to a specific 1wire/iButton chip type
	( or a closely related family of chips )

	The connection to the larger program is through the "device" data structure,
	  which must be declared in the acompanying header file.

	The device structure holds the
	  family code,
	  name,
	  device type (chip, interface or pseudo)
	  number of properties,
	  list of property structures, called "filetype".

	Each filetype structure holds the
	  name,
	  estimated length (in bytes),
	  aggregate structure pointer,
	  data format,
	  read function,
	  write funtion,
	  generic data pointer

	The aggregate structure, is present for properties that several members
	(e.g. pages of memory or entries in a temperature log. It holds:
	  number of elements
	  whether the members are lettered or numbered
	  whether the elements are stored together and split, or separately and joined
*/

#include <config.h>
#include "owfs_config.h"
#include "ow_connection.h"

#define _1W_READ_F0  0xF0
#define _1W_READ_A5  0xA5
#define _1W_READ_AA  0xAA

static void Set_OWQ_length( struct one_wire_query * owq ) ;
static int OW_r_crc16(BYTE code, struct one_wire_query * owq, size_t page, size_t pagesize ) ;

static void Set_OWQ_length( struct one_wire_query * owq ) {
	switch( OWQ_pn(owq).ft->format ) {
		case ft_binary:
		case ft_ascii:
		case ft_vascii:
			OWQ_length(owq) = OWQ_size(owq) ;
			break ;
		default:
			break ;
	}
}

/* No CRC -- 0xF0 code */
int OW_r_mem_simple(struct one_wire_query * owq, size_t page, size_t pagesize )
{
	off_t offset = OWQ_offset(owq) + page * pagesize ;
    BYTE p[3] = { _1W_READ_F0, LOW_HIGH_ADDRESS(offset), };
	struct transaction_log t[] = {
		TRXN_START,
        TRXN_WRITE3(p),
        TRXN_READ((BYTE *) OWQ_buffer(owq), OWQ_size(owq)),
		TRXN_END,
	};

	Set_OWQ_length(owq) ;
	return BUS_transaction(t, PN(owq));
}

/* read up to end of page to CRC16 -- 0xA5 code */
static int OW_r_crc16(BYTE code, struct one_wire_query * owq, size_t page, size_t pagesize )
{
    off_t offset = OWQ_offset(owq) + page * pagesize ;
    size_t size = OWQ_size(owq) ;
    BYTE p[3 + pagesize + 2] ;
    int rest = pagesize - (offset % pagesize);
    struct transaction_log t[] = {
        TRXN_START,
        TRXN_WR_CRC16(p,3,rest),
        TRXN_END,
    };

    p[0] = code;
    p[1] = BYTE_MASK(offset) ;
    p[2] = BYTE_MASK(offset >> 8) ;
    if (BUS_transaction(t, PN(owq)))
        return 1;
    memcpy(OWQ_buffer(owq), &p[3], size);
    Set_OWQ_length( owq) ;
    return 0;
}

/* read up to end of page to CRC16 -- 0xA5 code */
int OW_r_mem_crc16_A5(struct one_wire_query * owq, size_t page, size_t pagesize )
{
    return OW_r_crc16( _1W_READ_A5, owq, page, pagesize ) ;
}

/* read up to end of page to CRC16 -- 0xA5 code */
int OW_r_mem_crc16_AA(struct one_wire_query * owq, size_t page, size_t pagesize )
{
    return OW_r_crc16( _1W_READ_AA, owq, page, pagesize ) ;
}

/* read up to end of page to CRC16 -- 0xF0 code */
int OW_r_mem_crc16_F0(struct one_wire_query * owq, size_t page, size_t pagesize )
{
    return OW_r_crc16( _1W_READ_F0, owq, page, pagesize ) ;
}

/* read up to end of page to CRC16 -- 0xA5 code */
/* Extra 8 bytes, (for counter) too -- discarded */
int OW_r_mem_toss8(struct one_wire_query * owq, size_t page, size_t pagesize)
{
    off_t offset = OWQ_offset(owq) + page * pagesize ;
    BYTE p[3 + pagesize + 8 + 2] ;
    int rest = pagesize - (offset % pagesize);
    struct transaction_log t[] = {
        TRXN_START,
        TRXN_WR_CRC16(p,3,rest+8),
        TRXN_END,
    };

    p[0] = _1W_READ_A5;
    p[1] = BYTE_MASK(offset) ;
    p[2] = BYTE_MASK(offset >> 8) ;
    if (BUS_transaction(t, PN(owq) ))
        return 1;
    memcpy(OWQ_buffer(owq), &p[3], OWQ_size(owq) );
    Set_OWQ_length( owq ) ;
    return 0;
}

/*  0xA5 code */
/* Extra 8 bytes, too */
int OW_r_mem_counter_bytes(BYTE * extra, size_t page, size_t pagesize, struct parsedname * pn)
{
    off_t offset = (page+1) * pagesize - 1 ; // last byte of page
    BYTE p[3 + 1 + 8 + 2] = { _1W_READ_A5, LOW_HIGH_ADDRESS(offset), };
    struct transaction_log t[] = {
        TRXN_START,
        TRXN_WR_CRC16(p,3,1+8),
        TRXN_END,
    };

    if (BUS_transaction(t, pn ))
        return 1;
    if ( extra ) memcpy(extra, &p[3+1], 8);
    return 0;
}

