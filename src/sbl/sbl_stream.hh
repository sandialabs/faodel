// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/*
 * sbl_stream.hh
 *
 *  Created on: Jul 13, 2015
 *      Author: thkorde
 */

#ifndef SBL_STREAM_HH_
#define SBL_STREAM_HH_

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

namespace sbl  {

typedef boost::log::sinks::synchronous_sink< boost::log::sinks::text_ostream_backend > text_sink;
typedef boost::log::expressions::channel_severity_filter_actor< std::string, severity_level > min_severity_filter;

// Define the attribute keywords
BOOST_LOG_ATTRIBUTE_KEYWORD(line_id_attr,   "LineID",   unsigned int)
BOOST_LOG_ATTRIBUTE_KEYWORD(severity_attr,  "Severity", severity_level)
BOOST_LOG_ATTRIBUTE_KEYWORD(channel_attr,   "Channel",  std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(logger_id_attr, "LoggerID", uint64_t)


class stream {
public:
    stream(
        const severity_level severity);
    stream(
        std::ostream&        stream,
        const severity_level severity);
    stream(
        const std::string    filename,
        const severity_level severity);
    ~stream();

    void set_severity(
            const severity_level severity);
    severity_level severity(void);
    void set_channel_severity(
            const std::string    channel,
            const severity_level severity);
    void set_logger_id(uint64_t id);
    void flush(void);

private:
    void init(
            boost::shared_ptr< std::ostream > stream,
            const severity_level              severity);
    void set_auto_flush(
            const severity_level severity);
    void set_auto_flush(
            const std::string    channel,
            const severity_level severity);

private:
    uint64_t                          logger_id_;

    severity_level                    severity_;
    boost::shared_ptr< std::ostream > stream_;
    std::string                       filename_;

    min_severity_filter                     *min_severity_;
    std::map< std::string, severity_level >  severity_map_;

    uint32_t                          debug_count_;

    boost::shared_ptr< boost::log::sinks::text_ostream_backend > backend_;
    boost::shared_ptr< text_sink >                               sink_;
};

} /* namespace sbl */

#endif /* SBL_STREAM_HH_ */
