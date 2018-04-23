// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/*
 *  @file: nnti_util.hpp
 *
 *  Created on: Jul 13, 2015
 *      Author: thkorde
 */

#ifndef NNTI_UTIL_HPP_
#define NNTI_UTIL_HPP_

#include <errno.h>
#include <time.h>
#if NNTI_HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if NNTI_HAVE_ENDIAN_H
#include <endian.h>
#endif

#include <boost/lexical_cast.hpp>

#include "nnti/nntiConfig.h"

#include "nnti/nnti_logger.hpp"

#if defined(NNTI_ENABLE_STATS)
#define NNTI_STATS_DATA(x) x
#else
#define NNTI_STATS_DATA(x)
#endif

#if defined(NNTI_ENABLE_FAST_STATS)
#define NNTI_FAST_STAT(x) x
#else
#define NNTI_FAST_STAT(x)
#endif

#if defined(NNTI_ENABLE_SLOW_STATS)
#define NNTI_SLOW_STAT(x) x
#else
#define NNTI_SLOW_STAT(x)
#endif

namespace nnti  {
namespace util {

static uint32_t
str2uint32(const std::string &s)
{
    uint32_t i = 0;

    try {
        i = boost::lexical_cast<uint32_t>(s);
    }
    catch (boost::bad_lexical_cast &) {
        // i is already 0.  do nothing.
        // i = 0;
    }

    return i;
}
static uint64_t
str2uint64(const std::string &s)
{
    uint64_t i = 0;

    try {
        i = boost::lexical_cast<uint64_t>(s);
    }
    catch (boost::bad_lexical_cast &) {
        // i is already 0.  do nothing.
        // i = 0;
    }

    return i;
}

static int32_t
str2int32(const std::string &s)
{
    int32_t i = 0;

    try {
        i = boost::lexical_cast<int32_t>(s);
    }
    catch (boost::bad_lexical_cast &) {
        // i is already 0.  do nothing.
        // i = 0;
    }

    return i;
}
static int64_t
str2int64(const std::string &s)
{
    int64_t i = 0;

    try {
        i = boost::lexical_cast<int64_t>(s);
    }
    catch (boost::bad_lexical_cast &) {
        // i is already 0.  do nothing.
        // i = 0;
    }

    return i;
}

static inline int
sleep(
    const uint64_t msec)
{
    int rc=0;
    struct timespec ts, rmtp;

    ts.tv_sec=0;
    if (msec < 1000) {
        ts.tv_nsec=msec*1000*1000; /* 1sec == 1ns*1000*1000*1000; */
    } else {
        uint64_t sec=msec/1000;
        ts.tv_sec=sec;
        ts.tv_nsec=(msec-(sec*1000))*1000*1000;
    }

    rc=nanosleep(&ts, &rmtp);
    if (rc!=0) {
        log_error("nnti_util", "nanosleep failed: %s\n", strerror(errno));
    }

    return(rc);
}

/* Thomas Wang's 64 bit to 32 bit Hash Function (http://www.concentric.net/~ttwang/tech/inthash.htm) */
static inline uint32_t
hash6432shift(
    uint64_t key)
{
    key = (~key) + (key << 18); // key = (key << 18) - key - 1;
    key = key ^ (key >> 31);
    key = key * 21;             // key = (key + (key << 2)) + (key << 4);
    key = key ^ (key >> 11);
    key = key + (key << 6);
    key = key ^ (key >> 22);
    return (uint32_t)key;
}

/*
 * Convert a 64 bit value from big endian to host byte order.
 */
static inline uint64_t betoh64(uint64_t val)
{
#if __BYTE_ORDER == __BIG_ENDIAN
    return val;
#elif NNTI_HAVE_BE64TOH
    return be64toh(val);
#else
    union { uint64_t ll;
            uint32_t l[2];
    } w, r;
    w.ll = val;
    r.l[0] = ntohl(w.l[1]);
    r.l[1] = ntohl(w.l[0]);
    return r.ll;
#endif
}

} /* namespace util */
} /* namespace nnti */

#endif /* NNTI_UTIL_HPP_ */
