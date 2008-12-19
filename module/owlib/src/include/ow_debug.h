/* $Id$
   Debug and error messages
   Meant to be included in ow.h
   Separated just for readability
*/

/* OWFS source code
   1-wire filesystem for linux
   {c} 2006 Paul H Alfille
   License GPL2.0
*/

#ifndef OW_DEBUG_H
#define OW_DEBUG_H

#include <config.h>
#include <owfs_config.h>

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#include <stdarg.h>

/* error functions */
enum e_err_level { e_err_default, e_err_connect, e_err_call, e_err_data,
	e_err_detail, e_err_debug, e_err_beyond,
};
enum e_err_type { e_err_type_level, e_err_type_error, };
enum e_err_print { e_err_print_mixed, e_err_print_syslog,
	e_err_print_console,
};

extern const char mutex_init_failed[];
extern const char mutex_destroy_failed[];
extern const char mutex_lock_failed[];
extern const char mutex_unlock_failed[];
extern const char rwlock_init_failed[];
extern const char rwlock_read_lock_failed[];
extern const char rwlock_read_unlock_failed[];
extern const char cond_timedwait_failed[];
extern const char cond_signal_failed[];
extern const char cond_wait_failed[];

void err_msg(enum e_err_type errnoflag, enum e_err_level level, const char *fmt, ...);
void _Debug_Bytes(const char *title, const unsigned char *buf, int length);
void _Debug_Writev(struct iovec *io, int iosz);
void fatal_error(char *file, int row, const char *fmt, ...);
static inline int return_ok(void) { return 0; }

extern int log_available;

#if OW_DEBUG
#define LEVEL_DEFAULT(...)    if (Globals.error_level>=e_err_default) \
    err_msg(e_err_type_level,e_err_default,__VA_ARGS__)
#define LEVEL_CONNECT(...)    if (Globals.error_level>=e_err_connect) \
    err_msg(e_err_type_level,e_err_connect,__VA_ARGS__)
#define LEVEL_CALL(...)       if (Globals.error_level>=e_err_call)  \
    err_msg(e_err_type_level,e_err_call,__VA_ARGS__)
#define LEVEL_DATA(...)       if (Globals.error_level>=e_err_data) \
    err_msg(e_err_type_level,e_err_data,__VA_ARGS__)
#define LEVEL_DETAIL(...)     if (Globals.error_level>=e_err_detail) \
    err_msg(e_err_type_level,e_err_detail,__VA_ARGS__)
#define LEVEL_DEBUG(...)      if (Globals.error_level>=e_err_debug) \
    err_msg(e_err_type_level,e_err_debug,__VA_ARGS__)

#define ERROR_DEFAULT(...)    if (Globals.error_level>=e_err_default) \
    err_msg(e_err_type_error,e_err_default,__VA_ARGS__)
#define ERROR_CONNECT(...)    if (Globals.error_level>=e_err_connect) \
    err_msg(e_err_type_error,e_err_connect,__VA_ARGS__)
#define ERROR_CALL(...)       if (Globals.error_level>=e_err_call)  \
    err_msg(e_err_type_error,e_err_call,__VA_ARGS__)
#define ERROR_DATA(...)       if (Globals.error_level>=e_err_data) \
    err_msg(e_err_type_error,e_err_data,__VA_ARGS__)
#define ERROR_DETAIL(...)     if (Globals.error_level>=e_err_detail) \
    err_msg(e_err_type_error,e_err_detail,__VA_ARGS__)
#define ERROR_DEBUG(...)      if (Globals.error_level>=e_err_debug) \
    err_msg(e_err_type_error,e_err_debug,__VA_ARGS__)

#define FATAL_ERROR(...) fatal_error(__FILE__, __LINE__, __VA_ARGS__)

#define Debug_OWQ(owq)        if (Globals.error_level>=e_err_debug) \
    _print_owq(owq)
#define Debug_Bytes(title,buf,length)    if (Globals.error_level>=e_err_beyond) \
    _Debug_Bytes(title,buf,length)
#define Debug_Writev(io,iosz)    if (Globals.error_level>=e_err_beyond) \
    _Debug_Writev(io, iosz)
#else
#define LEVEL_DEFAULT(...)    { } while (0);
#define LEVEL_CONNECT(...)    { } while (0);
#define LEVEL_CALL(...)       { } while (0);
#define LEVEL_DATA(...)       { } while (0);
#define LEVEL_DETAIL(...)     { } while (0);
#define LEVEL_DEBUG(...)      { } while (0);

#define ERROR_DEFAULT(...)    { } while (0);
#define ERROR_CONNECT(...)    { } while (0);
#define ERROR_CALL(...)       { } while (0);
#define ERROR_DATA(...)       { } while (0);
#define ERROR_DETAIL(...)     { } while (0);
#define ERROR_DEBUG(...)      { } while (0);

#define FATAL_ERROR(...)      { } while (0);

#define Debug_Bytes(title,buf,length)    { } while (0);
#define Debug_Writev(io, iosz)    { } while (0);
#define Debug_OWQ(owq)        { } while (0);
#endif

/* Make sure strings are safe for printf */
#define SAFESTRING(x) ((x) ? (x):"")

/* Easy way to show 64bit serial numbers */
#define SNformat	"%.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X"
#define SNvar(sn)	(sn)[0],(sn)[1],(sn)[2],(sn)[3],(sn)[4],(sn)[5],(sn)[6],(sn)[7]

#if OW_MT
/* Need to define those functions to get FILE and LINE information */
#define my_pthread_mutex_init(mutex, attr) \
{ \
	int mrc = pthread_mutex_init(mutex, attr);	\
	if(mrc != 0) { \
		FATAL_ERROR(mutex_init_failed, mrc, strerror(mrc)); \
	} \
}

#define my_pthread_mutex_destroy(mutex) \
{ \
	int mrc = pthread_mutex_destroy(mutex); \
	if(mrc != 0) { \
		FATAL_ERROR(mutex_destroy_failed, mrc, strerror(mrc)); \
	} \
}

#define my_pthread_mutex_lock(mutex) \
{ \
	int mrc = pthread_mutex_lock(mutex); \
	if(mrc != 0) { \
		FATAL_ERROR(mutex_lock_failed, mrc, strerror(mrc)); \
	} \
}

#define my_pthread_mutex_unlock(mutex) \
{ \
	int mrc = pthread_mutex_unlock(mutex); \
	if(mrc != 0) { \
		FATAL_ERROR(mutex_unlock_failed, mrc, strerror(mrc)); \
	} \
}
#endif /* OW_MT */

#endif							/* OW_DEBUG_H */
