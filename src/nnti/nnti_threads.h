// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/**
 * @file nnti_threads.h
 *
 * @brief nnti_threads.h
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Jul 07, 2015
 */

/**
 * NNTI does not have any internal threading, but it should run in a
 * multithreaded environment.  The nthread library provides a single API
 * for locks and atomic counters.
 */

#ifndef _NNTI_THREADS_H_
#define _NNTI_THREADS_H_

#include "nnti/nntiConfig.h"

#include "nnti/nnti_threads_types.h"


#ifdef __cplusplus
extern "C" {
#endif

int nthread_lock_init(
        nthread_lock_t *lock);
int nthread_lock(
        nthread_lock_t *lock);
int nthread_unlock(
        nthread_lock_t *lock);
int nthread_lock_fini(
        nthread_lock_t *lock);

int nthread_cond_init(
        nthread_cond_t *condvar);
int nthread_wait(
        nthread_cond_t *condvar,
        nthread_lock_t *lock);
int nthread_timedwait(
        nthread_cond_t *condvar,
        nthread_lock_t *lock,
        uint64_t        timeout);
int nthread_signal(
        nthread_cond_t *condvar);
int nthread_broadcast(
        nthread_cond_t *condvar);
int nthread_cond_fini(
        nthread_cond_t *condvar);

int     nthread_counter_init(
        nthread_counter_t *c);
int64_t nthread_counter_increment(
        nthread_counter_t *c);
int64_t nthread_counter_decrement(
        nthread_counter_t *c);
int64_t nthread_counter_read(
        nthread_counter_t *c);
int64_t nthread_counter_set(
        nthread_counter_t *c,
        int64_t            new_value);
int     nthread_counter_fini(
        nthread_counter_t *c);

#ifdef __cplusplus
}
#endif

#endif /* _NNTI_THREADS_H_ */
