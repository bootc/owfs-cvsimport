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

#include <sys/stat.h>
#include <string.h>

#include "owfs_config.h"
#include "ow.h"

/* ------- Prototypes ----------- */
static int FS_real_write(const char * const path, const char * const buf, const size_t size, const off_t offset , const struct parsedname * pn) ;
static int FS_w_all(const char * const buf, const size_t size, const off_t offset , const struct parsedname * const pn) ;
static int FS_w_split(const char * const buf, const size_t size, const off_t offset , const struct parsedname * const pn) ;
static int FS_parse_write(const char * const buf, const size_t size, const off_t offset , const struct parsedname * const pn) ;

static int FS_input_yesno( int * const result, const char * const buf, const size_t size ) ;
static int FS_input_integer( int * const result, const char * const buf, const size_t size ) ;
static int FS_input_unsigned( unsigned int * const result, const char * const buf, const size_t size ) ;
static int FS_input_float( float * const result, const char * const buf, const size_t size ) ;
static int FS_input_yesno_array( int * const results, const char * const buf, const size_t size, const struct parsedname * const pn ) ;
static int FS_input_unsigned_array( unsigned int * const results, const char * const buf, const size_t size, const struct parsedname * const pn ) ;
static int FS_input_integer_array( int * const results, const char * const buf, const size_t size, const struct parsedname * const pn ) ;
static int FS_input_float_array( float * const results, const char * const buf, const size_t size, const struct parsedname * const pn ) ;

/* ---------------------------------------------- */
/* Filesystem callback functions                  */
/* ---------------------------------------------- */
int FS_write(const char *path, const char *buf, const size_t size, const off_t offset) {
    struct parsedname pn ;
    int r ;
//printf("WRITE\n");
    if ( FS_ParsedName( path , &pn ) ) {
         FS_ParsedName_destroy(&pn) ;
        return -ENOENT;
    } else if ( pn.dev==NULL || pn.ft == NULL ) {
        FS_ParsedName_destroy(&pn) ;
        return -EISDIR ;
    }

    ++ write_calls ; /* statistics */
    r = FS_real_write( path, buf, size, offset, &pn ) ;

#ifdef OW_CACHE
    if ( offset  || r ) {
        Cache_Del( path ) ;
    } else {
//printf("Write adding %s\n",path) ;
        Cache_Add( path, size, buf, pn.ft->change ) ;
    }
#endif /* OW_CACHE */
    if ( r == 0 ) {
        ++write_success ; /* statistics */
        write_bytes += size ; /* statistics */
    }
    FS_ParsedName_destroy(&pn) ;
    return r ;
}

static int FS_real_write(const char * const path, const char * const buf, const size_t size, const off_t offset, const struct parsedname * pn) {
    int r ;
//printf("REAL_WRITE\n");

    /* Writable? */
    if ( (pn->ft->write.v) == NULL ) return -ENOTSUP ;

    /* Do we exist? Only test static cases */
    if ( presencecheck && pn->ft->change==ft_static && CheckPresence(pn) ) return -ENOENT ;

    /* Array properties? Write all together if aggregate */
    if ( pn->ft->ag && pn->extension==-1 && pn->ft->ag->combined==ag_separate ) return FS_w_all(buf,size,offset,pn) ;
    if ( pn->ft->ag && pn->extension>=0 && pn->ft->ag->combined==ag_aggregate ) return FS_w_split(buf,size,offset,pn) ;

    /* Norml write. Triplicate attempt */
    ++ write_tries[0] ; /* statistics */
    if ( (r=FS_parse_write( buf, size, offset, pn ))>=0  ) return r;
    ++ write_tries[1] ; /* statistics */
    if ( (r=FS_parse_write( buf, size, offset, pn ))>=0  ) return r;
    ++ write_tries[2] ; /* statistics */
    r = FS_parse_write( buf, size, offset, pn ) ;
    if (r<0) syslog(LOG_INFO,"Write error on %s (size=%d)\n",path,size) ;
    return r ;
}

static int FS_parse_write(const char * const buf, const size_t size, const off_t offset , const struct parsedname * const pn) {
    size_t elements = 1 ;
    int ret ;
    /* We will allocate memory for array variables off heap, but not single vars for efficiency */
//printf("PARSE_WRITE\n");
    if ( pn->ft->ag && pn->ft->ag->combined==ag_aggregate ) elements = pn->ft->ag->elements ;                                                                switch( pn->ft->format ) {
    case ft_integer: {
        int I ;
        int * i = &I ;
        if ( offset ) return -EINVAL ;
        if ( elements>1 ) {
            i = (int *) calloc( elements , sizeof(int) ) ;
            if ( i==NULL ) return -ENOMEM ;
            ret = FS_input_integer_array( i, buf, size, pn ) ;
        } else {
            ret = FS_input_integer( i, buf, size ) ;
        }
        ret |= (pn->ft->write.i)(i,pn) ;
        if ( elements>1) free(i) ;
        return ret ;
    }
    case ft_unsigned: {
        unsigned int U ;
        unsigned int * u = &U ;
        if ( offset ) return -EINVAL ;
        if ( elements>1 ) {
            u = (unsigned int *) calloc( elements , sizeof(unsigned int) ) ;
            if ( u==NULL ) return -ENOMEM ;
            ret = FS_input_unsigned_array( u, buf, size, pn ) ;
        } else {
            ret = FS_input_unsigned( u, buf, size ) ;
        }
        ret |= (pn->ft->write.u)(u,pn) ;
        if ( elements>1) free(u) ;
        return ret ;
    }
    case ft_float: {
        float F ;
        float * f = &F ;
        if ( offset ) return -EINVAL ;
        if ( elements>1 ) {
            f = (float *) calloc( elements , sizeof(float) ) ;
            if ( f==NULL ) return -ENOMEM ;
            ret = FS_input_float_array( f, buf, size, pn ) ;
        } else {
            ret = FS_input_float( f, buf, size ) ;
        }
        ret |= (pn->ft->write.f)(f,pn) ;
        if ( elements>1) free(f) ;
        return ret ;
    }
    case ft_yesno: {
        int Y ;
        int * y = &Y ;
        if ( offset ) return -EINVAL ;
        if ( elements>1 ) {
            y = (int *) calloc( elements , sizeof(int) ) ;
            if ( y==NULL ) return -ENOMEM ;
            ret = FS_input_yesno_array( y, buf, size, pn ) ;
        } else {
            ret = FS_input_yesno( y, buf, size ) ;
        }
        ret |= (pn->ft->write.y)(y,pn) ;
        if ( elements>1) free(y) ;
        return ret ;
    }
    case ft_ascii:
        return (pn->ft->write.a)(buf,size,offset,pn) ;
    case ft_binary:
        return (pn->ft->write.b)(buf,size,offset,pn) ;
    case ft_directory:
        return -ENOSYS ;
    }
    return -EINVAL ; /* unknown data type */
}

/* Non-combined input  field, so treat  as serveral separate tranactions */
static int FS_w_all(const char * const buf, const size_t size, const off_t offset , const struct parsedname * const pn) {
    size_t left = size ;
    const char * p = buf ;
    int r ;
    struct parsedname pname ;
//printf("WRITE_ALL\n");

    ++ write_array ; /* statistics */
    memcpy( &pname , pn , sizeof(struct parsedname) ) ;
//printf("WRITEALL(%p) %s\n",p,path) ;
    if ( offset ) return -ERANGE ;

    if ( pname.ft->format==ft_binary ) {
        int suglen = pname.ft->suglen ;
        for ( pname.extension=0 ; pname.extension < pname.ft->ag->elements ; ++pname.extension ) {
            if ( (int) left < suglen ) return -ERANGE ;
            if ( (r=FS_parse_write(p,(size_t) suglen,0,&pname)) < 0 ) return r ;
            p += suglen ;
            left -= suglen ;
        }
    } else {
        for ( pname.extension=0 ; pname.extension < pname.ft->ag->elements ; ++pname.extension ) {
            char * c = memchr( p , ',' , left ) ;
            if ( c==NULL ) {
                if ( (r=FS_parse_write(p,left,0,&pname)) < 0 ) return r ;
                p = buf + size ;
                left = 0 ;
            } else {
                if ( (r=FS_parse_write(p,(size_t)(c-p),0,&pname)) < 0 ) return r ;
                p = c + 1 ;
                left = size - (buf-p) ;
            }
        }
    }
    ++write_arraysuccess ; /* statistics */
    return 0 ;
}

/* Combined field, so read all, change the relevant field, and write back */
static int FS_w_split(const char * const buf, const size_t size, const off_t offset , const struct parsedname * const pn) {
    size_t elements = pn->ft->ag->elements ;
    int ret ;
//printf("WRITE_SPLIT\n");

    ++ write_aggregate ; /* statistics */

    (void) offset ;

    switch( pn->ft->format ) {
    case ft_yesno: {
        int * y = (int *) calloc( elements , sizeof(int) ) ;
            if ( y==NULL ) return -ENOMEM ;
            ret = ((pn->ft->read.y)(y,pn)<0) || FS_input_yesno(&y[pn->extension],buf,size) || (pn->ft->write.y)(y,pn)  ;
        free( y ) ;
        break ;
        }
    case ft_integer: {
        int * i = (int *) calloc( elements , sizeof(int) ) ;
            if ( i==NULL ) return -ENOMEM ;
            ret = ((pn->ft->read.i)(i,pn)<0) || FS_input_integer(&i[pn->extension],buf,size) || (pn->ft->write.i)(i,pn) ;
        free( i ) ;
        break ;
        }
    case ft_unsigned: {
        int * u = (unsigned int *) calloc( elements , sizeof(unsigned int) ) ;
            if ( u==NULL ) return -ENOMEM ;
            ret = ((pn->ft->read.u)(u,pn)<0) || FS_input_unsigned(&u[pn->extension],buf,size) || (pn->ft->write.u)(u,pn) ;
        free( u ) ;
        break ;
        }
    case ft_float: {
        float * f = (float *) calloc( elements , sizeof(float) ) ;
            if ( f==NULL ) return -ENOMEM ;
            ret = ((pn->ft->read.f)(f,pn)<0) || FS_input_float(&f[pn->extension],buf,size) || (pn->ft->write.f)(f,pn) ;
        free(f) ;
        break ;
        }
    case ft_binary: {
        int suglen = pn->ft->suglen ;
        if( size != (size_t) suglen ) {
            return -EMSGSIZE ;
        } else {
            unsigned char * all = (unsigned char *) malloc( suglen*elements ) ;
                if ( all==NULL ) return -ENOMEM ;
                memcpy(&all[suglen*pn->extension],buf,(size_t)suglen) ;
                ret = (pn->ft->write.b)(all,suglen*elements,0,pn) ;
            free( all ) ;
        }
        break ;
        }
    case ft_ascii: {
        int suglen = pn->ft->suglen ;
        if( size != (size_t) suglen ) {
            return -EMSGSIZE ;
        } else {
            char * all = (char *) malloc((suglen+1)*elements-1) ;
                if ( all==NULL ) return -ENOMEM ;
                memcpy(&all[(suglen+1)*pn->extension],buf,(size_t)suglen) ;
                ret = (pn->ft->write.a)(all,suglen*elements,0,pn) ;
            free( all ) ;
        }
        break ;
    case ft_directory:
        return -ENOSYS ;
        }
    }
    if ( ret ) return -EINVAL ;
    ++write_aggregatesuccess ; /* statistics */
    return 0 ;
}

static int FS_input_yesno( int * const result, const char * const buf, const size_t size ) {
//printf("yesno size=%d, buf=%s\n",size,buf);
    if ( size ) {
        if ( buf[0]=='1' || strncasecmp("on",buf,2)==0 || strncasecmp("yes",buf,2)==0 ) {
            *result = 1 ;
//printf("YESno\n");
            return 0 ;
        }
        if ( buf[0]=='0' || strncasecmp("off",buf,2)==0 || strncasecmp("no",buf,2)==0 ) {
            *result = 0 ;
//printf("yesNO\n") ;
            return 0 ;
        }
    }
    return 1 ;
}

static int FS_input_integer( int * const result, const char * const buf, const size_t size ) {
    char cp[size+1] ;
    char * end ;

    memcpy( cp, buf, size ) ;
    cp[size] = '\0' ;
    errno = 0 ;
    * result = strtol( cp,&end,10) ;
    return end==cp || errno ;
}

static int FS_input_unsigned( unsigned int * const result, const char * const buf, const size_t size ) {
    char cp[size+1] ;
    char * end ;

    memcpy( cp, buf, size ) ;
    cp[size] = '\0' ;
    errno = 0 ;
    * result = strtoul( cp,&end,10) ;
//printf("UI str=%s, val=%u\n",cp,*result) ;
    return end==cp || errno ;
}

static int FS_input_float( float * const result, const char * const buf, const size_t size ) {
    char cp[size+1] ;
    char * end ;

    memcpy( cp, buf, size ) ;
    cp[size] = '\0' ;
    errno = 0 ;
    * result = strtod( cp,&end) ;
    return end==cp || errno ;
}

/* returns number of valid integers, or negative for error */
static int FS_input_yesno_array( int * const results, const char * const buf, const size_t size, const struct parsedname * const pn ) {
    int i ;
    size_t left = size ;
    const char * first = buf ;
    const char * next = buf ;
    for ( i=0 ; i<pn->ft->ag->elements - 1 ; ++i ) {
        if ( (next=memchr( first, ',' , left )) == NULL ) return -(i>0) ;
        if ( FS_input_yesno( &results[i], first, next-first ) ) results[i]=0 ;
//printf("IYA index=%d, result=%d, first=%s\n",i,results[i],first) ;
        left -= (next-first) ;
        if ( left < 2 ) return 0 ;
        -- left ;
        first = next + 1 ;
    }
    /* i==elements now */
    if ( FS_input_yesno( &results[i], first, next-first ) ) results[i]=0 ;
//printf("IYA index=%d, result=%d, first=%s\n",i,results[i],first) ;
    return 0 ;
}

/* returns number of valid integers, or negative for error */
static int FS_input_integer_array( int * const results, const char * const buf, const size_t size, const struct parsedname * const pn ) {
    int i ;
    size_t left = size ;
    const char * first = buf ;
    const char * next ;
    for ( i=0 ; i<pn->ft->ag->elements - 1 ; ++i ) {
        if ( (next=memchr( first, ',' , left )) == NULL ) return -(i>0) ;
        if ( FS_input_integer( &results[i], first, next-first ) ) results[i]=0 ;
        left -= (next-first) ;
        if ( left < 2 ) return 0 ;
        -- left ;
        first = next + 1 ;
    }
    /* i==elements now */
    if ( FS_input_integer( &results[i], first, left ) ) results[i]=0 ;
    return 0 ;
}

/* returns 0, or negative for error */
static int FS_input_unsigned_array( unsigned int * const results, const char * const buf, const size_t size, const struct parsedname * const pn ) {
    int i ;
    size_t left = size ;
    const char * first = buf ;
    const char * next ;
    for ( i=0 ; i<pn->ft->ag->elements - 1 ; ++i ) {
        if ( (next=memchr( first, ',' , left )) == NULL ) return -(i>0) ;
//printf("UIA index %d\n",i) ;
        if ( FS_input_unsigned( &results[i], first, next-first ) ) results[i]=0 ;
//printf("UIA value= %u\n",results[i]) ;
        left -= (next-first) ;
        if ( left < 2 ) return 0 ;
        -- left ;
        first = next + 1 ;
    }
    /* i==elements now */
//printf("UIA index %d\n",i) ;
    if ( FS_input_unsigned( &results[i], first, left ) ) results[i]=0 ;
//printf("UIA value= %u\n",results[i]) ;
//printf("UIA done\n") ;
    return 0 ;
}

/* returns 0, or negative for error */
static int FS_input_float_array( float * const results, const char * const buf, const size_t size, const struct parsedname * const pn ) {
    int i ;
    size_t left = size ;
    const char * first = buf ;
    const char * next ;
    for ( i=0 ; i<pn->ft->ag->elements - 1 ; ++i ) {
        if ( (next=memchr( first, ',' , left )) == NULL ) return -(i>0) ;
        if ( FS_input_float( &results[i], first, next-first ) ) results[i]=0. ;
        left -= (next-first) ;
        if ( left < 2 ) return 0 ;
        -- left ;
        first = next + 1 ;
    }
    /* i==elements now */
    if ( FS_input_float( &results[i], first, left ) ) results[i]=0. ;
    return 0 ;
}
