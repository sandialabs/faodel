// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/*
 * sbl_logger.hh
 *
 *  Created on: Jul 13, 2015
 *      Author: thkorde
 */

#ifndef SBL_LOGGER_HH_
#define SBL_LOGGER_HH_

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
#include "sbl/sbl_source.hh"
#include "sbl/sbl_stream.hh"

namespace sbl  {

/*
 * This is the simplified logger class.  It creates one stream with a
 * severity threshold and a source for each severity level.
 */
class logger {
public:
    logger(
        const severity_level severity);
    logger(
        std::ostream&        stream,
        const severity_level severity);
    logger(
        const std::string    filename,
        const severity_level severity);
    ~logger();

    void
    set_severity(
            const severity_level severity);
    severity_level
    severity(void);
    void
    set_channel_severity(
            const std::string    channel,
            const severity_level severity);
    void
    flush(void);
    void
    debug(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        ...);
    void
    debug(
        const char *channel,
        const char *msg,
        ...);
    void
    info(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        ...);
    void
    info(
        const char *channel,
        const char *msg,
        ...);
    void
    warning(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        ...);
    void
    warning(
        const char *channel,
        const char *msg,
        ...);
    void
    error(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        ...);
    void
    error(
        const char *channel,
        const char *msg,
        ...);
    void
    fatal(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        ...);
    void
    fatal(
        const char *channel,
        const char *msg,
        ...);
    sbl::source&
    debug_source(void);
    sbl::source&
    info_source(void);
    sbl::source&
    warning_source(void);
    sbl::source&
    error_source(void);
    sbl::source&
    fatal_source(void);

private:
    void
    init(
        uint64_t logger_id);

private:
    static uint64_t logger_id_;

    sbl::stream stream_;

    sbl::source debug_source_;
    sbl::source info_source_;
    sbl::source warning_source_;
    sbl::source error_source_;
    sbl::source fatal_source_;
};

} /* namespace sbl */

#endif /* SBL_LOGGER_HH_ */
