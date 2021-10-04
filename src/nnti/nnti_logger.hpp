// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/**
 * @file nnti_logger.hpp
 *
 * @brief nnti_logger.hpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: August 03, 2015
 *
 */

#ifndef NNTI_LOGGER_HPP_
#define NNTI_LOGGER_HPP_

#include <sbl/sbl_logger.hh>

#include "nnti/nnti_types.h"

#include "nnti/nnti_logger.h"


#ifdef NNTI_ENABLE_DEBUG_LOGGING
#define NNTI_ENABLE_DEBUG_STREAM true
#else
#define NNTI_ENABLE_DEBUG_STREAM false
#endif

#define log_stream(s, c) SBL_LOG_STREAM((s), (c))

#define log_debug_stream(c) if (!NNTI_ENABLE_DEBUG_STREAM) {} else log_stream(nnti::core::logger::get_instance()->debug_source(), c)
#define log_info_stream(c)  log_stream(nnti::core::logger::get_instance()->info_source(), c)
#define log_warn_stream(c)  log_stream(nnti::core::logger::get_instance()->warning_source(), c)
#define log_error_stream(c) log_stream(nnti::core::logger::get_instance()->error_source(), c)
#define log_fatal_stream(c) log_stream(nnti::core::logger::get_instance()->fatal_source(), c)


namespace nnti {
namespace core {

class logger {
public:
    static void
    init()
    {
        if (sbl_logger == nullptr) {
            sbl_logger = new sbl::logger(sbl::severity_level::debug);
        }
    }
    static void
    init(
        const bool include_ffl)
    {
        if (sbl_logger == nullptr) {
            sbl_logger             = new sbl::logger(sbl::severity_level::debug);
            include_file_func_line = include_ffl;
        }
    }
    static void
    init(
        const sbl::severity_level severity)
    {
        if (sbl_logger == nullptr) {
            sbl_logger = new sbl::logger(severity);
        }
    }
    static void
    init(
        const bool                include_ffl,
        const sbl::severity_level severity)
    {
        if (sbl_logger == nullptr) {
            sbl_logger             = new sbl::logger(severity);
            include_file_func_line = include_ffl;
        }
    }
    static void
    init(
        std::ostream&             stream,
        const sbl::severity_level severity)
    {
        if (sbl_logger == nullptr) {
            sbl_logger = new sbl::logger(stream, severity);
        }
    }
    static void
    init(
        std::ostream&             stream,
        const bool                include_ffl,
        const sbl::severity_level severity)
    {
        if (sbl_logger == nullptr) {
            sbl_logger             = new sbl::logger(stream, severity);
            include_file_func_line = include_ffl;
        }
    }
    static void
    init(
        const std::string         filename,
        const sbl::severity_level severity)
    {
        if (sbl_logger == nullptr) {
            sbl_logger = new sbl::logger(filename, severity);
        }
    }
    static void
    init(
        const std::string         filename,
        const bool                include_ffl,
        const sbl::severity_level severity)
    {
        if (sbl_logger == nullptr) {
            sbl_logger             = new sbl::logger(filename, severity);
            include_file_func_line = include_ffl;
        }
    }

    static void
    fini()
    {
        if (sbl_logger != nullptr) {
            delete sbl_logger;
        }
    }

    static sbl::logger *
    get_instance()
    {
        if (sbl_logger == nullptr) {
            init();
        }
        return sbl_logger;
    }

    static void
    flush(void)
    {
        sbl_logger->flush();
    }

private:
    static sbl::logger *sbl_logger;

public:
    static bool include_file_func_line;
};

}
}

std::ostream&
operator<< (
    std::ostream       &os,
    const NNTI_event_t &event);

std::ostream&
operator<< (
    std::ostream       &os,
    const NNTI_event_t *event);


#endif /* NNTI_LOGGER_HPP_ */
