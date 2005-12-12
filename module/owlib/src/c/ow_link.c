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

#include "owfs_config.h"
#include "ow.h"

int LINK_mode ; /* flag to use LINKs in ascii mode */

static int LINK_write(const unsigned char *const buf, const size_t size, const struct parsedname * const pn ) ;
static int LINK_read(unsigned char * const buf, const size_t size, const struct parsedname * const pn ) ;
static int LINK_reset( const struct parsedname * const pn ) ;
static int LINK_next_both(unsigned char * serialnumber, unsigned char search, const struct parsedname * const pn) ;
static int LINK_PowerByte(const unsigned char byte, const unsigned int delay, const struct parsedname * const pn) ;
static int LINK_ProgramPulse( const struct parsedname * const pn ) ;
static int LINK_sendback_data( const unsigned char * const data, unsigned char * const resp, const int len, const struct parsedname * const pn ) ;
static int LINK_select(const struct parsedname * const pn) ;
static int LINK_send_back(const unsigned char * out, unsigned char * in, size_t size, int startbytemode, int endbytemode, const struct parsedname * pn ) ;
static int LINK_reconnect( const struct parsedname * const pn ) ;

static void LINK_setroutines( struct interface_routines * const f ) {
    f->write = LINK_write ;
    f->read  = LINK_read ;
    f->reset = LINK_reset ;
    f->next_both = LINK_next_both ;
    f->PowerByte = LINK_PowerByte ;
    f->ProgramPulse = LINK_ProgramPulse ;
    f->sendback_data = LINK_sendback_data ;
    f->select        = LINK_select ;
    f->overdrive = NULL ;
    f->testoverdrive = NULL ;
    f->reconnect = LINK_reconnect ;
}

/* Called from DS2480_detect, and is set up to DS9097U emulation by default */
int LINK_detect( struct connection_in * in ) {
    struct parsedname pn ;
    struct stateinfo si ;

    pn.si = &si ;
    FS_ParsedName(NULL,&pn) ; // minimal parsename -- no destroy needed
    pn.in = in ;

    // set the baud rate to 9600
    COM_speed(B9600,&pn);
    if ( LINK_reset(&pn)==0 && BUS_write(" ",1,&pn)==0 ) {
        char tmp[36] = "(none)";
        char * stringp = tmp ;
        /* read the version string */
        memset(tmp,0,36) ;
        BUS_read(tmp,36,&pn) ; // ignore return value -- will time out, probably
        COM_flush(&pn) ;

        /* Now find the dot for the version parsing */
        strsep( &stringp , "." ) ;
        if ( stringp && stringp[0] ) {
            switch (stringp[0] ) {
                case '0':
                    in->Adapter = adapter_LINK_10 ;
                    in->adapter_name = "LINK v1.0" ;
                    break ;
                case '1':
                    in->Adapter = adapter_LINK_11 ;
                    in->adapter_name = "LINK v1.1" ;
                    break ;
                case '2':
                default:
                    in->Adapter = adapter_LINK_12 ;
                    in->adapter_name = "LINK v1.2" ;
                    break ;
            }
            /* Set up low-level routines */
            LINK_setroutines( & (in->iroutines) ) ;
            return 0 ;
        }
    }
    LEVEL_DEFAULT("LINK detection error -- back to emulation mode\n");
    return 0  ;
}

static int LINK_reset( const struct parsedname * const pn ) {
    char resp[3] ;
    COM_flush(pn) ;
    if ( BUS_write("r",1,pn) ) return -errno ;
    sleep(1) ;
    if ( BUS_read(resp,3,pn) ) return -errno ;
    switch( resp[0] ) {
        case 'P':
            pn->si->AnyDevices=1 ;
            break ;
        case 'N':
            pn->si->AnyDevices=0 ;
            break ;
        default:
            pn->si->AnyDevices=0 ;
            LEVEL_CONNECT("1-wire bus short circuit.\n")
    }
    return 0 ;
}

static int LINK_next_both(unsigned char * serialnumber, unsigned char search, const struct parsedname * const pn) {
    struct stateinfo * si = pn->si ;
    char resp[20] ;
    int ret ;

//printf("NEXT\n");
    (void) search ;
    if ( !si->AnyDevices ) si->LastDevice = 1 ;
    if ( si->LastDevice ) return -ENODEV ;

    COM_flush(pn) ;
    if ( si->LastDiscrepancy == -1 ) {
        if ( (ret=BUS_write("f",1,pn)) ) return ret ;
        si->LastDiscrepancy = 0 ;
    } else {
        if ( (ret=BUS_write("n",1,pn)) ) return ret ;
    }
    
    if ( (ret=BUS_read(resp,20,pn)) ) return ret ;

    switch ( resp[0] ) {
        case '-':
            si->LastDevice = 1 ;
        case '+':
            break ;
        case 'N' :
            si->AnyDevices = 0 ;
            return -ENODEV ;
        case 'X':
        default :
            return -EIO ;
    }

    serialnumber[7] = string2num(&resp[2]) ;
    serialnumber[6] = string2num(&resp[4]) ;
    serialnumber[5] = string2num(&resp[6]) ;
    serialnumber[4] = string2num(&resp[8]) ;
    serialnumber[3] = string2num(&resp[10]) ;
    serialnumber[2] = string2num(&resp[12]) ;
    serialnumber[1] = string2num(&resp[14]) ;
    serialnumber[0] = string2num(&resp[16]) ;
    
    // CRC check
    if ( CRC8(serialnumber,8) || (serialnumber[0] == 0)) {
        STAT_ADD1(DS2480_next_both_errors);
        /* A minor "error" and should perhaps only return -1 to avoid reconnect */
        return -EIO ;
    }

    if((serialnumber[0] & 0x7F) == 0x04) {
        /* We found a DS1994/DS2404 which require longer delays */
        pn->in->ds2404_compliance = 1 ;
    }

    return 0 ;
}

/* Basic LINK communication -- send a HEX bytes(in ascii) and get them back */
static int LINK_send_back(const unsigned char * out, unsigned char * in, size_t size, int startbytemode, int endbytemode, const struct parsedname * pn ) {
    if ( size > UART_FIFO_SIZE - 2 ) {
        size_t hsize = size >> 1 ;
        return LINK_send_back(out,in,hsize,startbytemode,0,pn) || LINK_send_back(&out[hsize],&in[hsize],size-hsize,0,endbytemode,pn) ;
    } else {
        char * ascii_start = pn->in->combuffer ;
        char * ascii_pointer = ascii_start ;
        int ascii_inlength = size*2 ;

        /* Add the beginning "b" to start byte mode (not echoed) */
        if ( startbytemode ) {
            ascii_pointer[0] = 'b' ;
            ++ascii_pointer ;
        }
        bytes2string( ascii_pointer, out, size ) ;
        ascii_pointer += ascii_inlength ;
        
        /* Add final \n to end byte mode (echoed as \r\n) */
        if ( endbytemode ) {
            ascii_pointer[0] = '\n' ;
            ++ascii_pointer ;
            // ascii_inlength ++ ;
        }
            
        if ( BUS_write(ascii_start,ascii_pointer-ascii_start,pn) ) {
            LEVEL_DATA("Trouble sending data to LINK\n");
            return -EIO ;
        }
        if ( BUS_read( ascii_start, ascii_inlength, pn ) ) {
            printf("LINKread -- Attempting to read %d bytes, got string back: %s\n",ascii_inlength,ascii_start);
            LEVEL_DATA("Trouble sending data to LINK\n");
            return -EIO ;
        }
        string2bytes( ascii_start, in, size) ;
    }
    return 0;
}

/* Assymetric */
/* Read from LINK with timeout on each character */
// NOTE: from PDkit, reead 1-byte at a time
// NOTE: change timeout to 40msec from 10msec for LINK
// returns 0=good 1=bad
/* return 0=good,
          -errno = read error
          -EINTR = timeout
 */
static int LINK_read(unsigned char * const buf, const size_t size, const struct parsedname * const pn ) {
    size_t inlength = size ;
    fd_set fdset;
    ssize_t r ;
    struct timeval tval;
    int ret ;

    while(inlength > 0) {
        if(!pn->in) {
            ret = -EIO;
            STAT_ADD1(DS2480_read_null);
            break;
        }
        // set a descriptor to wait for a character available
        FD_ZERO(&fdset);
        FD_SET(pn->in->fd,&fdset);
        tval.tv_sec = 0;
        tval.tv_usec = 500000;
        /* This timeout need to be pretty big for some reason.
        * Even commands like DS2480_reset() fails with too low
        * timeout. I raise it to 0.5 seconds, since it shouldn't
        * be any bad experience for any user... Less read and
        * timeout errors for users with slow machines. I have seen
        * 276ms delay on my Coldfire board.
        *
        * DS2480_reset()
        *   DS2480_sendback_cmd()
        *     DS2480_sendout_cmd()
        *       DS2480_write()
        *         write()
        *         tcdrain()   (all data should be written on serial port)
        *     DS2480_read()
        *       select()      (waiting 40ms should be enough!)
        *       read()
        *
        */

        // if byte available read or return bytes read
        ret = select(pn->in->fd+1,&fdset,NULL,NULL,&tval);
        if (ret > 0) {
            if( FD_ISSET( pn->in->fd, &fdset )==0 ) {
                ret = -EIO;  /* error */
                STAT_ADD1(DS2480_read_fd_isset);
                break;
            }
            update_max_delay(pn);
            r = read(pn->in->fd,&buf[size-inlength],inlength);
            printf("LINK_read bytes=%d of %d, <%s>\n",r,inlength,buf);
            if ( r < 0 ) {
                if(errno == EINTR) {
                    /* read() was interrupted, try again */
                    STAT_ADD1(DS2480_read_interrupted);
                    continue;
                }
                ret = -errno;  /* error */
                STAT_ADD1(DS2480_read_read);
                break;
            }
            inlength -= r;
        } else if(ret < 0) {
            printf("LINK_read select=%d\n",ret);
            if(errno == EINTR) {
                /* select() was interrupted, try again */
                STAT_ADD1(DS2480_read_interrupted);
                continue;
            }
            STAT_ADD1(DS2480_read_select_errors);
            return -EINTR;
        } else {
            printf("LINK_read select=%d\n",ret);
            STAT_ADD1(DS2480_read_timeout);
            return -EINTR;
        }
    }
    if(inlength > 0) { /* signal that an error was encountered */
        STAT_ADD1(DS2480_read_errors);
        return ret;  /* error */
    }
    return 0;
}

//
// Write a string to the serial port
/* return 0=good,
          -EIO = error
 */
static int LINK_write(const unsigned char *const buf, const size_t size, const struct parsedname * const pn ) {
    ssize_t r, sl = size;

    while(sl > 0) {
        if(!pn->in) break;
        r = write(pn->in->fd,&buf[size-sl],sl) ;
        if(r < 0) {
            if(errno == EINTR) {
                STAT_ADD1(DS2480_write_interrupted);
                continue;
            }
            break;
        }
        sl -= r;
    }
    if(pn->in) {
        tcdrain(pn->in->fd) ;
        gettimeofday( &(pn->in->bus_write_time) , NULL );
    }
    if(sl > 0) {
        STAT_ADD1(DS2480_write_errors);
        return -EIO;
    }
    return 0;
}

//--------------------------------------------------------------------------
// Send 8 bits of communication to the 1-Wire Net and verify that the
// 8 bits read from the 1-Wire Net is the same (write operation).
// The parameter 'byte' least significant 8 bits are used.  After the
// 8 bits are sent change the level of the 1-Wire net.
// Delay delay msec and return to normal
//
/* Returns 0=good
   bad = -EIO
 */
static int LINK_PowerByte(const unsigned char byte, const unsigned int delay, const struct parsedname * const pn) {
    char pow[] = "pXX" ;
    char cr[] = "\n" ; 
    int ret ;
    
    num2string( &pow[1], byte ) ;
    ret = BUS_write(pow,3,pn) ; /* send the data */
    if ( (BUS_read( pow, 2, pn ) < 2) || ret ) ret = -EIO ; /* read echoed data */
    
    // delay
    UT_delay( delay ) ;

    // flush the buffers
    COM_flush(pn);

    pow[0] = '\n' ;
    if ( BUS_write(cr,1,pn) || ret ) ret = -EIO ; /* send a CR */
    if ( (BUS_read( pow, 2, pn ) < 2) || ret ) ret = -EIO ; /* read echoed data */
    
    return ret ;
}

/* Send a 12v 480usec pulse on the 1wire bus to program the EPROM */
/* Note, DS2480_reset must have been called at once in the past for ProgramAvailable setting */
/* returns 0 if good
    -EINVAL if not program pulse available
    -EIO on error
 */
static int LINK_ProgramPulse( const struct parsedname * const pn ) {
    (void) pn ;
    return -EINVAL ;
}

//
// DS2480_sendback_data
//  Send data and return response block
//  puts into data mode if needed.
/* return 0=good
   sendout_data, readin
 */
static int LINK_sendback_data( const unsigned char * const data, unsigned char * const resp, const int len, const struct parsedname * const pn ) {
    return LINK_send_back(data, resp, len, 1, 1, pn ) ;
}

static int LINK_reconnect( const struct parsedname * const pn ) {
    STAT_ADD1(BUS_reconnect);

    if ( !pn || !pn->in ) return -EIO;
    STAT_ADD1(pn->in->bus_reconnect);

    COM_close(pn->in);
    usleep(100000);
    if(!COM_open(pn->in)) {
        if(!DS2480_detect(pn->in)) {
            LEVEL_DEFAULT("LINK adapter reconnected\n");
            return 0;
        }
    }
    STAT_ADD1(BUS_reconnect_errors);
    STAT_ADD1(pn->in->bus_reconnect_errors);
    LEVEL_DEFAULT("Failed to reconnect LINK adapter!\n");
    return -EIO ;
}

static int LINK_select(const struct parsedname * const pn) {
    if ( pn->pathlength > 0 ) {
        LEVEL_CALL("Attempt to use a branched path (DS2409 main or aux) with the ascii-mode LINK\n") ;
        return -ENOTSUP ; /* cannot do branching with LINK ascii */
    }
    return BUS_select_low(pn) ;
}

