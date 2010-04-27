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

/* locks are to handle multithreading */

#include <config.h>
#include "owfs_config.h"
#include "ow.h"

#if OW_MT

void my_rwlock_init(my_rwlock_t * my_rwlock)
{
	MUTEX_INIT(my_rwlock->protect_reader_count);
	sem_init(&(my_rwlock->allow_readers), 0, 1);
	sem_init(&(my_rwlock->no_processes), 0, 1);
	my_rwlock->reader_count = 1;
}

inline void my_rwlock_write_lock(my_rwlock_t * my_rwlock)
{
	sem_wait(&(my_rwlock->allow_readers));

	sem_wait(&(my_rwlock->no_processes));
}

inline void my_rwlock_write_unlock(my_rwlock_t * my_rwlock)
{
	sem_post(&(my_rwlock->allow_readers));

	sem_post(&(my_rwlock->no_processes));
}

inline void my_rwlock_read_lock(my_rwlock_t * my_rwlock)
{
	sem_wait(&(my_rwlock->allow_readers));
	sem_post(&(my_rwlock->allow_readers));

	MUTEX_LOCK(my_rwlock->protect_reader_count);
	if (++my_rwlock->reader_count == 1) {
		sem_wait(&(my_rwlock->no_processes));
	}
	MUTEX_UNLOCK(my_rwlock->protect_reader_count);
}

inline void my_rwlock_read_unlock(my_rwlock_t * my_rwlock)
{
	MUTEX_LOCK(my_rwlock->protect_reader_count);
	if (--my_rwlock->reader_count == 0) {
		sem_post(&(my_rwlock->no_processes));
	}
	MUTEX_UNLOCK(my_rwlock->protect_reader_count);
}

void my_rwlock_destroy(my_rwlock_t * my_rwlock)
{
	MUTEX_DESTROY(my_rwlock->protect_reader_count);
	sem_destroy(&(my_rwlock->allow_readers));
	sem_destroy(&(my_rwlock->no_processes));
}

#endif							/* OW_MT */
