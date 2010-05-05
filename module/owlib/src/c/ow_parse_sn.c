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

enum parse_serialnumber Parse_SerialNumber(char *sn_char, BYTE * sn)
{
	ASCII ID[14];
	int i;

	for (i = 0; i < 14; ++i, ++sn_char) {	/* get ID number */
		if (*sn_char == '.') { // ignore dots
			++sn_char;
		}
		if (isxdigit(*sn_char)) {
			ID[i] = *sn_char;
		} else {
			return sn_alias; // non-hex
		}
	}
	sn[0] = string2num(&ID[0]);
	sn[1] = string2num(&ID[2]);
	sn[2] = string2num(&ID[4]);
	sn[3] = string2num(&ID[6]);
	sn[4] = string2num(&ID[8]);
	sn[5] = string2num(&ID[10]);
	sn[6] = string2num(&ID[12]);
	sn[7] = CRC8compute(sn, SERIAL_NUMBER_SIZE-1, 0);
	if (*sn_char == '.') {
		++sn_char;
	}
	// Check CRC8 if given
	if (isxdigit(sn_char[0]) && isxdigit(sn_char[1])) {
		char crc[2];
		num2string(crc, sn[SERIAL_NUMBER_SIZE-1]);
		if (strncasecmp(crc, sn_char, 2)) {
			return sn_invalid;
		}
		sn_char += 2 ;
	}
	return ( *sn_char == '\0' ) ? sn_valid : sn_alias ;
}

