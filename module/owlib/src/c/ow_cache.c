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

// 8/18/2004 -- applied Serg Oskin's correction
// 8/20/2004 -- changed everything, specifically no longer using db, tsearch instead!
#include "owfs_config.h"
#include "ow.h"

#ifdef OW_CACHE
#include <limits.h>
#ifndef __USE_GNU
 #define __USE_GNU
 #include <search.h>
 #undef __USE_GNU
#else /* __USE_GNU */
 #include <search.h>
#endif /* __USE_GNU */

/* Put the globals into a struct to declutter the namespace */
static struct {
    void * new_db ; // current cache database
    void * old_db ; // older cache database
    void * store  ; // persistent database
    time_t retired ; // start time of older
    time_t killed ; // deathtime of older
    time_t lifespan ; // lifetime of older
    unsigned int added ; // items added
} cache ;

struct tree_key {
    unsigned char sn[8] ;
    union {
        struct filetype * ft ;
        char * nm ;
    } p ;
    int extension ;
} ;

struct tree_node {
    struct tree_key tk ;
    time_t expires ;
    int dsize ;
} ;

/* Bad bad C library */
/* implementation of tfind, tsearch returns an opaque structure */
/* you have to know that the first element is a pointer to your data */
struct tree_opaque {
    struct tree_node * key ;
    void * other ;
} ;

#define TREE_DATA(tn)	( (unsigned char *)(tn) + sizeof(struct tree_node) )

static int Cache_Add_Common( struct tree_node * const tn ) ;
static int Cache_Add_Store( struct tree_node * const tn ) ;
static int Cache_Get_Common( void * data, size_t * dsize, time_t duration, const struct tree_node * const tn ) ;
static int Cache_Get_Store( void * data, size_t * dsize, time_t duration, const struct tree_node * const tn ) ;
static int Cache_Del_Common( const struct tree_node * tn ) ;
static int Cache_Del_Store( const struct tree_node * tn ) ;
static int tree_compare( const void * a , const void * b ) ;
static time_t TimeOut( const enum ft_change change ) ;

static int tree_compare( const void * a , const void * b ) {
    return memcmp( &((const struct tree_node *)a)->tk , &((const struct tree_node *)b)->tk , sizeof(struct tree_key) ) ;
}

static time_t TimeOut( const enum ft_change change ) {
    switch( change ) {
    case ft_second:
    case ft_persistent : /* arbitrary non-zero */
        return 1 ;
    case ft_volatile:
    case ft_Avolatile:
        return timeout.vol ;
    case ft_stable:
    case ft_Astable:
        return timeout.stable;
    default: /* static or statistic */
        return 0 ;
    }
}

/* debug routine -- shows a table */
/* Run it as twalk(dababase, tree_show ) */
/*
static void tree_show( const void *node, const VISIT which, const int depth ) {
    const struct tree_node * tn = *(struct tree_node * const *) node ;
    char b[26] ;
    (void) depth ;
    if (node) {
        switch(which){
        case leaf:
        case postorder:
	    if ( tn->tk.extension == -2 ) {
                printf("Node %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X name=%s length=%d start=%p data=%p expires=%s",
	        tn->tk.sn[0] ,
    	        tn->tk.sn[1] ,
        	tn->tk.sn[2] ,
        	tn->tk.sn[3] ,
        	tn->tk.sn[4] ,
        	tn->tk.sn[5] ,
        	tn->tk.sn[6] ,
        	tn->tk.sn[7] ,
        	tn->tk.p.nm ,
        	tn->dsize ,
		tn,TREE_DATA(tn),
        	ctime_r(&tn->expires,b)
        	) ;
	    } else {
                printf("Node %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X name=%s length=%d start=%p data=%p expires=%s",
	        tn->tk.sn[0] ,
    	        tn->tk.sn[1] ,
        	tn->tk.sn[2] ,
        	tn->tk.sn[3] ,
        	tn->tk.sn[4] ,
        	tn->tk.sn[5] ,
        	tn->tk.sn[6] ,
        	tn->tk.sn[7] ,
        	tn->tk.p.ft->name ,
        	tn->dsize ,
		tn,TREE_DATA(tn),
        	ctime_r(&tn->expires,b)
        	) ;
	    }
	    // fall through
        default:
	    break ;
        }
    } else {
        printf("Node empty\n") ;
    }
}
*/

char * Cache_Version( void ) {
    return "intrinsic tree routines" ;
}

    /* DB cache creation code */
    /* Note: done in single-threaded mode so locking not yet needed */
void Cache_Open( void ) {
    cache.new_db = NULL ;
    cache.old_db = NULL ;
    cache.store  = NULL ;
    cacheavailable = 1 ;
    cache.lifespan = TimeOut( ft_stable ) ;
    if (cache.lifespan>3600 ) cache.lifespan = 3600 ; /* 1 hour tops */
    cache.retired = time(NULL) ;
    cache.killed = cache.retired+cache.lifespan ;
}

    /* Note: done in single-threaded mode so locking not yet needed */
void Cache_Close( void ) {
    tdestroy( cache.new_db , free ) ;
    tdestroy( cache.old_db , free ) ;
    tdestroy( cache.store  , free ) ;
}

/* Add an item to the cache */
/* return 0 if good, 1 if not */
int Cache_Add( const void * data, const size_t datasize, const struct parsedname * const pn ) {
    if ( pn ) { // do check here to avoid needless processing
        time_t duration = TimeOut( pn->ft->change ) ;
        if ( duration > 0 ) {
            struct tree_node * tn = (struct tree_node *) malloc ( sizeof(struct tree_node) + datasize ) ;
            if ( tn ) {
                memcpy( tn->tk.sn , pn->sn , 8 ) ;
                tn->tk.p.ft = pn->ft ;
                tn->tk.extension = pn->extension ;
                tn->expires = duration + time(NULL) ;
                tn->dsize = datasize ;
                memcpy( TREE_DATA(tn) , data , datasize ) ;
//printf("ADD EXTERNAL name= %s (%d) size=%d \n",tn->tk.p.ft->name,tn->tk.extension,tn->dsize);
                switch (pn->ft->change) {
                case ft_persistent:
                    return Cache_Add_Store( tn ) ;
                default:
                    return Cache_Add_Common( tn ) ;
                }
            }
            return -ENOMEM ;
        }
    }
    return 0 ;
}

/* Add an item to the cache */
/* return 0 if good, 1 if not */
int Cache_Add_Internal( const void * data, const size_t datasize, const struct internal_prop * ip, const struct parsedname * const pn ) {
    if ( pn ) { // do check here to avoid needless processing
        time_t duration = TimeOut( ip->change ) ;
        if ( duration > 0 ) {
            struct tree_node * tn = (struct tree_node *) malloc ( sizeof(struct tree_node) + datasize ) ;
            if ( tn ) {
                memcpy( tn->tk.sn , pn->sn , 8 ) ;
                tn->tk.p.nm = ip->name ;
                tn->tk.extension = -2 ;
                tn->expires = duration + time(NULL) ;
                tn->dsize = datasize ;
                memcpy( TREE_DATA(tn) , data , datasize ) ;
//printf("ADD INTERNAL name= %s size=%d \n",tn->tk.p.nm,tn->dsize);
                switch (ip->change) {
                case ft_persistent:
                    return Cache_Add_Store( tn ) ;
                default:
                    return Cache_Add_Common( tn ) ;
                }
            }
            return -ENOMEM ;
        }
    }
    return 0 ;
}

/* Add an item to the cache */
/* retire the cache (flip) if too old, and start a new one (keep the old one for a while) */
/* return 0 if good, 1 if not */
static int Cache_Add_Common( struct tree_node * const tn ) {
    struct tree_opaque * opaque ;
    enum { no_add, yes_add, just_update } state = no_add ;
    void * flip = NULL ;
    CACHELOCK
        if  (cache.killed < time(NULL) ) { // old database has timed out
	    flip = cache.old_db ;
            /* Flip caches! old = new. New truncated, reset time and counters and flag */
            cache.old_db  = cache.new_db ;
            cache.new_db = NULL ;
            cache.added = 0 ;
            cache.retired = time(NULL) ;
            cache.killed = cache.retired + cache.lifespan ;
        }
        if ( (opaque=tsearch(tn,&cache.new_db,tree_compare)) ) {
//printf("CACHE ADD pointer=%p, key=%p\n",tn,opaque->key);
	    if ( tn!=opaque->key ) {
	        free(opaque->key);
	        opaque->key = tn ;
		state = just_update ;
	    } else {
		state = yes_add ;
	    }
	} else { // nothing found or added?!? free our memory segment
	    free(tn) ;
	}
    CACHEUNLOCK
    /* flipped old database is now out of circulation -- can be destroyed without a lock */
    if ( flip ) {
        tdestroy( flip, free ) ;
        STATLOCK
            ++ cache_flips ; /* statistics */
	    memcpy(&old_avg,&new_avg,sizeof(struct average)) ;
	    AVERAGE_CLEAR(&new_avg)
        STATUNLOCK
//printf("FLIP points to: %p\n",flip);
    }
    /* Added or updated, update statistics */
    switch (state) {
    case yes_add:
	STATLOCK
	    AVERAGE_IN(&new_avg)
    	    ++ cache_adds ; /* statistics */
	STATUNLOCK
	return 0 ;
    case just_update:
	STATLOCK
	    AVERAGE_MARK(&new_avg)
    	    ++ cache_adds ; /* statistics */
	STATUNLOCK
	return 0 ;
    default:
	return 1 ;
    }
}

/* Add an item to the cache */
/* retire the cache (flip) if too old, and start a new one (keep the old one for a while) */
/* return 0 if good, 1 if not */
static int Cache_Add_Store( struct tree_node * const tn ) {
    struct tree_opaque * opaque ;
    enum { no_add, yes_add, just_update } state = no_add ;
    STORELOCK
        if ( (opaque=tfind( tn , &cache.store, tree_compare ) ) ) {
            tdelete( opaque->key, &cache.store, tree_compare ) ;
            free( opaque->key ) ;
        }
        opaque = tsearch( (void *)tn, &cache.store, tree_compare ) ;
        if ( (opaque=tsearch(tn,&cache.store,tree_compare)) ) {
	    if ( tn!=opaque->key ) {
	        free(opaque->key);
	        opaque->key = tn ;
		state = just_update ;
	    } else {
		state = yes_add ;
	    }
	} else { // Problem adding node, so free the memory
	    free(tn) ;
	}
    STOREUNLOCK
    switch (state) {
    case yes_add:
	STATLOCK
	    AVERAGE_IN(&store_avg)
	STATUNLOCK
	return 0 ;
    case just_update:
	STATLOCK
	    AVERAGE_MARK(&store_avg)
	STATUNLOCK
	return 0 ;
    default:
	return 1 ;
    }
}

/* Look in caches, 0=found and valid, 1=not or uncachable in the first place */
int Cache_Get( void * data, size_t * dsize, const struct parsedname * const pn ) {
    if ( pn ) { // do check here to avoid needless processing
        time_t duration = TimeOut( pn->ft->change ) ;
        if ( duration > 0 ) {
            struct tree_node tn  ;
            memcpy( tn.tk.sn , pn->sn , 8 ) ;
            tn.tk.p.ft = pn->ft ;
            tn.tk.extension = pn->extension ;
            switch(pn->ft->change) {
            case ft_persistent:
                return Cache_Get_Store(data,dsize,duration,&tn) ;
            default:
                return Cache_Get_Common(data,dsize,duration,&tn) ;
            }
        }
    }
    return 1 ;
}

/* Look in caches, 0=found and valid, 1=not or uncachable in the first place */
int Cache_Get_Internal( void * data, size_t * dsize, const struct internal_prop * ip, const struct parsedname * const pn ) {
    if ( pn ) { // do check here to avoid needless processing
        time_t duration = TimeOut( ip->change ) ;
        if ( duration > 0 ) {
            struct tree_node tn  ;
            memcpy( tn.tk.sn , pn->sn , 8 ) ;
            tn.tk.p.nm = ip->name ;
            tn.tk.extension = -2 ;
            switch(ip->change) {
            case ft_persistent:
                return Cache_Get_Store(data,dsize,duration,&tn) ;
            default:
                return Cache_Get_Common(data,dsize,duration,&tn) ;
            }
        }
    }
    return 1 ;
}

/* Look in caches, 0=found and valid, 1=not or uncachable in the first place */
static int Cache_Get_Common( void * data, size_t * dsize, time_t duration, const struct tree_node * const tn ) {
    int ret = 1 ;
    time_t now = time(NULL) ;
    struct tree_opaque * opaque ;
//printf("CACHE GET 1\n");
    STATLOCK
        ++ cache_tries ; /* statistics */
    STATUNLOCK
    CACHELOCK
        if ( (opaque=tfind(tn,&cache.new_db,tree_compare)) 
	     || ( (cache.retired+duration>now) && (opaque=tfind(tn,&cache.old_db,tree_compare)) ) 
	   ) {
//printf("CACHE GET 2 opaque=%p tn=%p\n",opaque,opaque->key);
            if ( opaque->key->expires >= now ) {
//printf("CACHE GET 3 buffer size=%d stored size=%d\n",*dsize,opaque->key->dsize);
                if ( *dsize >= opaque->key->dsize ) {
//printf("CACHE GET 4\n");
                        *dsize = opaque->key->dsize ;
//printf("CACHE GET 4a\n");
//tree_show(opaque,leaf,0);
//printf("CACHE GET 5 size=%d\n",*dsize);
                        memcpy( data , TREE_DATA(opaque->key) , *dsize ) ;
//printf("CACHE GET 6\n");
                        ret = 0 ;
                        STATLOCK
                            ++ cache_hits ; /* statistics */
                        STATUNLOCK
//printf("CACHE GOT\n");
//twalk(cache.new_db,tree_show) ;
                }
            } else {
//char b[26];
//printf("GOT DEAD now:%s",ctime_r(&now,b)) ;
//printf("        then:%s",ctime_r(&opaque->key->expires,b)) ;
                STATLOCK
                    ++ cache_expired ; /* statistics */
                STATUNLOCK
            }
        } else {
            STATLOCK
                ++ cache_misses ; /* statistics */
            STATUNLOCK
        }
    CACHEUNLOCK
    return ret ;
}

/* Look in caches, 0=found and valid, 1=not or uncachable in the first place */
static int Cache_Get_Store( void * data, size_t * dsize, time_t duration, const struct tree_node * const tn ) {
    struct tree_opaque * opaque ;
    int ret = 1 ;
    (void) duration ;
    STORELOCK
        if ( (opaque=tfind(tn,&cache.store,tree_compare)) ) {
            if ( *dsize >= opaque->key->dsize ) {
                    *dsize = opaque->key->dsize ;
                    memcpy( data, TREE_DATA(opaque->key), *dsize ) ;
                    ret = 0 ;
            }
        }
    STOREUNLOCK
    return ret ;
}

int Cache_Del( const struct parsedname * const pn ) {
    if ( pn ) { // do check here to avoid needless processing
        time_t duration = TimeOut( pn->ft->change ) ;
        if ( duration > 0 ) {
            struct tree_node tn  ;
            memcpy( tn.tk.sn , pn->sn , 8 ) ;
            tn.tk.p.ft = pn->ft ;
            tn.tk.extension = pn->extension ;
            switch(pn->ft->change) {
            case ft_persistent:
                return Cache_Del_Store(&tn) ;
            default:
                return Cache_Del_Common(&tn) ;
            }
        }
    }
    return 1 ;
}

int Cache_Del_Internal( const struct internal_prop * ip, const struct parsedname * const pn ) {
    if ( pn ) { // do check here to avoid needless processing
        time_t duration = TimeOut( ip->change ) ;
        if ( duration > 0 ) {
            struct tree_node tn  ;
            memcpy( tn.tk.sn , pn->sn , 8 ) ;
            tn.tk.p.nm = ip->name ;
            tn.tk.extension = 0 ;
            switch(ip->change) {
            case ft_persistent:
                return Cache_Del_Store(&tn) ;
            default:
                return Cache_Del_Common(&tn) ;
            }
        }
    }
    return 1 ;
}

static int Cache_Del_Common( const struct tree_node * tn ) {
    struct tree_opaque * opaque ;
    time_t now = time(NULL) ;
    int ret = 1 ;
    CACHELOCK
        if ( (opaque=tfind( tn, &cache.new_db, tree_compare )) 
	     || ( (cache.killed>now) && (opaque=tfind( tn, &cache.old_db, tree_compare )) )
	   ) {
	    opaque->key->expires = now - 1 ;
	    ret = 0 ;
            STATLOCK
                ++ cache_dels ; /* statistics */
            STATUNLOCK
        }
    CACHEUNLOCK
    return ret ;
}

static int Cache_Del_Store( const struct tree_node * tn ) {
    struct tree_opaque * opaque ;
    struct tree_node * tn_found = NULL ;
    STORELOCK
        if ( (opaque = tfind( tn , &cache.store, tree_compare )) ) {
	    tn_found = opaque->key ;
            tdelete( tn , &cache.store , tree_compare ) ;
        }
    STOREUNLOCK
    if ( tn_found ) {
	free(tn_found) ;
	STATLOCK
	    AVERAGE_OUT(&store_avg)
	STATUNLOCK
	return 0 ;
    }
    return 1 ;
}

#else /* OW_CACHE is unset */

void Cache_Open( void ) 
    {}
void Cache_Close( void ) 
    {}
char * Cache_Version( void ) 
    { return ""; }
int Cache_Add(          const void * data, const size_t datasize, const struct parsedname * const pn ) 
    { return 1; }
int Cache_Add_Internal( const void * data, const size_t datasize, const struct internal_prop * ip, const struct parsedname * const pn ) 
    { return 1; }
int Cache_Get(          void * data, size_t * dsize, const struct parsedname * const pn ) 
    { return 1; }
int Cache_Get_Internal( void * data, size_t * dsize, const struct internal_prop * ip, const struct parsedname * const pn ) 
    { return 1; }
int Cache_Del(          const struct parsedname * const pn                                                                   ) 
    { return 1; }
int Cache_Del_Internal( const struct internal_prop * ip, const struct parsedname * const pn ) 
    { return 1; }

#endif /* OW_CACHE */
