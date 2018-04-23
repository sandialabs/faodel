// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/**
 * @file nnti_threads_types.h
 *
 * @brief nnti_threads_types.h
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Jul 07, 2015
 */

/**
 * NNTI does not have any internal threading, but it should run in a
 * multithreaded environment.  The nthread library provides a single API
 * for locks and atomic counters.
 */

#ifndef _NNTI_THREADS_TYPES_H_
#define _NNTI_THREADS_TYPES_H_

#include "nnti/nntiConfig.h"

#include <stdint.h>

#if defined(NNTI_HAVE_PTHREAD_H)
#include <pthread.h>
#endif

typedef struct {
    char  *name;
#if defined(NNTI_HAVE_PTHREAD_MUTEX_T)
    pthread_mutex_t lock;
#else
#warning No locking mechanism available on this system.
#endif
} nthread_lock_t;

typedef struct {
    nthread_lock_t lock;
    int64_t        value;
} nthread_counter_t;

typedef struct {
#if defined(NNTI_HAVE_PTHREAD_COND_T)
    pthread_cond_t condvar;
#endif
} nthread_cond_t;


#endif /* _NNTI_THREADS_TYPES_H_ */
