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

#include "owfs_config.h"
#include "ow.h"
//#include <sys/uio.h>
#include <netinet/in.h>
#include <netdb.h> /* addrinfo */

static int ConnectFD( struct addrinfo * ai) ;
static int PortToFD(  char * port ) ;
static int FromServer( int fd, struct client_msg * cm, char * msg, int size ) ;
static void * FromServerAlloc( int fd, struct client_msg * cm ) ;
static int ToServer( int fd, struct server_msg * sm, const char * path, const char * data, int datasize ) ;
static int OpenServer( void ) ;

int Server_detect( void ) {
    int ret = OpenServer() ;
    if ( ret==0) busmode = bus_remote ;
    return ret ;
}

static int OpenServer( void ) {
    if ( servername ) {
        connectfd = PortToFD( servername ) ;
    }
    return (connectfd>-1) ;
}

void CloseServer( void ) {
    if ( connectfd > -1 ) {
        close(connectfd) ;
        connectfd = -1 ;
    }
}


int ServerRead( const char * path, char * buf, const size_t size, const off_t offset ) {
    struct server_msg sm ;
    struct client_msg cm ;
    void * r ;
    int ret ;

    sm.type = msg_read ;
    sm.size = size ;
    sm.tempscale = tempscale ;
    sm.offset = offset ;
    ret = ToServer( connectfd, &sm, path, NULL, 0) ;
    if (ret) return ret ;
    ret = FromServer( connectfd, &cm, buf, size ) ;
    if (ret) return ret ;
    return cm.ret ;
}

int ServerWrite( const char * path, const char * buf, const size_t size, const off_t offset ) {
    struct server_msg sm ;
    struct client_msg cm ;
    int ret ;

    sm.type = msg_write ;
    sm.size = size ;
    sm.tempscale = tempscale ;
    sm.offset = offset ;
    ret = ToServer( connectfd, &sm, path, NULL, 0) ;
    if (ret) return ret ;
    ret = FromServer( connectfd, &cm, NULL, 0 ) ;
    if (ret) return ret ;
    return cm.ret ;
}

/* Read "n" bytes from a descriptor. */
/* Stolen from Unix Network Programming by Stevens, Fenner, Rudoff p89 */
ssize_t readn(int fd, void *vptr, size_t n) {
    size_t	nleft;
    ssize_t	nread;
    char	*ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0; /* and call read() again */
            else
                return(-1);
        } else if (nread == 0)
            break; /* EOF */
        nleft -= nread ;
        ptr   += nread;
    }
    return(n - nleft); /* return >= 0 */
}

/* read from server, free return pointer if not Null */
static void * FromServerAlloc( int fd, struct client_msg * cm ) {
    char * msg ;

    if ( readn(fd, cm, sizeof(struct client_msg) ) != sizeof(struct client_msg) ) {
        cm->size = 0 ;
        cm->ret = -EIO ;
        return NULL ;
    }
    cm->payload = ntohl(cm->payload) ;
    cm->size = ntohl(cm->size) ;
    cm->ret = ntohl(cm->ret) ;
    cm->format = ntohl(cm->format) ;
    cm->offset = ntohl(cm->offset) ;

printf("FromServer payload=%d size=%d ret=%d format=%d offset=%d\n",cm->payload,cm->size,cm->ret,cm->format,cm->offset);
    if ( cm->size == 0 ) return NULL ;

    if ( (msg=malloc(cm->size)) ) {
        if ( readn(fd,msg,cm->size) != cm->size ) {
            cm->size = 0 ;
            cm->ret = -EIO ;
            free(msg);
            msg = NULL ;
        }
    }
    return msg ;
}
/* Read from server -- return negative on error,
    return 0 or positive giving size of data element */
static int FromServer( int fd, struct client_msg * cm, char * msg, int size ) {
    int rtry ;
    int d ;

    if ( readn(fd, cm, sizeof(struct client_msg) ) != sizeof(struct client_msg) ) {
        cm->size = 0 ;
        cm->ret = -EIO ;
        return -1 ;
    }

    cm->payload = ntohl(cm->payload) ;
    cm->size = ntohl(cm->size) ;
    cm->ret = ntohl(cm->ret) ;
    cm->format = ntohl(cm->format) ;
    cm->offset = ntohl(cm->offset) ;

printf("FromServer payload=%d size=%d ret=%d format=%d offset=%d\n",cm->payload,cm->size,cm->ret,cm->format,cm->offset);
    if ( cm->payload == 0 ) return cm->payload ;

    d = cm->payload - size ;
    rtry = d<0 ? cm->payload : size ;
    if ( readn(fd, msg, rtry ) != rtry ) {
        cm->ret = -EIO ;
        return -1 ;
    }

    if ( d>0 ) {
        char extra[d] ;
        if ( readn(fd,extra,d) != d ) {
            cm->ret = -EIO ;
            return -1 ;
        }
        return size ;
    }
    return cm->payload ;
}

static int ToServer( int fd, struct server_msg * sm, const char * path, const char * data, int datasize ) {
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

printf("ToServer payload=%d size=%d type=%d tempscale=%d offset=%d\n",payload,sm->size,sm->type,sm->tempscale,sm->offset);

    sm->payload   = htonl(payload)       ;
    sm->size      = htonl(sm->size)      ;
    sm->type      = htonl(sm->type)      ;
    sm->tempscale = htonl(sm->tempscale) ;
    sm->offset    = htonl(sm->offset)    ;

    return writev( fd, io, nio ) != payload + sizeof(struct server_msg) ;
}

static int PortToFD(  char * port ) {
    char * host ;
    char * serv ;
    struct addrinfo hint ;
    struct addrinfo * ai ;
    int matches = 0 ;
    int ret = -1;

    if ( port == NULL ) return -1 ;
    if ( (serv=strrchr(port,':')) ) { /* : exists */
        *serv = '\0' ;
        ++serv ;
        host = port ;
    } else {
        host = NULL ;
        serv = port ;
    }

    bzero( &hint, sizeof(struct addrinfo) ) ;
    hint.ai_flags = AI_PASSIVE ;
    hint.ai_socktype = SOCK_STREAM ;
    hint.ai_family = AF_UNSPEC ;

    for ( matches=0 ; matches<1 ; ++matches ) {
        if ( (ret=getaddrinfo( host, serv, &hint, &ai )) )
            break ;
    }

    if (matches) {
        ret = ConnectFD(ai) ;
        freeaddrinfo(ai) ;
    }

    return ret ;
}

static int ConnectFD( struct addrinfo * ai) {
    int fd ;

    if ( (fd=socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol))<0 ) {
        fprintf(stderr,"Socket problem errno=%d\n",errno) ;
        return -1 ;
    }
    
    if ( connect(fd, ai->ai_addr, ai->ai_addrlen) ) { /* Arbitrary "backlog" parameter */
        fprintf(stderr,"Connect problem. errno=%d\n",errno) ;
        return -1 ;
    }
    
    return fd ;
}


