// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/*
 * sbl_logger.cpp
 *
 *  Created on: Aug 05, 2015
 *      Author: thkorde
 */

#include "faodelConfig.h"
#include "sbl/sbl_boost_headers.hh"

#include "sbl/sbl_source.hh"
#include "sbl/sbl_stream.hh"
#include "sbl/sbl_logger.hh"


uint64_t sbl::logger::logger_id_=0;


namespace sbl  {

    logger::logger(
        const severity_level severity)
    : stream_(severity),
      debug_source_(severity_level::debug),
      info_source_(severity_level::info),
      warning_source_(severity_level::warning),
      error_source_(severity_level::error),
      fatal_source_(severity_level::fatal)
    {
        logger::logger_id_++;

        init(logger::logger_id_);

        // this shouldn't be necessary.  the severity should be set in the stream ctor.
        set_severity(severity);
    }
    logger::logger(
        std::ostream&        stream,
        const severity_level severity)
    : stream_(stream, severity),
      debug_source_(severity_level::debug),
      info_source_(severity_level::info),
      warning_source_(severity_level::warning),
      error_source_(severity_level::error),
      fatal_source_(severity_level::fatal)
    {
        logger::logger_id_++;

        init(logger::logger_id_);

        // this shouldn't be necessary.  the severity should be set in the stream ctor.
        set_severity(severity);
    }
    logger::logger(
        const std::string    filename,
        const severity_level severity)
    : stream_(filename, severity),
      debug_source_(severity_level::debug),
      info_source_(severity_level::info),
      warning_source_(severity_level::warning),
      error_source_(severity_level::error),
      fatal_source_(severity_level::fatal)
    {
        logger::logger_id_++;

        init(logger::logger_id_);

        // this shouldn't be necessary.  the severity should be set in the stream ctor.
        set_severity(severity);
    }
    logger::~logger()
    {
    }

    void
    logger::set_severity(
            const severity_level severity)
    {
        stream_.set_severity(severity);
    }

    severity_level
    logger::severity(void)
    {
        return stream_.severity();
    }

    void
    logger::set_channel_severity(
            const std::string    channel,
            const severity_level severity)
    {
        stream_.set_channel_severity(channel, severity);
    }

    void
    logger::flush(void)
    {
        stream_.flush();
    }

    void
    logger::debug(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        ...)
    {
        va_list params;

        va_start(params, msg);

        debug_source_.log(
            channel,
            func_name,
            file_name,
            line_num,
            msg,
            params);

        va_end(params);

        return;
    }

    void
    logger::debug(
        const char *channel,
        const char *msg,
        ...)
    {
        va_list params;

        va_start(params, msg);

        debug_source_.log(
            channel,
            msg,
            params);

        va_end(params);

        return;
    }

    void
    logger::info(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        ...)
    {
        va_list params;

        va_start(params, msg);

        info_source_.log(
            channel,
            func_name,
            file_name,
            line_num,
            msg,
            params);

        va_end(params);

        return;
    }

    void
    logger::info(
        const char *channel,
        const char *msg,
        ...)
    {
        va_list params;

        va_start(params, msg);

        info_source_.log(
            channel,
            msg,
            params);

        va_end(params);

        return;
    }

    void
    logger::warning(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        ...)
    {
        va_list params;

        va_start(params, msg);

        warning_source_.log(
            channel,
            func_name,
            file_name,
            line_num,
            msg,
            params);

        va_end(params);

        return;
    }

    void
    logger::warning(
        const char *channel,
        const char *msg,
        ...)
    {
        va_list params;

        va_start(params, msg);

        warning_source_.log(
            channel,
            msg,
            params);

        va_end(params);

        return;
    }

    void
    logger::error(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        ...)
    {
        va_list params;

        va_start(params, msg);

        error_source_.log(
            channel,
            func_name,
            file_name,
            line_num,
            msg,
            params);

        va_end(params);

        return;
    }

    void
    logger::error(
        const char *channel,
        const char *msg,
        ...)
    {
        va_list params;

        va_start(params, msg);

        error_source_.log(
            channel,
            msg,
            params);

        va_end(params);

        return;
    }

    void
    logger::fatal(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        ...)
    {
        va_list params;

        va_start(params, msg);

        fatal_source_.log(
            channel,
            func_name,
            file_name,
            line_num,
            msg,
            params);

        va_end(params);

        return;
    }

    void
    logger::fatal(
        const char *channel,
        const char *msg,
        ...)
    {
        va_list params;

        va_start(params, msg);

        fatal_source_.log(
            channel,
            msg,
            params);

        va_end(params);

        return;
    }

    sbl::source&
    logger::debug_source(void)
    {
        return debug_source_;
    }

    sbl::source&
    logger::info_source(void)
    {
        return info_source_;
    }

    sbl::source&
    logger::warning_source(void)
    {
        return warning_source_;
    }

    sbl::source&
    logger::error_source(void)
    {
        return error_source_;
    }

    sbl::source&
    logger::fatal_source(void)
    {
        return fatal_source_;
    }

    /* private methods */
    void
    logger::init(
        uint64_t logger_id)
    {
        stream_.set_logger_id(logger_id);

        debug_source_.set_logger_id(logger_id);
        info_source_.set_logger_id(logger_id);
        warning_source_.set_logger_id(logger_id);
        error_source_.set_logger_id(logger_id);
        fatal_source_.set_logger_id(logger_id);
    }
}
