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

/* Stats are a pseudo-device -- they are a file-system entry and handled as such,
     but have a different caching type to distiguish their handling */

#include "owfs_config.h"
#include "ow_stats.h"

/* ------- Prototypes ----------- */
/* Statistics reporting */
 uREAD_FUNCTION( FS_stat ) ;
 fREAD_FUNCTION( FS_time ) ;
 uREAD_FUNCTION( FS_elapsed ) ;

/* -------- Structures ---------- */
struct filetype stats_cache[] = {
    {"flips"           , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_flips      , } ,
    {"additions"       , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_adds       , } ,
    {"primary"         ,  0, NULL  , ft_subdir  , ft_statistic, {v:NULL}   , {v:NULL}, NULL               , } ,
    {"primary/now"     , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & new_avg.current  , } ,
    {"primary/sum"     , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & new_avg.sum      , } ,
    {"primary/num"     , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & new_avg.count    , } ,
    {"primary/max"     , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & new_avg.max      , } ,
    {"secondary"       ,  0, NULL  , ft_subdir  , ft_statistic, {v:NULL}   , {v:NULL}, NULL               , } ,
    {"secondary/now"   , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & old_avg.current  , } ,
    {"secondary/sum"   , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & old_avg.sum      , } ,
    {"secondary/num"   , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & old_avg.count    , } ,
    {"secondary/max"   , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & old_avg.max      , } ,
    {"persistent"      ,  0, NULL  , ft_subdir  , ft_statistic, {v:NULL}   , {v:NULL}, NULL               , } ,
    {"persistent/now"  , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & store_avg.current, } ,
    {"persistent/sum"  , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & store_avg.sum    , } ,
    {"persistent/num"  , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & store_avg.count  , } ,
    {"persistent/max"  , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & store_avg.max    , } ,
    {"external"        ,  0, NULL  , ft_subdir  , ft_statistic, {v:NULL}   , {v:NULL}, NULL               , } ,
    {"external/tries"  , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_ext.tries  , } ,
    {"external/hits"   , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_ext.hits   , } ,
    {"external/added"  , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_ext.adds, } ,
    {"external/expired", 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_ext.expires, } ,
    {"external/deleted", 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_ext.deletes, } ,
    {"internal"        ,  0, NULL  , ft_subdir  , ft_statistic, {v:NULL}   , {v:NULL}, NULL               , } ,
    {"internal/tries"  , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_int.tries  , } ,
    {"internal/hits"   , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_int.hits   , } ,
    {"internal/added"  , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_int.adds, } ,
    {"internal/expired", 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_int.expires, } ,
    {"internal/deleted", 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_int.deletes, } ,
    {"directory"        , 0, NULL  , ft_subdir  , ft_statistic, {v:NULL}   , {v:NULL}, NULL               , } ,
    {"directory/tries"  ,15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_dir.tries  , } ,
    {"directory/hits"   ,15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_dir.hits   , } ,
    {"directory/added"  ,15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_dir.adds   , } ,
    {"directory/expired",15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_dir.expires, } ,
    {"directory/deleted",15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_dir.deletes, } ,
} ;

struct device d_stats_cache = { "cache", "cache", 0, NFT(stats_cache), stats_cache } ;
    // Note, the store hit rate and deletions are not shown -- too much information!

struct aggregate Aread = { 3 , ag_numbers, ag_separate, } ;
struct filetype stats_read[] = {
    {"calls"           , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_calls       , } ,
    {"cachesuccess"    , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_cache       , } ,
    {"cachebytes"      , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_cachebytes  , } ,
    {"success"         , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_success     , } ,
    {"bytes"           , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_bytes       , } ,
    {"tries"           , 15, &Aread, ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, read_tries         , } ,
}
 ;
struct device d_stats_read = { "read", "read", 0, NFT(stats_read), stats_read } ;

struct filetype stats_write[] = {
    {"calls"           , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & write_calls      , } ,
    {"success"         , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & write_success    , } ,
    {"bytes"           , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & write_bytes      , } ,
    {"tries"           , 15, &Aread, ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, write_tries        , } ,
}
 ;
struct device d_stats_write = { "write", "write", 0, NFT(stats_write), stats_write } ;

struct filetype stats_directory[] = {
    {"maxdepth"        , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & dir_depth        , } ,
    {"bus"             ,  0, NULL  , ft_subdir  , ft_statistic, {v:NULL}   , {v:NULL}, NULL               , } ,
    {"bus/calls"       , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & dir_main.calls   , } ,
    {"bus/entries"     , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & dir_main.entries , } ,
    {"device"          ,  0, NULL  , ft_subdir  , ft_statistic, {v:NULL}   , {v:NULL}, NULL               , } ,
    {"device/calls"    , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & dir_dev.calls    , } ,
    {"device/entries"  , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & dir_dev.entries  , } ,
}
 ;
struct device d_stats_directory = { "directory", "directory", 0, NFT(stats_directory), stats_directory } ;

struct filetype stats_thread[] = {
    {"multithreading"  , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & multithreading   , } ,
    {"device_slots"    , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & maxslots         , } ,
    {"directory"       ,  0, NULL  , ft_subdir  , ft_statistic, {v:NULL}   , {v:NULL}, NULL               , } ,
    {"directory/now"   , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & dir_avg.current  , } ,
    {"directory/sum"   , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & dir_avg.sum      , } ,
    {"directory/num"   , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & dir_avg.count    , } ,
    {"directory/max"   , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & dir_avg.max      , } ,
    {"overall"         ,  0, NULL  , ft_subdir  , ft_statistic, {v:NULL}   , {v:NULL}, NULL               , } ,
    {"overall/now"     , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & all_avg.current  , } ,
    {"overall/sum"     , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & all_avg.sum      , } ,
    {"overall/num"     , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & all_avg.count    , } ,
    {"overall/max"     , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & all_avg.max      , } ,
    {"read"            ,  0, NULL  , ft_subdir  , ft_statistic, {v:NULL}   , {v:NULL}, NULL               , } ,
    {"read/now"        , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_avg.current , } ,
    {"read/sum"        , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_avg.sum     , } ,
    {"read/num"        , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_avg.count   , } ,
    {"read/max"        , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_avg.max     , } ,
    {"write"           ,  0, NULL  , ft_subdir  , ft_statistic, {v:NULL}   , {v:NULL}, NULL               , } ,
    {"write/now"       , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & write_avg.current, } ,
    {"write/sum"       , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & write_avg.sum    , } ,
    {"write/num"       , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & write_avg.count  , } ,
    {"write/max"       , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & write_avg.max    , } ,
}
 ;
struct device d_stats_thread = { "threads", "threads", 0, NFT(stats_thread), stats_thread } ;

struct filetype stats_bus[] = {
    {"elapsed_time"    , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_elapsed}, {v:NULL}, NULL            , } ,
    {"bus_time"        , 12, NULL  , ft_float, ft_statistic, {f:FS_time}, {v:NULL}, & bus_time         , } ,
    {"pause_time"      , 12, NULL  , ft_float, ft_statistic, {f:FS_time}, {v:NULL}, & bus_pause        , } ,
    {"locks"           , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & bus_locks        , } ,
    {"unlocks"         , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & bus_unlocks      , } ,
    {"CRC8_tries"      , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & crc8_tries       , } ,
    {"CRC8_errors"     , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & crc8_errors      , } ,
    {"CRC16_tries"     , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & crc16_tries      , } ,
    {"CRC16_errors"    , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & crc16_errors     , } ,
    {"read_timeout"    , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_timeout     , } ,
}
 ;
struct device d_stats_bus = { "bus", "bus", 0, NFT(stats_bus), stats_bus } ;


/* ------- Functions ------------ */

static int FS_stat(unsigned int * u , const struct parsedname * pn) {
    int dindex = pn->extension ;
    if (dindex<0) dindex = 0 ;
    if (pn->ft->data == NULL) return -ENOENT ;
    STATLOCK
        u[0] =  ((unsigned int *)pn->ft->data)[dindex] ;
    STATUNLOCK
    return 0 ;
}

static int FS_time(FLOAT *u , const struct parsedname * pn) {
    int dindex = pn->extension ;
    struct timeval * tv = (struct timeval *) pn->ft->data ;
    FLOAT f;
    if (dindex<0) dindex = 0 ;
    if (tv == NULL) return -ENOENT ;

    STATLOCK /* to prevent simultaneous changes to bus timing variables */
    f = (FLOAT)tv[dindex].tv_sec + ((FLOAT)(tv[dindex].tv_usec/1000))/1000.0;
    STATUNLOCK
//printf("FS_time sec=%ld usec=%ld f=%7.3f\n",tv[dindex].tv_sec,tv[dindex].tv_usec, f) ;
    u[0] = f;
    return 0 ;
}

static int FS_elapsed(unsigned int * u , const struct parsedname * pn) {
//printf("ELAPSE start=%u, now=%u, diff=%u\n",start_time,time(NULL),time(NULL)-start_time) ;
    (void) pn ;
    STATLOCK
        u[0] = time(NULL)-start_time ;
    STATUNLOCK
    return 0 ;
}
