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

/* ow_server talks to the server, sending and recieving messages */
/* this is an alternative to direct bus communication */

#include <config.h>
#include "owfs_config.h"
#include "ow.h"
#include "ow_connection.h"

static int FromServer( int fd, struct client_msg * cm, char * msg, size_t size ) ;
static void * FromServerAlloc( int fd, struct client_msg * cm ) ;
static int ToServer( int fd, struct server_msg * sm, char * path, char * data, size_t datasize ) ;
static void Server_setroutines( struct interface_routines * f ) ;
static void Server_close( struct connection_in * in ) ;
static uint32_t SetupSemi( const struct parsedname * pn ) ;

struct timeval tv = { 2, 0, } ;

static void Server_setroutines( struct interface_routines * f ) {
    f->detect        = Server_detect ;
//    f->reset         =;
//    f->next_both  ;
//    f->overdrive = ;
//    f->testoverdrive = ;
//    f->PowerByte     = ;
//    f->ProgramPulse = ;
//    f->sendback_data = ;
//    f->sendback_bits = ;
//    f->select        = ;
    f->reconnect     = NULL          ;
    f->close         = Server_close  ;
}

int Server_detect( struct connection_in * in ) {
    if ( in->name == NULL ) return -1 ;
    if ( ClientAddr( in->name, in ) ) return -1 ;
    in->Adapter = adapter_tcp ;
    in->adapter_name = "tcp" ;
    in->busmode = bus_remote ;
    Server_setroutines( & (in->iroutines) ) ;
    return 0 ;
}

static void Server_close( struct connection_in * in ) {
    FreeClientAddr( in ) ;
}

int ServerRead( char * buf, const size_t size, const off_t offset, const struct parsedname * pn ) {
    struct server_msg sm ;
    struct client_msg cm ;
    int connectfd = ClientConnect( pn->in ) ;
    int ret = 0 ;

    if ( connectfd < 0 ) return -EIO ;
    //printf("ServerRead pn->path=%s, size=%d, offset=%u\n",pn->path,size,offset);
    memset(&sm, 0, sizeof(struct server_msg));
    sm.type = msg_read ;
    sm.size = size ;
    sm.sg = SetupSemi(pn) ;
    sm.offset = offset ;

    //printf("ServerRead path=%s\n", pn->path_busless);
    LEVEL_CALL("SERVER(%d)READ path=%s\n", pn->in->index, SAFESTRING(pn->path_busless));

    if ( ToServer( connectfd, &sm, pn->path_busless, NULL, 0) ) {
        ret = -EIO ;
    } else if ( FromServer( connectfd, &cm, buf, size ) < 0 ) {
        ret = -EIO ;
    } else {
        ret = cm.ret ;
    }
    close( connectfd ) ;
    return ret ;
}

int ServerPresence( const struct parsedname * pn ) {
    struct server_msg sm ;
    struct client_msg cm ;
    int connectfd = ClientConnect( pn->in ) ;
    int ret = 0 ;

    if ( connectfd < 0 ) return -EIO ;
    //printf("ServerPresence pn->path=%s\n",pn->path);
    memset(&sm, 0, sizeof(struct server_msg));
    sm.type = msg_presence ;

    sm.sg =  SetupSemi(pn) ;

    //printf("ServerPresence path=%s\n", pn->path_busless);
    LEVEL_CALL("SERVER(%d)PRESENCE path=%s\n", pn->in->index, SAFESTRING(pn->path_busless));

    if ( ToServer( connectfd, &sm, pn->path_busless, NULL, 0) ) {
        ret = -EIO ;
    } else if ( FromServer( connectfd, &cm, NULL, 0 ) < 0 ) {
        ret = -EIO ;
    } else {
        ret = cm.ret ;
    }
    close( connectfd ) ;
    return ret ;
}

int ServerWrite( const char * buf, const size_t size, const off_t offset, const struct parsedname * pn ) {
    struct server_msg sm ;
    struct client_msg cm ;
    int connectfd = ClientConnect( pn->in ) ;
    int ret = 0 ;

    if ( connectfd < 0 ) return -EIO ;
    //printf("ServerWrite path=%s, buf=%*s, size=%d, offset=%d\n",path,size,buf,size,offset);
    memset(&sm, 0, sizeof(struct server_msg));
    sm.type = msg_write ;
    sm.size = size ;
    sm.sg =  SetupSemi(pn) ;
    sm.offset = offset ;

    //printf("ServerRead path=%s\n", pn->path_busless);
    LEVEL_CALL("SERVER(%d)WRITE path=%s\n", pn->in->index, SAFESTRING(pn->path_busless));

    if ( ToServer( connectfd, &sm, pn->path_busless, buf, size) ) {
        ret = -EIO ;
    } else if ( FromServer( connectfd, &cm, NULL, 0 ) < 0 ) {
        ret = -EIO ;
    } else {
        ret = cm.ret ;
        if ( SemiGlobal != cm.sg ) {
            //printf("ServerRead: cm.sg changed!  SemiGlobal=%X cm.sg=%X\n", SemiGlobal, cm.sg);
            CACHELOCK;
                SemiGlobal = cm.sg & (~BUSRET_MASK) ;
            CACHEUNLOCK;
        }
    }
    close( connectfd ) ;
    return ret ;
}

int ServerDir( void (* dirfunc)(const struct parsedname * const), const struct parsedname * pn, uint32_t * flags ) {
    struct server_msg sm ;
    struct client_msg cm ;
    int connectfd = ClientConnect( pn->in ) ;

    if ( connectfd < 0 ) return -EIO ;

    memset(&sm, 0, sizeof(struct server_msg));
    sm.type = msg_dir ;

    sm.sg =  SetupSemi(pn) ;


    LEVEL_CALL("SERVER(%d)DIR path=%s\n", pn->in->index, SAFESTRING(pn->path_busless));

    if ( ToServer( connectfd, &sm, pn->path_busless, NULL, 0) ) {
        cm.ret = -EIO ;
    } else {
        char * path2 ;
        BYTE * snlist = NULL ;
        size_t devices = 0 ;
        size_t allocated = 0 ;
        struct parsedname pn2 ;

        /* If cacheable, try to allocate a blob for storage */
        /* only for "read devices" and not alarm */
        if ( IsRealDir(pn) && NotAlarmDir(pn) && !SpecifiedBus(pn) && pn->dev==NULL ) {
            if ( pn2.pathlength == 0 ) { /* root dir */
                BUSLOCK(pn) ;
                    allocated = pn->in->last_root_devs ; // root dir estimated length
                BUSUNLOCK(pn) ;
            }
            allocated += 10 ; /* add space for additional devices */
            snlist = (BYTE *) malloc(allocated*8+2) ; /* NULL ok, its well handled later */
        }

        while((path2 = FromServerAlloc( connectfd, &cm))) {
            path2[cm.payload-1] = '\0' ; /* Ensure trailing null */
            LEVEL_DEBUG("ServerDir: got=[%s]\n", path2);

            if ( FS_ParsedName_Remote( path2, &pn2 ) ) {
                cm.ret = -EINVAL ;
                free(path2) ;
                break ;
            }
            pn2.in = pn->in ; //use parent connection_in
            
            /* we got a device on bus_nr = pn->in->index. Cache it so we
                find it quicker next time we want to do read values from the
                the actual device
            */
            if( pn2.dev && IsRealDir(&pn2) ) {
                /* If we get a device then cache the bus_nr */
                Cache_Add_Device(pn->in->index, &pn2);
            }
            /* Add to cache Blob -- snlist is also a flag for cachable */
            if ( snlist ) { /* only add if there is a blob allocated successfully */
                if ( devices >= allocated ) {
                    void * temp = snlist ;
                    allocated += 10 ;
                    snlist = (BYTE*) realloc( temp, allocated*8+2 ) ;
                    if ( snlist==NULL ) free(temp) ;
                }
                if ( snlist ) { /* test again, after realloc */
                    memcpy( snlist + 8*devices, pn2.sn, 8 ) ;
                }
            }
            ++devices ;

            DIRLOCK;
                dirfunc(&pn2) ;
            DIRUNLOCK;

            FS_ParsedName_destroy( &pn2 ) ;  // destroy the last parsed name
            free(path2) ;
        }
        /* Add to the cache (full list as a single element */
        if ( snlist ) {
            Cache_Add_Dir(snlist,devices,pn) ;  // end with a null entry
            free(snlist) ;
            if ( pn2.pathlength == 0 ) {
                BUSLOCK(pn) ;
                    pn->in->last_root_devs = devices ; // root dir estimated length
                BUSUNLOCK(pn) ;
            }
        }

        DIRLOCK;
            /* flags are sent back in "offset" of final blank entry */
            flags[0] |= cm.offset ;
        DIRUNLOCK;
    }
    close( connectfd ) ;
    return cm.ret ;
}

/* read from server, free return pointer if not Null */
static void * FromServerAlloc( int fd, struct client_msg * cm ) {
    char * msg ;
    int ret;

    do { /* loop until non delay message (payload>=0) */
        //printf("OW_SERVER loop1\n");
        ret = readn(fd, cm, sizeof(struct client_msg), &tv );
        if ( ret != sizeof(struct client_msg) ) {
            memset(cm, 0, sizeof(struct client_msg)) ;
            cm->ret = -EIO ;
            return NULL ;
        }
        cm->payload = ntohl(cm->payload) ;
        cm->size = ntohl(cm->size) ;
        cm->ret = ntohl(cm->ret) ;
        cm->sg = ntohl(cm->sg) ;
        cm->offset = ntohl(cm->offset) ;
    } while ( cm->payload < 0 ) ;
    //printf("OW_SERVER loop1 done\n");

//printf("FromServerAlloc payload=%d size=%d ret=%d sg=%X offset=%d\n",cm->payload,cm->size,cm->ret,cm->sg,cm->offset);
//printf(">%.4d|%.4d\n",cm->ret,cm->payload);
    if ( cm->payload == 0 ) return NULL ;
    if ( cm->ret < 0 ) return NULL ;
    if ( cm->payload > 65000 ) {
//printf("FromServerAlloc payload too large\n");
        return NULL ;
    }

    if ( (msg=(char *)malloc((size_t)cm->payload)) ) {
        ret = readn(fd,msg,(size_t)(cm->payload), &tv );
        if ( ret != cm->payload ) {
//printf("FromServer couldn't read payload\n");
            cm->payload = 0 ;
            cm->offset = 0 ;
            cm->ret = -EIO ;
            free(msg);
            msg = NULL ;
        }
//printf("FromServer payload read ok\n");
    }
    return msg ;
}
/* Read from server -- return negative on error,
    return 0 or positive giving size of data element */
static int FromServer( int fd, struct client_msg * cm, char * msg, size_t size ) {
    size_t rtry ;
    size_t ret;

    do { // read regular header, or delay (delay when payload<0)
        //printf("OW_SERVER loop2\n");
        ret = readn(fd, cm, sizeof(struct client_msg), &tv );
        if ( ret != sizeof(struct client_msg) ) {
            //printf("OW_SERVER loop2 bad\n");
            cm->size = 0 ;
            cm->ret = -EIO ;
            return -EIO ;
        }

        cm->payload = ntohl(cm->payload) ;
        cm->size = ntohl(cm->size) ;
        cm->ret = ntohl(cm->ret) ;
        cm->sg = ntohl(cm->sg) ;
        cm->offset = ntohl(cm->offset) ;
    } while ( cm->payload < 0 ) ; // flag to show a delay message
    //printf("OW_SERVER loop2 done\n");

//printf("FromServer payload=%d size=%d ret=%d sg=%d offset=%d\n",cm->payload,cm->size,cm->ret,cm->sg,cm->offset);
//printf(">%.4d|%.4d\n",cm->ret,cm->payload);
    if ( cm->payload==0 ) return 0 ; // No payload, done.

    rtry = cm->payload<size ? cm->payload : size ;
    ret = readn(fd, msg, rtry, &tv ); // read expected payload now.
    if ( ret != rtry ) {
        cm->ret = -EIO ;
        return -EIO ;
    }

    if ( cm->payload > size ) { // Uh oh. payload bigger than expected. read it in and discard
        size_t d = cm->payload - size ;
        char extra[d] ;
        ret = readn(fd,extra,d,&tv);
        if ( ret != d ) {
            cm->ret = -EIO ;
            return -EIO ;
        }
        return size ;
    }
    return cm->payload ;
}

// should be const char * data but iovec has problems with const arguments
//static int ToServer( int fd, struct server_msg * sm, const char * path, const char * data, int datasize ) {
static int ToServer( int fd, struct server_msg * sm, char * path, char * data, size_t datasize ) {
    int nio = 1 ;
    int payload = 0 ;
    struct iovec io[] = {
        { sm, sizeof(struct server_msg), } ,
        { path, 0, } ,
        { data, datasize, } ,
    } ;
    if ( path ) {
        ++ nio ;
        io[1].iov_len = payload = strlen(path) + 1 ;
        if ( data && datasize ) {
            ++nio ;
            payload += datasize ;
        }
    }

//printf("ToServer payload=%d size=%d type=%d tempscale=%X offset=%d\n",payload,sm->size,sm->type,sm->sg,sm->offset);
//printf("<%.4d|%.4d\n",sm->type,payload);
    //printf("Scale=%s\n", TemperatureScaleName(SGTemperatureScale(sm->sg)));

    sm->payload = htonl(payload)       ;
    sm->size    = htonl(sm->size)      ;
    sm->type    = htonl(sm->type)      ;
    sm->sg      = htonl(sm->sg)        ;
    sm->offset  = htonl(sm->offset)    ;

    return writev( fd, io, nio ) != (payload + sizeof(struct server_msg)) ;
}

/* flag the sg for "virtual root" -- the remote bus was specifically requested */
static uint32_t SetupSemi( const struct parsedname * pn ) {
    uint32_t sg = pn->sg & (~BUSRET_MASK) ;
    if ( SpecifiedBus(pn) ) sg |= (1<<BUSRET_BIT) ;
    return sg ;
}
