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
#include "ow_2505.h"

/* ------- Prototypes ----------- */

READ_FUNCTION(FS_r_page);
WRITE_FUNCTION(FS_w_page);
READ_FUNCTION(FS_r_status);
WRITE_FUNCTION(FS_w_status);
READ_FUNCTION(FS_r_memory);
WRITE_FUNCTION(FS_w_memory);

#define _1W_READ_MEMORY          0xF0
#define _1W_READ_STATUS          0xAA
#define _1W_EXTENDED_READ_MEMORY 0xA5
#define _1W_WRITE_MEMORY         0x0F
#define _1W_SPEED_WRITE_MEMORY   0xF3
#define _1W_WRITE_STATUS         0x55
#define _1W_SPEED_WRITE_STATUS   0xF5

/* ------- Structures ----------- */

struct aggregate A2505 = { 64, ag_numbers, ag_separate, };
struct aggregate A2505s = { 11, ag_numbers, ag_separate, };
struct filetype DS2505[] = {
	F_STANDARD,
	{"status", PROPERTY_LENGTH_SUBDIR, NON_AGGREGATE, ft_subdir, fc_volatile, NO_READ_FUNCTION, NO_WRITE_FUNCTION, VISIBLE, NO_FILETYPE_DATA,},
  {"status/page", 8, &A2505s, ft_binary, fc_stable, FS_r_status, FS_w_status, VISIBLE, NO_FILETYPE_DATA,},
  {"memory", 2048, NON_AGGREGATE, ft_binary, fc_stable, FS_r_memory, FS_w_memory, VISIBLE, NO_FILETYPE_DATA,},
  {"pages", PROPERTY_LENGTH_SUBDIR, NON_AGGREGATE, ft_subdir, fc_volatile, NO_READ_FUNCTION, NO_WRITE_FUNCTION, VISIBLE, NO_FILETYPE_DATA,},
  {"pages/page", 32, &A2505, ft_binary, fc_stable, FS_r_page, FS_w_page, VISIBLE, NO_FILETYPE_DATA,},
};

DeviceEntry(0B, DS2505);

struct filetype DS1985U[] = {
	F_STANDARD,
	{"status", PROPERTY_LENGTH_SUBDIR, NON_AGGREGATE, ft_subdir, fc_volatile, NO_READ_FUNCTION, NO_WRITE_FUNCTION, VISIBLE, NO_FILETYPE_DATA,},
  {"status/page", 8, &A2505s, ft_binary, fc_stable, FS_r_status, FS_w_status, VISIBLE, NO_FILETYPE_DATA,},
  {"memory", 2048, NON_AGGREGATE, ft_binary, fc_stable, FS_r_memory, FS_w_memory, VISIBLE, NO_FILETYPE_DATA,},
  {"pages", PROPERTY_LENGTH_SUBDIR, NON_AGGREGATE, ft_subdir, fc_volatile, NO_READ_FUNCTION, NO_WRITE_FUNCTION, VISIBLE, NO_FILETYPE_DATA,},
  {"pages/page", 32, &A2505, ft_binary, fc_stable, FS_r_page, FS_w_page, VISIBLE, NO_FILETYPE_DATA,},
};

DeviceEntry(8B, DS1985U);

struct aggregate A2506 = { 256, ag_numbers, ag_separate, };
struct aggregate A2506s = { 11, ag_numbers, ag_separate, };
struct filetype DS2506[] = {
	F_STANDARD,
	{"status", PROPERTY_LENGTH_SUBDIR, NON_AGGREGATE, ft_subdir, fc_volatile, NO_READ_FUNCTION, NO_WRITE_FUNCTION, VISIBLE, NO_FILETYPE_DATA,},
  {"status/page", 32, &A2506s, ft_binary, fc_stable, FS_r_status, FS_w_status, VISIBLE, NO_FILETYPE_DATA,},
  {"memory", 8192, NON_AGGREGATE, ft_binary, fc_stable, FS_r_memory, FS_w_memory, VISIBLE, NO_FILETYPE_DATA,},
  {"pages", PROPERTY_LENGTH_SUBDIR, NON_AGGREGATE, ft_subdir, fc_volatile, NO_READ_FUNCTION, NO_WRITE_FUNCTION, VISIBLE, NO_FILETYPE_DATA,},
  {"pages/page", 32, &A2506, ft_binary, fc_stable, FS_r_page, FS_w_page, VISIBLE, NO_FILETYPE_DATA,},
};

DeviceEntryExtended(0F, DS2506, DEV_ovdr);

struct filetype DS1986U[] = {
	F_STANDARD,
	{"status", PROPERTY_LENGTH_SUBDIR, NON_AGGREGATE, ft_subdir, fc_volatile, NO_READ_FUNCTION, NO_WRITE_FUNCTION, VISIBLE, NO_FILETYPE_DATA,},
  {"status/page", 32, &A2506s, ft_binary, fc_stable, FS_r_status, FS_w_status, VISIBLE, NO_FILETYPE_DATA,},
  {"memory", 8192, NON_AGGREGATE, ft_binary, fc_stable, FS_r_memory, FS_w_memory, VISIBLE, NO_FILETYPE_DATA,},
  {"pages", PROPERTY_LENGTH_SUBDIR, NON_AGGREGATE, ft_subdir, fc_volatile, NO_READ_FUNCTION, NO_WRITE_FUNCTION, VISIBLE, NO_FILETYPE_DATA,},
  {"pages/page", 32, &A2506, ft_binary, fc_stable, FS_r_page, FS_w_page, VISIBLE, NO_FILETYPE_DATA,},
};

DeviceEntryExtended(8F, DS1986U, DEV_ovdr);

/* ------- Functions ------------ */

/* DS2505 */
static GOOD_OR_BAD OW_w_status(BYTE * data, size_t size, off_t offset, struct parsedname *pn);

/* 2505 memory */
static ZERO_OR_ERROR FS_r_memory(struct one_wire_query *owq)
{
	size_t pagesize = 32;
	return RETURN_Z_OR_E(COMMON_OWQ_readwrite_paged(owq, 0, pagesize, COMMON_read_memory_F0)) ;
}

static ZERO_OR_ERROR FS_r_page(struct one_wire_query *owq)
{
	size_t pagesize = 32;
	return RETURN_Z_OR_E(COMMON_OWQ_readwrite_paged(owq, OWQ_pn(owq).extension, pagesize, COMMON_read_memory_F0)) ;
}

static ZERO_OR_ERROR FS_r_status(struct one_wire_query *owq)
{
	size_t pagesize = FileLength(PN(owq)) ;
	return RETURN_Z_OR_E(COMMON_OWQ_readwrite_paged(owq, OWQ_pn(owq).extension, pagesize, COMMON_read_memory_crc16_AA)) ;
}

static ZERO_OR_ERROR FS_w_memory(struct one_wire_query *owq)
{
	if (COMMON_write_eprom_mem_owq(owq)) {
		return -EINVAL;
	}
	return 0;
}

static ZERO_OR_ERROR FS_w_status(struct one_wire_query *owq)
{
	return RETURN_Z_OR_E(OW_w_status(OWQ_explode(owq))) ;
}

static ZERO_OR_ERROR FS_w_page(struct one_wire_query *owq)
{
	size_t pagesize = 32;
	if (COMMON_offset_process( COMMON_write_eprom_mem_owq, owq, OWQ_pn(owq).extension * pagesize)) {
		return -EINVAL;
	}
	return 0;
}

static GOOD_OR_BAD OW_w_status(BYTE * data, size_t size, off_t offset, struct parsedname *pn)
{
	BYTE p[6] = { _1W_WRITE_STATUS, LOW_HIGH_ADDRESS(offset), data[0] };
	GOOD_OR_BAD ret = gbGOOD;
	struct transaction_log tfirst[] = {
		TRXN_START,
		TRXN_WR_CRC16(p, 4, 0),
		TRXN_PROGRAM,
		TRXN_READ1(p),
		TRXN_END,
	};

	if (size == 0) {
		return gbGOOD;
	}
	if (size == 1) {
		return BUS_transaction(tfirst, pn) || (p[0] & (~data[0]));
	}
	BUSLOCK(pn);
	if (BUS_transaction(tfirst, pn) || (p[0] & ~data[0])) {
		ret = gbBAD;
	} else {
		size_t i;
		const BYTE *d = &data[1];
		UINT s = offset + 1;
		struct transaction_log trest[] = {
			//TRXN_WR_CRC16_SEEDED( p, &s, 1, 0 ) ,
			TRXN_WR_CRC16_SEEDED(p, p, 1, 0),
			TRXN_PROGRAM,
			TRXN_READ1(p),
			TRXN_END,
		};
		for (i = 0; i < size; ++i, ++d, ++s) {
			if (BUS_transaction(trest, pn) || (p[0] & ~d[0])) {
				ret = gbBAD;
				break;
			}
		}
	}
	BUSUNLOCK(pn);
	return ret;
}
