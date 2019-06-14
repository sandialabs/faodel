// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/*
 * sbl_source.hh
 *
 *  Created on: Jul 13, 2015
 *      Author: thkorde
 */

#ifndef SBL_SOURCE_HH_
#define SBL_SOURCE_HH_

#include "faodelConfig.h"
#include "sbl/sbl_boost_headers.hh"

#include <errno.h>
#include <time.h>
#include <stdint.h>
#ifdef SBL_HAVE_SYSCALL_H
#include <syscall.h>
#endif

#include <cstring>
#include <cstdarg>
#include <fstream>
#include <map>
#include <string>

#include "sbl/sbl_types.hh"

/*
 * This is some serious voodoo.  The difficulty here is that we
 * need to create a temporary record variable but we also need to
 * append stream operators after this macro is expanded, so we
 * can't use braces to create a temporary variable.  Instead,
 * we use a for-loop which creates the scoping for the temporary
 * record variable without requiring braces.
 *
 * I take no credit for this.  This a specialization of the
 * BOOST_LOG_STREAM_WITH_PARAMS_INTERNAL macro found here:
 * boost/log/sources/record_ostream.hpp
 */
#if 0
#define SBL_LOG_STREAM(s, c) \
    for (::boost::log::record _boost_log_record = (s).boostlogger().open_record((::boost::log::keywords::channel = (c), ::boost::log::keywords::severity = ((s).severity()))); \
         !!_boost_log_record;) \
         ((s).make_record_pump(_boost_log_record)).stream()
#endif

#define SBL_LOG(s, ...) s.log("", __FUNCTION__,__FILE__,__LINE__, ## __VA_ARGS__)
#define SBL_LOG_STREAM(s, c) BOOST_LOG_CHANNEL_SEV(s.boostlogger(), c, s.severity())

namespace sbl  {

typedef boost::log::sources::severity_channel_logger_mt< severity_level, std::string > sbl_logger ;
typedef boost::log::aux::record_pump< sbl_logger > sbl_record_pump;


class source {
public:
    source(
        severity_level  severity);
    source(
        uint64_t       logger_id,
        severity_level severity);
    ~source();

    sbl_logger& boostlogger(void);
    sbl_record_pump make_record_pump(boost::log::record &rec);
    severity_level severity(void);
    void set_logger_id(uint64_t id);

    void log(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        ...);
    void log(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        va_list     params);
    void log(
        const char *channel,
        const char *prefix,
        const char *msg);
    void log(
        const char *channel,
        const char *msg);
    void log(
        const char *channel,
        const char *msg,
        va_list     params);

private:
    void disable(void);
    void output(
            const char *channel,
            const char *msg);
    void output(
            const char *channel,
            const char *prefix,
            const char *msg);
    void output(
            const char *channel,
            const char *msg,
            va_list     params);
    void output(
            const char *channel,
            const char *prefix,
            const char *msg,
            va_list     params);

private:
    uint64_t                                           logger_id_;
    boost::log::attributes::mutable_constant<uint64_t> logger_id_attr_;

    severity_level severity_;
    sbl_logger     boostlogger_;

    bool disabled;
};

} /* namespace sbl */

#endif /* SBL_SOURCE_HH_ */
