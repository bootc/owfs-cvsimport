/* ----------------------------------------------------------------------------
 * This file was automatically generated by SWIG (http://www.swig.org).
 * Version 1.3.24
 * 
 * This file is not intended to be easily readable and contains a number of 
 * coding conventions designed to improve portability and efficiency. Do not make
 * changes to this file unless you know what you are doing--modify the SWIG 
 * interface file instead. 
 * ----------------------------------------------------------------------------- */

/*
  +----------------------------------------------------------------------+
  | PHP version 4.0                                                      |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997, 1998, 1999, 2000, 2001 The PHP Group             |
  +----------------------------------------------------------------------+
  | This source file is subject to version 2.02 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available at through the world-wide-web at                           |
  | http://www.php.net/license/2_02.txt.                                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors:                                                             |
  |                                                                      |
  +----------------------------------------------------------------------+
 */


#ifndef PHP_OW_H
#define PHP_OW_H

extern zend_module_entry OW_module_entry;
#define phpext_OW_ptr &OW_module_entry

#ifdef PHP_WIN32
# define PHP_OW_API __declspec(dllexport)
#else
# define PHP_OW_API
#endif

PHP_MINIT_FUNCTION(OW);
PHP_MSHUTDOWN_FUNCTION(OW);
PHP_RINIT_FUNCTION(OW);
PHP_RSHUTDOWN_FUNCTION(OW);
PHP_MINFO_FUNCTION(OW);

ZEND_NAMED_FUNCTION(_wrap_version);
ZEND_NAMED_FUNCTION(_wrap_init);
ZEND_NAMED_FUNCTION(_wrap_get);
ZEND_NAMED_FUNCTION(_wrap_put);
ZEND_NAMED_FUNCTION(_wrap_finish);
ZEND_NAMED_FUNCTION(_wrap_error_print_set);
ZEND_NAMED_FUNCTION(_wrap_error_print_get);
ZEND_NAMED_FUNCTION(_wrap_error_level_set);
ZEND_NAMED_FUNCTION(_wrap_error_level_get);
/*If you declare any globals in php_OW.h uncomment this:
ZEND_BEGIN_MODULE_GLOBALS(OW)
ZEND_END_MODULE_GLOBALS(OW)
*/
#ifdef ZTS
#define OW_D  zend_OW_globals *OW_globals
#define OW_DC  , OW_D
#define OW_C  OW_globals
#define OW_CC  , OW_C
#define OW_SG(v)  (OW_globals->v)
#define OW_FETCH()  zend_OW_globals *OW_globals = ts_resource(OW_globals_id)
#else
#define OW_D
#define OW_DC
#define OW_C
#define OW_CC
#define OW_SG(v)  (OW_globals.v)
#define OW_FETCH()
#endif

#endif /* PHP_OW_H */
