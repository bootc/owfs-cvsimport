/*
$Id$
    OW -- One-Wire filesystem
    version 0.4 7/2/2003

    Function naming scheme:
    OW -- Generic call to interaface
    LI -- LINK commands
    L1 -- 2480B commands
    FS -- filesystem commands
    UT -- utility functions

    Written 2003 Paul H Alfille
        Fuse code based on "fusexmp" {GPL} by Miklos Szeredi, mszeredi@inf.bme.hu
        Serial code based on "xt" {GPL} by David Querbach, www.realtime.bc.ca
        in turn based on "miniterm" by Sven Goldt, goldt@math.tu.berlin.de
    GPL license
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    Other portions based on Dallas Semiconductor Public Domain Kit,
    ---------------------------------------------------------------------------
    Copyright (C) 2000 Dallas Semiconductor Corporation, All Rights Reserved.
        Permission is hereby granted, free of charge, to any person obtaining a
        copy of this software and associated documentation files (the "Software"),
        to deal in the Software without restriction, including without limitation
        the rights to use, copy, modify, merge, publish, distribute, sublicense,
        and/or sell copies of the Software, and to permit persons to whom the
        Software is furnished to do so, subject to the following conditions:
        The above copyright notice and this permission notice shall be included
        in all copies or substantial portions of the Software.
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
    OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY,  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL DALLAS SEMICONDUCTOR BE LIABLE FOR ANY CLAIM, DAMAGES
    OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
    ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
    OTHER DEALINGS IN THE SOFTWARE.
        Except as contained in this notice, the name of Dallas Semiconductor
        shall not be used except as stated in the Dallas Semiconductor
        Branding Policy.
    ---------------------------------------------------------------------------
    Implementation:
    25-05-2003 iButtonLink device
*/

#ifndef OW_H  /* tedious wrapper */
#define OW_H

#ifndef OWFS_CONFIG_H
 #error Please make sure owfs_config.h is included *before* this header file
#endif

// Define this to avoid some VALGRIND warnings... (just for testing)
// Warning: This will partially remove the multithreaded support since ow_net.c
// will wait for a thread to complete before executing a new one.
//#define VALGRIND 1

#define _FILE_OFFSET_BITS   64
#ifdef HAVE_FEATURES_H
    #include <features.h>
#endif
#ifdef HAVE_FEATURE_TESTS_H
    #include <feature_tests.h>
#endif
#ifdef HAVE_SYS_STAT_H
    #include <sys/stat.h> /* for stat */
#endif
#ifdef HAVE_SYS_TYPES_H
    #include <sys/types.h> /* for stat */
#endif
#include <sys/times.h> /* for times */
#include <ctype.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#ifdef HAVE_STDINT_H
 #include <stdint.h> /* for bit twiddling */
#endif

#include <unistd.h>
#include <fcntl.h>
#ifndef __USE_XOPEN
 #define __USE_XOPEN /* for strptime fuction */
 #include <time.h>
 #undef __USE_XOPEN /* for strptime fuction */
#else
 #include <time.h>
#endif
#include <termios.h>
#include <errno.h>
#include <syslog.h>
//#include <sys/file.h> /* for flock */
#ifdef HAVE_GETOPT_H
 #include <getopt.h> /* for long options */
#endif

#include <sys/uio.h>
#include <sys/time.h> /* for gettimeofday */
#ifdef HAVE_SYS_SOCKET_H
 #include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
 #include <netinet/in.h>
#endif
#include <netdb.h> /* addrinfo */

/* Can't include search.h when compiling owperl on Fedora Core 1. */
#ifndef SKIP_SEARCH_H
 #ifndef __USE_GNU
  #define __USE_GNU
  #include <search.h>
  #undef __USE_GNU
 #else /* __USE_GNU */
  #include <search.h>
 #endif /* __USE_GNU */
#endif /* SKIP_SEARCH_H */

/* Include some compatibility functions */
#include "compat.h"

/* Debugging and error messages separated out for readability */
#include "ow_debug.h"


/* Floating point */
/* I hate to do this, making everything a double */
/* The compiler complains mercilessly, however */
/* 1-wire really is low precision -- float is more than enough */
typedef double          FLOAT ;
typedef time_t          DATE ;
typedef unsigned char   BYTE ;
typedef char            ASCII ;
typedef unsigned int    UINT ;
typedef int             INT ;

/* Directory blob separated out for readability */
#include "ow_dirblob.h"

/*
    OW -- One Wire
    Global variables -- each invokation will have it's own data
*/

/* command line options */
/* These are the owlib-specific options */
#define OWLIB_OPT "m:c:f:p:s:hu::d:t:CFRKVP:"
extern const struct option owopts_long[] ;
enum opt_program { opt_owfs, opt_server, opt_httpd, opt_ftpd, opt_tcl, opt_swig, opt_c, } ;
int owopt( const int c , const char * arg ) ;
int owopt_packed( const char * params ) ;

/* Several different structures:
  device -- one for each type of 1-wire device
  filetype -- one for each type of file
  parsedname -- translates a path into usable form
*/

/* --------------------------------------------------------- */
/* Filetypes -- directory entries for each 1-wire chip found */
/*
Filetype is the most elaborate of the internal structures, though
simple in concept.

Actually a little misnamed. Each filetype corresponds to a device
property, and to a file in the file system (though there are Filetype
entries for some directory elements too)

Filetypes belong to a particular device. (i.e. each device has it's list
of filetypes) and have a name and pointers to processing functions. Filetypes
also have a data format (integer, ascii,...) and a data length, and an indication
of whether the property is static, changes only on command, or is volatile.

Some properties occur are arrays (pages of memory, logs of temperature
values). The "aggregate" structure holds to allowable size, and the method
of access. -- Aggregate properties are either accessed all at once, then
split, or accessed individually. The choice depends on the device hardware.
There is even a further wrinkle: mixed. In cases where the handling can be either,
mixed causes separate handling of individual items are querried, and combined
if ALL are requested. This is useful for the DS2450 quad A/D where volt and PIO functions
step on each other, but the conversion time for individual is rather costly.
 */

enum ag_index {ag_numbers, ag_letters, } ;
enum ag_combined { ag_separate, ag_aggregate, ag_mixed, } ;
/* aggregate defines array properties */
struct aggregate {
    int elements ; /* Maximum number of elements */
    enum ag_index letters ; /* name them with letters or numbers */
    enum ag_combined combined ; /* Combined bitmaps properties, or separately addressed */
} ;
extern struct aggregate Asystem ; /* for remote directories */

    /* property format, controls web display */
/* Some explanation of ft_format:
     Each file type is either a device (physical 1-wire chip or virtual statistics container).
     or a file (property).
     The devices act as directories of properties.
     The files are either properties of the device, or sometimes special directories themselves.
     If properties, they can be integer, text, etc or special directory types.
     There is also the directory type, ft_directory reflects a branch type, which restarts the parsing process.
*/
enum ft_format {
    ft_directory,
    ft_subdir,
    ft_integer,
    ft_unsigned,
    ft_float,
    ft_ascii,
    ft_vascii, // variable length ascii -- must be read and measured.
    ft_binary,
    ft_yesno,
    ft_date,
    ft_bitfield,
    ft_temperature,
    ft_tempgap,
} ;
    /* property changability. Static unchanged, Stable we change, Volatile changes */
enum fc_change { fc_local, fc_static, fc_stable, fc_Astable, fc_volatile, fc_Avolatile, fc_second, fc_statistic, fc_persistent, fc_directory, fc_presence, } ;

/* Predeclare parsedname */
struct parsedname ;

/* predeclare connection_in/out */
struct connection_in ;

/* Exposed connection info */
extern int indevices ;

/* Maximum length of a file or directory name, and extension */
#define OW_NAME_MAX      (32)
#define OW_EXT_MAX       (6)
#define OW_FULLNAME_MAX  (OW_NAME_MAX+OW_EXT_MAX)
#define OW_DEFAULT_LENGTH (128)

/* filetype gives -file types- for chip properties */
struct filetype {
    char * name ;
    int suglen ; // length of field
    struct aggregate * ag ; // struct pointer for aggregate
    enum ft_format format ; // type of data
    enum fc_change change ; // volatility
    union {
        void * v ;
        int (*i) (int *, const struct parsedname *);
        int (*u) (UINT *, const struct parsedname *);
        int (*f) (FLOAT *, const struct parsedname *);
        int (*y) (int *, const struct parsedname *);
        int (*d) (DATE *, const struct parsedname *);
        int (*a) (char *, const size_t, const off_t, const struct parsedname *);
        int (*b) (BYTE *, const size_t, const off_t, const struct parsedname *);
    } read ; // read callback function
    union {
        void * v ;
        int (*i) (const int *, const struct parsedname *);
        int (*u) (const UINT *, const struct parsedname *);
        int (*f) (const FLOAT *, const struct parsedname *);
        int (*y) (const int *, const struct parsedname *);
        int (*d) (const DATE *, const struct parsedname *);
        int (*a) (const char *, const size_t, const off_t, const struct parsedname *);
        int (*b) (const BYTE *, const size_t, const off_t, const struct parsedname *);
    } write ; // write callback function
    union {
        void * v ;
        int    i ;
        UINT u ;
        FLOAT  f ;
        size_t s ;
        BYTE c ;
    } data ; // extra data pointer (used for separating similar but differently name functions)
} ;
#define NFT(ft) ((int)(sizeof(ft)/sizeof(struct filetype)))

/* --------- end Filetype -------------------- */
/* ------------------------------------------- */

/* Internal properties -- used by some devices */
/* in passing to store state information        */
struct internal_prop {
    char * name ;
    enum fc_change change ;
} ;

/* -------------------------------- */
/* Devices -- types of 1-wire chips */
/*
device structure corresponds to 1-wire device
also to virtual devices, like statistical groupings
and interfaces (LINK, DS2408, ... )

devices have a list or properties that appear as
files under the device directory, they correspond
to device features (memory, name, temperature) and
bound the allowable files in a device directory
*/

/* Device tree for matching names */
/* Bianry tree implementation */
/* A separate root for each device type: real, statistics, settings, system, structure */
extern void * Tree[6] ;

    /* supports RESUME command */
#define DEV_resume  0x0001
    /* can trigger an alarm */
#define DEV_alarm   0x0002
    /* support OVERDRIVE */
#define DEV_ovdr    0x0004
    /* responds to simultaneous temperature convert 0x44 */
#define DEV_temp    0x8000
    /* responds to simultaneous voltage convert 0x3C */
#define DEV_volt    0x4000


struct device {
    const char * code ;
    char * name ;
    uint32_t flags ;
    int nft ;
    struct filetype * ft ;
} ;

#define DeviceHeader( chip )    extern struct device d_##chip
//#define DeviceEntry( code , chip )  { #code, #chip, 0, NFT(chip), chip } ;
/* Entries for struct device */
/* Cannot set the 3rd element (number of filetypes) at compile time because
   filetype arrays aren;t defined at this point */
#define DeviceEntryExtended( code , chip , flags )  struct device d_##chip = { #code, #chip, flags ,  NFT(chip), chip }
#define DeviceEntry( code , chip )  DeviceEntryExtended( code, chip, 0 )

/* Bad bad C library */
/* implementation of tfind, tsearch returns an opaque structure */
/* you have to know that the first element is a pointer to your data */
struct device_opaque {
    struct device * key ;
    void * other ;
} ;

/* Must be sorted for bsearch */
//extern struct device * Devices[] ;
//extern size_t nDevices ;
extern struct device NoDevice ;
extern struct device * DeviceSimultaneous ;
extern struct device * DeviceThermostat ;

/* ---- end device --------------------- */
/* ------------------------------------- */


/* ------------------------}
struct devlock {
    BYTE sn[8] ;
    pthread_mutex_t lock ;
    int users ;
} ;
extern struct devlock DevLock[] ;
-------------------- */
/* Parsedname -- path converted into components */
/*
Parsed name is the primary structure interpreting a
owfs systrem call. It is the interpretation of the owfs
file name, or the owhttpd URL. It contains everything
but the operation requested. The operation (read, write
or directory is in the extended URL or the actual callback
function requested).
*/
/*
Parsed name has several components:
sn is the serial number of the device
dev and ft are pointers to device and filetype
  members corresponding to the element
buspath and pathlength interpret the route through
  DS2409 branch controllers
filetype and extension correspond to property
  (filetype) details
subdir points to in-device groupings
*/

/* Semi-global information (for remote control) */
    /* bit0: cacheenabled  bit1: return bus-list */
    /* presencecheck */
    /* tempscale */
    /* device format */
extern int32_t SemiGlobal ;

/* Unique token for owserver loop checks */
union antiloop {
    struct {
        pid_t pid ;
        clock_t clock ;
    } simple ;
    BYTE uuid[16] ;
} ;

/* Global information (for local control) */
struct global {
    int announce_off ; // use zeroconf?
    ASCII * announce_name ;
#if OW_ZERO
    DNSServiceRef browse ;
#endif /* OW_ZERO */
    enum opt_program opt ;
    ASCII * progname ;
    union antiloop Token ;
    int want_background ;
    int now_background ;
    int error_level ;
    int error_print ;
    int readonly ;
    char * SimpleBusName ;
    int max_clients ; // for ftp
    int autoserver ;
    /* Special parameter to trigger William Robison <ibutton@n952.dyndns.ws> timings */
    int altUSB ;
    /* timeouts -- order must match ow_opt.c values for correct indexing */
    int timeout_volatile ;
    int timeout_stable ;
    int timeout_directory ;
    int timeout_presence ;
    int timeout_serial ;
    int timeout_usb ;
    int timeout_network ;
    int timeout_server ;
    int timeout_ftp ;

} ;
extern struct global Global ;

struct buspath {
    BYTE sn[8] ;
    BYTE branch ;
} ;

struct devlock {
    BYTE sn[8] ;
    UINT users ;
    pthread_mutex_t lock ;
} ;

enum pn_type { pn_real=0, pn_statistics, pn_system, pn_settings, pn_structure } ;
enum pn_state { pn_normal=0, pn_uncached=1, pn_alarm=2, pn_text=4, pn_bus=8, pn_buspath = 16, } ;
struct parsedname {
    char * path ; // text-more device name
    char * path_busless ; // pointer to path without first bus
    int    bus_nr ;
    enum pn_type type ; // global branch
    enum pn_state state ; // global branch
    BYTE sn[8] ; // 64-bit serial number
    struct device * dev ; // 1-wire device
    struct filetype * ft ; // device property
    int    extension ; // numerical extension (for array values) or -1
    struct filetype * subdir ; // in-device grouping
    UINT pathlength ; // DS2409 branching depth
    struct buspath * bp ; // DS2409 branching route
    struct connection_in * indevice ; // Global indevice at definition time
    struct connection_in * in ;
    uint32_t sg ; // more state info, packed for network transmission
    struct devlock ** lock ; // need to clear dev lock?
    int tokens ; /* for anti-loop work */
    BYTE * tokenstring ; /* List of tokens from owservers passed */
} ;

enum simul_type { simul_temp, simul_volt, } ;
#define SpecifiedBus(pn)  ((((pn)->state) & pn_buspath) != 0 )

/* ---- end Parsedname ----------------- */
/* ------------------------------------- */

/* Delay for clearing buffer */
#define    WASTE_TIME    (2)

/* device display format */
enum deviceformat { fdi, fi, fdidc, fdic, fidc, fic } ;
/* Gobal temperature scale */
enum temp_type { temp_celsius, temp_fahrenheit, temp_kelvin, temp_rankine, } ;
FLOAT Temperature( FLOAT C, const struct parsedname * pn) ;
FLOAT TemperatureGap( FLOAT C, const struct parsedname * pn) ;
FLOAT fromTemperature( FLOAT T, const struct parsedname * pn) ;
FLOAT fromTempGap( FLOAT T, const struct parsedname * pn) ;
const char *TemperatureScaleName(enum temp_type t) ;

extern int cacheavailable ; /* is caching available */

extern void set_signal_handlers( void (*exit_handler)(int errcode) ) ;

/* Server (Socket-based) interface */
enum msg_classification {
    msg_error,
    msg_nop,
    msg_read,
    msg_write,
    msg_dir,
    msg_size, // No longer used, leave here to compatibility
    msg_presence,
} ;
/* message to owserver */
struct server_msg {
    int32_t version ;
    int32_t payload ;
    int32_t type ;
    int32_t sg ;
    int32_t size ;
    int32_t offset ;
} ;

/* message to client */
struct client_msg {
    int32_t version ;
    int32_t payload ;
    int32_t ret ;
    int32_t sg ;
    int32_t size ;
    int32_t offset ;
} ;

struct serverpackage {
    ASCII * path ;
    BYTE * data ;
    size_t datasize ;
    BYTE * tokenstring ;
    size_t tokens ;
} ;

#define Servermessage       (((int32_t)1)<<16)
#define isServermessage( version )    (((version)&Servermessage)!=0)
#define Servertokens(version)    ((version)&0xFFFF)
/* -------------------------------------------- */
/* start of program -- for statistics amd file atrtributes */
extern time_t start_time ;
extern time_t dir_time ; /* time of last directory scan */

/* Prototypes */
#define iREAD_FUNCTION( fname )  static int fname(int *, const struct parsedname *)
#define uREAD_FUNCTION( fname )  static int fname(UINT *, const struct parsedname * pn)
#define fREAD_FUNCTION( fname )  static int fname(FLOAT *, const struct parsedname * pn)
#define dREAD_FUNCTION( fname )  static int fname(DATE *, const struct parsedname * pn)
#define yREAD_FUNCTION( fname )  static int fname(int *, const struct parsedname * pn)
#define aREAD_FUNCTION( fname )  static int fname(char *buf, const size_t size, const off_t offset, const struct parsedname * pn)
#define bREAD_FUNCTION( fname )  static int fname(BYTE *buf, const size_t size, const off_t offset, const struct parsedname * pn)

#define iWRITE_FUNCTION( fname )  static int fname(const int *, const struct parsedname * pn)
#define uWRITE_FUNCTION( fname )  static int fname(const UINT *, const struct parsedname * pn)
#define fWRITE_FUNCTION( fname )  static int fname(const FLOAT *, const struct parsedname * pn)
#define dWRITE_FUNCTION( fname )  static int fname(const DATE *, const struct parsedname * pn)
#define yWRITE_FUNCTION( fname )  static int fname(const int *, const struct parsedname * pn)
#define aWRITE_FUNCTION( fname )  static int fname(const char *buf, const size_t size, const off_t offset, const struct parsedname * pn)
#define bWRITE_FUNCTION( fname )  static int fname(const BYTE *buf, const size_t size, const off_t offset, const struct parsedname * pn)

int FS_ParsedNamePlus( const char * path, const char * file, struct parsedname * pn ) ;
int FS_ParsedName( const char * fn , struct parsedname * pn ) ;
int FS_ParsedName_Remote( const char * fn , struct parsedname * pn ) ;
void FS_ParsedName_destroy( struct parsedname * pn ) ;
size_t FileLength( const struct parsedname * pn ) ;
size_t FullFileLength( const struct parsedname * pn ) ;
size_t SimpleFileLength( const struct parsedname * pn ) ;
size_t SimpleFullFileLength( const struct parsedname * pn ) ;
int CheckPresence( const struct parsedname * pn ) ;
void FS_devicename( char * buffer, const size_t length, const BYTE * sn, const struct parsedname * pn ) ;
void FS_devicefind( const char * code, struct parsedname * pn ) ;
void FS_devicefindhex( BYTE f, struct parsedname * pn ) ;

int FS_dirname_state( char * buffer, const size_t length, const struct parsedname * pn ) ;
int FS_dirname_type( char * buffer, const size_t length, const struct parsedname * pn ) ;
void FS_DirName( char * buffer, const size_t size, const struct parsedname * pn ) ;
int FS_FileName( char * name, const size_t size, const struct parsedname * pn ) ;

int Simul_Test( const enum simul_type type, UINT msec, const struct parsedname * pn ) ;
int Simul_Clear( const enum simul_type type, const struct parsedname * pn ) ;

// ow_locks.c
void LockSetup( void ) ;
int LockGet( const struct parsedname * pn ) ;
void LockRelease( const struct parsedname * pn ) ;

/* 1-wire lowlevel */
void UT_delay(const UINT len) ;
void UT_delay_us(const unsigned long len) ;

ssize_t readn(int fd, void *vptr, size_t n, const struct timeval * ptv ) ;
int ClientAddr(  char * sname, struct connection_in * in ) ;
int ClientConnect( struct connection_in * in ) ;
void FreeClientAddr(  struct connection_in * in ) ;

int OW_ArgNet( const char * arg ) ;

void ow_help( void ) ;
void ow_morehelp( void ) ;

void update_max_delay( const struct parsedname * pn ) ;

int ServerPresence( const struct parsedname * pn ) ;
int ServerRead( char * buf, const size_t size, const off_t offset, const struct parsedname * pn ) ;
int ServerWrite( const char * buf, const size_t size, const off_t offset, const struct parsedname * pn ) ;
int ServerDir( ASCII * path ) ;

int FS_write(const char *path, const char *buf, const size_t size, const off_t offset) ;
int FS_write_postparse(const char *buf, const size_t size, const off_t offset, const struct parsedname * pn) ;

int FS_read(const char *path, char *buf, const size_t size, const off_t offset) ;
int FS_read_postparse(char *buf, const size_t size, const off_t offset, const struct parsedname * pn ) ;
int FS_read_postpostparse(char *buf, const size_t size, const off_t offset, const struct parsedname * pn ) ;
int FS_read_fake( char *buf, const size_t size, const off_t offset, const struct parsedname * pn ) ;
int FS_output_ascii( ASCII * buf, size_t size, off_t offset, ASCII * answer, size_t length ) ;

int FS_output_unsigned( UINT value, char * buf, const size_t size, const struct parsedname * pn ) ;
int FS_output_integer( int value, char * buf, const size_t size, const struct parsedname * pn ) ;
int FS_output_float( FLOAT value, char * buf, const size_t size, const struct parsedname * pn ) ;
int FS_output_date( DATE value, char * buf, const size_t size, const struct parsedname * pn ) ;

int FS_output_unsigned_array( UINT * values, char * buf, const size_t size, const struct parsedname * pn ) ;
int FS_output_integer_array( int * values, char * buf, const size_t size, const struct parsedname * pn ) ;
int FS_output_float_array( FLOAT * values, char * buf, const size_t size, const struct parsedname * pn ) ;
int FS_output_date_array( DATE * values, char * buf, const size_t size, const struct parsedname * pn ) ;

int FS_fstat(const char *path, struct stat *stbuf) ;
int FS_fstat_postparse(struct stat *stbuf, const struct parsedname * pn ) ;

/* iteration functions for splitting writes to buffers */
int OW_read_paged( BYTE * p, size_t size, off_t offset, const struct parsedname * pn,
    size_t pagelen, int (*readfunc)(BYTE *,const size_t,const off_t,const struct parsedname *) ) ;
int OW_write_paged( const BYTE * p, size_t size, off_t offset, const struct parsedname * pn,
    size_t pagelen, int (*writefunc)(const BYTE *,const size_t,const off_t,const struct parsedname *) ) ;
    
int OW_r_mem_simple( BYTE * data, const size_t size, const off_t offset, const struct parsedname * pn ) ;
int OW_r_mem_crc16( BYTE * data , const size_t size, const off_t offset , const struct parsedname * pn, size_t pagesize ) ;
int OW_r_mem_p8_crc16( BYTE * data , const size_t size, const off_t offset , const struct parsedname * pn, size_t pagesize, BYTE * extra ) ;

#define CACHE_MASK     ( (UINT) 0x00000001 )
#define CACHE_BIT      0
#define BUSRET_MASK    ( (UINT) 0x00000002 )
#define BUSRET_BIT     1
//#define PRESENCE_MASK  ( (UINT) 0x0000FF00 )
//#define PRESENCE_BIT   8
#define TEMPSCALE_MASK ( (UINT) 0x00FF0000 )
#define TEMPSCALE_BIT  16
#define DEVFORMAT_MASK ( (UINT) 0xFF000000 )
#define DEVFORMAT_BIT  24
#define IsLocalCacheEnabled(ppn)  ( ((ppn)->sg & CACHE_MASK) )
#define ShouldReturnBusList(ppn)  ( ((ppn)->sg & BUSRET_MASK) )
#define TemperatureScale(ppn)     ( (enum temp_type) (((ppn)->sg & TEMPSCALE_MASK) >> TEMPSCALE_BIT) )
#define SGTemperatureScale(sg)    ( (enum temp_type)(((sg) & TEMPSCALE_MASK) >> TEMPSCALE_BIT) )
#define DeviceFormat(ppn)         ( (enum deviceformat) (((ppn)->sg & DEVFORMAT_MASK) >> DEVFORMAT_BIT) )
#define set_semiglobal(s, mask, bit, val) do { *(s) = (*(s) & ~(mask)) | ((val)<<bit); } while(0)

#define IsDir( pn )    ( ((pn)->dev)==NULL \
                      || ((pn)->ft)==NULL  \
                      || ((pn)->ft)->format==ft_subdir \
                      || ((pn)->ft)->format==ft_directory )
#define NotUncachedDir(pn)    ( (((pn)->state)&pn_uncached) == 0 )
#define  IsUncachedDir(pn)    ( ! NotUncachedDir(pn) )
#define    NotAlarmDir(pn)    ( (((pn)->state)&pn_alarm) == 0 )
#define     IsAlarmDir(pn)    ( ! NotAlarmDir(pn) )
#define     NotRealDir(pn)    ( ((pn)->type) != pn_real )
#define      IsRealDir(pn)    ( ((pn)->type) == pn_real )
#endif /* OW_H */

#ifndef OW_CONNECTION_H  /* tedious wrapper */
#define OW_CONNECTION_H

struct connection_in ;
struct device_search ;
/* -------------------------------------------- */
/* Interface-specific routines ---------------- */
struct interface_routines {
    /* Detect if adapter is present, and open -- usually called outside of this routine */
    int (* detect) ( struct connection_in * in ) ;
    /* reset the interface -- actually the 1-wire bus */
    int (* reset ) (const struct parsedname * pn ) ;
    /* Bulk of search routine, after set ups for first or alarm or family */
    int (* next_both) (struct device_search * ds, const struct parsedname * pn) ;
    /* Change speed between overdrive and normal on the 1-wire bus */
    int (* overdrive) (const UINT overdrive, const struct parsedname * pn) ;
    /* Change speed between overdrive and normal on the 1-wire bus */
    int (* testoverdrive) (const struct parsedname * pn) ;
    /* Send a byte with bus power to follow */
    int (* PowerByte) (const BYTE byte, BYTE * resp, const UINT delay, const struct parsedname * pn) ;
    /* Send a 12V 480msec oulse to program EEPROM */
    int (* ProgramPulse) (const struct parsedname * pn) ;
    /* send and recieve data -- byte at a time */
    int (* sendback_data) (const BYTE * data , BYTE * resp , const size_t len, const struct parsedname * pn ) ;
    /* send and recieve data -- bit at a time */
    int (* sendback_bits) (const BYTE * databits , BYTE * respbits , const size_t len, const struct parsedname * pn ) ;
    /* select a device */
    int (* select) ( const struct parsedname * pn ) ;
    /* reconnect with a balky device */
    int (* reconnect) ( const struct parsedname * pn ) ;
    /* Close the connection (port) */
    void (* close) ( struct connection_in * in ) ;
    /* transaction */
    int (* transaction)( const struct transaction_log * tl, const struct parsedname * pn ) ;
    /* capabilities flags */
    UINT flags ;
} ;
#define BUS_detect(in)                      (((in)->iroutines.detect(in)))
//#define BUS_sendback_data(data,resp,len,pn) (((pn)->in->iroutines.sendback_data)((data),(resp),(len),(pn)))
#define BUS_sendback_bits(data,resp,len,pn) (((pn)->in->iroutines.sendback_bits)((data),(resp),(len),(pn)))
//#define BUS_next_both(ds,pn)                (((pn)->in->iroutines.next_both)((ds),(pn)))
#define BUS_ProgramPulse(pn)                (((pn)->in->iroutines.ProgramPulse)(pn))
//#define BUS_PowerByte(byte,resp,delay,pn)   (((pn)->in->iroutines.PowerByte)((byte),(resp),(delay),(pn)))
//#define BUS_select(pn)                      (((pn)->in->iroutines.select)(pn))
#define BUS_overdrive(speed,pn)             (((pn)->in->iroutines.overdrive)((speed),(pn)))
#define BUS_testoverdrive(pn)               (((pn)->in->iroutines.testoverdrive)((pn)))
#define BUS_close(in)                       (((in)->iroutines.close(in)))

/* placed in iroutines.flags */
#define ADAP_FLAG_overdrive     0x00000001
#define ADAP_FLAG_2409path      0x00000010
#define ADAP_FLAG_dirgulp       0x00000100

struct connin_server {
    char * host ;
    char * service ;
    struct addrinfo * ai ;
    struct addrinfo * ai_ok ;
    char * type ; // for zeroconf
    char * domain ; // for zeroconf
    char * fqdn ;
} ;

//enum server_type { srv_unknown, srv_direct, srv_client, src_
/* Network connection structure */
enum bus_mode {
    bus_unknown=0,
    bus_serial,
    bus_usb,
    bus_parallel,
    bus_server,
    bus_zero,
    bus_i2c,
    bus_ha7 ,
    bus_fake ,
    bus_link ,
    bus_elink ,
} ;

enum adapter_type {
    adapter_DS9097   =  0 ,
    adapter_DS1410   =  1 ,
    adapter_DS9097U2 =  2 ,
    adapter_DS9097U  =  3 ,
    adapter_LINK     =  7 ,
    adapter_DS9490   =  8 ,
    adapter_tcp      =  9 ,
    adapter_Bad      = 10 ,
    adapter_LINK_10       ,
    adapter_LINK_11       ,
    adapter_LINK_12       ,
    adapter_LINK_E        ,
    adapter_DS2482_100    ,
    adapter_DS2482_800    ,
    adapter_HA7           ,
    adapter_fake          ,
} ;

enum e_reconnect {
    reconnect_bad = -1 ,
    reconnect_ok = 0 ,
    reconnect_error = 5 ,
} ;

struct device_search {
    int LastDiscrepancy ; // for search
    int LastDevice ; // for search
    BYTE sn[8] ;
    BYTE search ;
} ;

struct connection_in {
    struct connection_in * next ;
    int index ;
    char * name ;
    int fd ;
    enum e_reconnect reconnect_state ;
    struct timeval last_lock ; /* statistics */
    struct timeval last_unlock ;
    UINT bus_reconnect ;
    UINT bus_reconnect_errors ;
    UINT bus_locks ;
    UINT bus_unlocks ;
    UINT bus_errors ;
    struct timeval bus_time ;

    struct timeval bus_read_time ;
    struct timeval bus_write_time ; /* for statistics */
  
    enum bus_mode busmode ;
    struct interface_routines iroutines ;
    enum adapter_type Adapter ;
    char * adapter_name ;
    int AnyDevices ;
    int ExtraReset ; // DS1994/DS2404 might need an extra reset
    int use_overdrive_speed ;
    int ds2404_compliance ;
    int ProgramAvailable ;
    size_t last_root_devs ;
    struct buspath branch ; // Branch currently selected

    union {
        struct connin_server server ;
    } connin ;
} ;

extern struct connection_in * indevice ;

/* This bug-fix/workaround function seem to be fixed now... At least on
 * the platforms I have tested it on... printf() in owserver/src/c/owserver.c
 * returned very strange result on c->busmode before... but not anymore */
enum bus_mode get_busmode(struct connection_in *c);
int is_servermode(struct connection_in *in) ;

void FreeIn( void ) ;
void DelIn( struct connection_in * in ) ;

struct connection_in * NewIn( const struct connection_in * in ) ;
struct connection_out * NewOut( void ) ;
struct connection_in *find_connection_in(int nr);

int Server_detect( struct connection_in * in  ) ;

int BUS_reset(const struct parsedname * pn) ;
int BUS_first(struct device_search * ds, const struct parsedname * pn) ;
int BUS_next(struct device_search * ds, const struct parsedname * pn) ;
int BUS_first_alarm(struct device_search * ds, const struct parsedname * pn) ;
int BUS_first_family(const BYTE family, struct device_search * ds, const struct parsedname * pn ) ;

int BUS_select(const struct parsedname * pn) ;
int BUS_select_branch( const struct parsedname * pn) ;

int BUS_sendout_cmd(const BYTE * cmd , const size_t len, const struct parsedname * pn  ) ;
int BUS_send_cmd(const BYTE * cmd , const size_t len, const struct parsedname * pn  ) ;
int BUS_sendback_cmd(const BYTE * cmd , BYTE * resp , const size_t len, const struct parsedname * pn  ) ;
int BUS_send_data(const BYTE * data , const size_t len, const struct parsedname * pn  ) ;
int BUS_readin_data(BYTE * data , const size_t len, const struct parsedname * pn ) ;
int BUS_verify(BYTE search, const struct parsedname * pn) ;

int BUS_PowerByte(const BYTE byte, BYTE * resp, UINT delay, const struct parsedname * pn) ;
int BUS_next_both(struct device_search * ds, const struct parsedname * pn) ;
int BUS_sendback_data( const BYTE * data, BYTE * resp , const size_t len, const struct parsedname * pn ) ;

int TestConnection( const struct parsedname * pn ) ;

int BUS_transaction( const struct transaction_log * tl, const struct parsedname * pn ) ;
int BUS_transaction_nolock( const struct transaction_log * tl, const struct parsedname * pn ) ;

#define STAT_ADD1_BUS( err, in )     STATLOCK; ++err; ++(in->bus_errors) ; STATUNLOCK ;

#endif /* OW_CONNECTION_H */
