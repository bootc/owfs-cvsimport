/*
$Id$
   OWFS (owfs, owhttpd, owserver, owperl, owtcl, owphp, owpython, owcapi)
   one-wire file system and related programs

    By Paul H Alfille
    {c} 2008 GPL
    palfille@earthlink.net
*/

/* This is the libownet header
   a C programmikng interface to easily access owserver
   and thus the entire Dallas/Maxim 1-wire system.

   This header has all the public routines for a program linking in the library
*/

/* OWNETAPI - specific header */
/* OWNETAPI is the simple C library for OWFS */

#ifndef OWNETAPI_H
#define OWNETAPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

/* OWNET_HANDLE
   A (non-negative) integer corresponding to a particular owserver connection.
   It is used for each function call, and allows multiple owservers to be
   accessed
*/
typedef int OWNET_HANDLE ;

/* OWNET_HANDLE OWNET_init( const char * owserver )
   Starting routine -- takes a string corresponding to the tcp address of owserver
   e.g. "192.168.0.1:5000" or "5001" or even "" for the default localhost:4304

   returns a non-negative HANDLE, or <0 for error
*/
OWNET_HANDLE OWNET_init( const char * owserver_tcp_address_and_port ) ;

/* int OWNET_dirlist( OWNET_HANDLE h, const char * onewire_path, 
        char ** return_string )
   Get the 1-wire directory as a comma-separated list.
   return_string is allocated by this program, and must be free-ed by your program.
   
   return non-negative length of return_string on success
   return <0 error and NULL on error
*/
int OWNET_dirlist( OWNET_HANDLE h, const char * onewire_path, char ** return_string ) ;

/* int OWNET_dirprocess( OWNET_HANDLE h, const char * onewire_path, 
        void (*dirfunc) (void * passed_on_value, const char* directory_element), 
        void * passed_on_value )
   Get the 1-wire directory corresponding to the given path
   Call function dirfunc on each element
   passed_on_value is an arbitrary pointer that gets included in the dirfunc call to
   add some state information

   returns number of elements processed,
   or <0 for error
*/
int OWNET_dirprocess( OWNET_HANDLE h, const char * onewire_path, void (*dirfunc) (void * passed_on_value, const char * directory_element), void * passed_on_value ) ;


/* int OWNET_read( OWNET_HANDLE h, const char * onewire_path, 
        char * return_string )
   Read a value from a one-wire device property
   return_string has the result but must be free-ed by the calling program.

   returns length of result on success,
   returns <0 on error
*/
int OWNET_read( OWNET_HANDLE h, const char * onewire_path, char ** return_string ) ;

/* int OWNET_write( OWNET_HANDLE h, const char * onewire_path, 
        const char * value_string, size_t value_length )
   Write a value to a one-wire device property
   
   return 0 on success
   return <0 on error
*/
int OWNET_write( OWNET_HANDLE h, const char * onewire_path, const char * value_string, size_t value_length ) ;

/* void OWNET_close( OWNET_HANDLE h)
   close a particular owserver connection
*/
void OWNET_close( OWNET_HANDLE h) ;

/* void OWNET_closeall( void )
   close all owserver connections
*/
void OWNET_closeall( void ) ;

/* get and set temperature scale
   Note that temperature scale applies to all HANDLES
   C - celsius
   F - farenheit
   R - rankine
   K - kelvin
   0 -> set default (C)
*/
void OWNET_set_temperature_scale( char temperature_scale ) ;
char OWNET_get_temperature_scale( void ) ;

/* get and set device format
   Note that device format applies to all HANDLES
   f.i default
   f.i.c
   fi.c
   fi
   f.ic
   fic
   NULL or "" -> set default
*/
void OWNET_set_device_format( const char * device_format ) ;
const char * OWNET_get_device_format( void ) ;

#ifdef __cplusplus
}
#endif
#endif                          /* OWNETAPI_H */

