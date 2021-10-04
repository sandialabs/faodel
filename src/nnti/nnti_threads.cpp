// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/**
 * @file nnti_threads.cpp
 *
 * @brief nnti_threads.cpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Jul 07, 2015
 */

/**
 * NNTI does not have any internal threading, but it should run in a
 * multithreaded environment.  The nthread library provides a single API
 * for locks and atomic counters.
 */

#include "nnti/nnti_pch.hpp"

#include "nnti/nntiConfig.h"

#include <fcntl.h>
#include <sys/stat.h>

#ifdef NNTI_HAVE_MALLOC_H
#include <malloc.h>
#endif

#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include <errno.h>
#include <string.h>

#if defined(NNTI_HAVE_PTHREAD_H)
#include <pthread.h>
#endif

#include <chrono>

#include "nnti/nnti_logger.hpp"
#include "nnti/nnti_threads.h"

#define _DEBUG_LOCKS_
#undef _DEBUG_LOCKS_


int nthread_lock_init(
        nthread_lock_t *lock)
{
    int  rc=0;

#ifdef _DEBUG_LOCKS_
    log_debug("nnti_threads", "nthread_lock_init: initializing lock(%p)\n", (void*)lock);
#endif

#if defined(NNTI_HAVE_PTHREAD_MUTEX_INIT)
    pthread_mutex_init(&lock->lock, NULL);
#else
#warning No locking mechanism available on this system.
#endif

#ifdef _DEBUG_LOCKS_
    log_debug("nnti_threads", "nthread_lock_init: initialized lock(%p), lock->name(%s)\n", (void*)lock, lock->name);
#endif

    return(rc);
}

int nthread_lock(
        nthread_lock_t *lock)
{
    int rc=0;

#ifdef _DEBUG_LOCKS_
    log_debug("nnti_threads", "nthread_lock: locking lock(%p)\n", (void*)lock);
#endif

#if defined(NNTI_HAVE_PTHREAD_MUTEX_LOCK)
    pthread_mutex_lock(&lock->lock);
#else
#warning No locking mechanism available on this system.
#endif

#ifdef _DEBUG_LOCKS_
    log_debug("nnti_threads", "nthread_lock: locked lock(%p)\n", (void*)lock);
#endif

    return(rc);
}

int nthread_unlock(
        nthread_lock_t *lock)
{
    int rc=0;

#ifdef _DEBUG_LOCKS_
    log_debug("nnti_threads", "nthread_unlock: unlocking lock(%p)\n", (void*)lock);
#endif

#if defined(NNTI_HAVE_PTHREAD_MUTEX_UNLOCK)
    pthread_mutex_unlock(&lock->lock);
#else
#warning No locking mechanism available on this system.
#endif

#ifdef _DEBUG_LOCKS_
    log_debug("nnti_threads", "nthread_unlock: unlocked lock(%p)\n", (void*)lock);
#endif

    return(rc);
}


int nthread_lock_fini(
        nthread_lock_t *lock)
{
    int rc=0;

#ifdef _DEBUG_LOCKS_
    log_debug("nnti_threads", "nthread_lock_fini: finalizing lock(%p), lock->name(%s)", (void*)lock, lock->name);
#endif

#if defined(NNTI_HAVE_PTHREAD_MUTEX_DESTROY)
    pthread_mutex_destroy(&lock->lock);
#else
#warning No locking mechanism available on this system.
#endif

#ifdef _DEBUG_LOCKS_
    log_debug("nnti_threads", "nthread_lock_fini: finalized lock(%p), lock->name(%s)", (void*)lock, lock->name);
#endif

    return(rc);
}


int nthread_cond_init(
        nthread_cond_t *condvar)
{
    int rc=0;

#if defined(NNTI_HAVE_PTHREAD_COND_INIT)
    pthread_cond_init(&condvar->condvar, NULL);
#endif

    return(rc);
}

int nthread_wait(
        nthread_cond_t *condvar,
        nthread_lock_t *lock)
{
    int rc=0;

#if defined(NNTI_HAVE_PTHREAD_COND_WAIT)
    pthread_cond_wait(&condvar->condvar, &lock->lock);
#endif

    return(rc);
}

int nthread_timedwait(
        nthread_cond_t *condvar,
        nthread_lock_t *lock,
        uint64_t        timeout)
{
    int rc=0;

#if defined(NNTI_HAVE_PTHREAD_COND_TIMEDWAIT)
    struct timespec abstime;

    auto now = std::chrono::high_resolution_clock::now();

    std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::nanoseconds> ns;
    ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(now);

    std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::nanoseconds> abs_timeout_ns(std::chrono::nanoseconds(ns.time_since_epoch().count() + timeout*1000000));

    std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::seconds> sec;
    sec = std::chrono::time_point_cast<std::chrono::seconds>(abs_timeout_ns);

    abstime.tv_sec  = sec.time_since_epoch().count();
    abstime.tv_nsec = abs_timeout_ns.time_since_epoch().count() - (abstime.tv_sec * 1000000000);

    rc=pthread_cond_timedwait(&condvar->condvar, &lock->lock, &abstime);
#endif

    return(rc);
}

int nthread_signal(
        nthread_cond_t *condvar)
{
    int rc=0;

#if defined(NNTI_HAVE_PTHREAD_COND_SIGNAL)
    pthread_cond_signal(&condvar->condvar);
#endif

    return(rc);
}

int nthread_broadcast(
        nthread_cond_t *condvar)
{
    int rc=0;

#if defined(NNTI_HAVE_PTHREAD_COND_BROADCAST)
    pthread_cond_broadcast(&condvar->condvar);
#endif

    return(rc);
}

int nthread_cond_fini(
        nthread_cond_t *condvar)
{
    int rc=0;

#if defined(NNTI_HAVE_PTHREAD_COND_DESTROY)
    pthread_cond_destroy(&condvar->condvar);
#endif

    return(rc);
}


int nthread_counter_init(
        nthread_counter_t *c)
{
    int rc=0;

    log_debug("nnti_threads", "nthread_counter_init(STUB)");
    rc=nthread_lock_init(&c->lock);
    if (rc == -1) {
        log_error("nnti_threads", "nthread_counter_init: nthread_lock_init failed: %s\n", strerror(errno));
        return(-1);
    }

    if (nthread_lock(&c->lock) != 0) {
        log_error("nnti_threads", "nthread_counter_init: failed to lock the counter lock: %s\n", strerror(errno));
        goto cleanup;
    }

    c->value = 0;

    if (nthread_unlock(&c->lock) != 0) {
        log_error("nnti_threads", "nthread_counter_init: failed to unlock the counter lock: %s\n", strerror(errno));
    }

cleanup:
    return rc;
}

int64_t nthread_counter_increment(
        nthread_counter_t *c)
{
    int64_t t=0;

    log_debug("nnti_threads", "nthread_counter_increment(STUB)");

    if (nthread_lock(&c->lock) != 0) {
        log_error("nnti_threads", "nthread_counter_increment: failed to lock the counter lock: %s\n", strerror(errno));
        t = -1;
        goto cleanup;
    }

    t = c->value;
    c->value++;

    if (nthread_unlock(&c->lock) != 0) {
        log_error("nnti_threads", "nthread_counter_increment: failed to unlock the counter lock: %s\n", strerror(errno));
    }

cleanup:
    return t;
}

int64_t nthread_counter_decrement(
        nthread_counter_t *c)
{
    int64_t t=0;

    log_debug("nnti_threads", "nthread_counter_decrement(STUB)");

    if (nthread_lock(&c->lock) != 0) {
        log_error("nnti_threads", "nthread_counter_decrement: failed to lock the counter lock: %s\n", strerror(errno));
        t = -1;
        goto cleanup;
    }

    t = c->value;
    c->value--;

    if (nthread_unlock(&c->lock) != 0) {
        log_error("nnti_threads", "nthread_counter_decrement: failed to unlock the counter lock: %s\n", strerror(errno));
    }

cleanup:
    return t;
}

int64_t nthread_counter_read(
        nthread_counter_t *c)
{
    int64_t t=0;

    log_debug("nnti_threads", "nthread_counter_read(STUB)");

    if (nthread_lock(&c->lock) != 0) {
        log_error("nnti_threads", "nthread_counter_read: failed to lock the counter lock: %s\n", strerror(errno));
        t = -1;
        goto cleanup;
    }

    t = c->value;

    if (nthread_unlock(&c->lock) != 0) {
        log_error("nnti_threads", "nthread_counter_read: failed to unlock the counter lock: %s\n", strerror(errno));
    }

cleanup:
    return t;
}

int64_t nthread_counter_set(
        nthread_counter_t *c,
        int64_t            new_value)
{
    int64_t t=0;

    log_debug("nnti_threads", "nthread_counter_set(STUB)");

    if (nthread_lock(&c->lock) != 0) {
        log_error("nnti_threads", "nthread_counter_set: failed to lock the counter lock: %s\n", strerror(errno));
        t = -1;
        goto cleanup;
    }

    t = c->value;
    c->value=new_value;

    if (nthread_unlock(&c->lock) != 0) {
        log_error("nnti_threads", "nthread_counter_set: failed to unlock the counter lock: %s\n", strerror(errno));
    }

cleanup:
    return t;
}

int nthread_counter_fini(
        nthread_counter_t *c)
{
    int rc=0;

    log_debug("nnti_threads", "nthread_counter_fini(STUB)");
    if (nthread_lock(&c->lock) != 0) {
        log_error("nnti_threads", "nthread_counter_fini: failed to lock the counter lock: %s\n", strerror(errno));
        goto cleanup;
    }

    c->value = 0;

    if (nthread_unlock(&c->lock) != 0) {
        log_error("nnti_threads", "nthread_counter_fini: failed to unlock the counter lock: %s\n", strerror(errno));
    }

    rc=nthread_lock_fini(&c->lock);
    if (rc == -1) {
        log_error("nnti_threads", "nthread_counter_fini: nthread_lock_fini failed: %s\n", strerror(errno));
        return(-1);
    }

cleanup:
    return rc;
}
