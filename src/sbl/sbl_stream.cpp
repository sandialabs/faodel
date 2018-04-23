// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/*
 * sbl_logger.cpp
 *
 *  Created on: Aug 05, 2015
 *      Author: thkorde
 */


#include "sbl_boost_headers.hpp"
#include "sblConfig.h"

#include "sbl_stream.hpp"


namespace sbl  {

    stream::stream(
        const severity_level severity)
    : logger_id_(0),
      debug_count_(0)
    {
        init(
            boost::shared_ptr< std::ostream > (&std::clog, boost::null_deleter()),
            severity);
    }
    stream::stream(
        std::ostream&        stream,
        const severity_level severity)
    : logger_id_(0),
      debug_count_(0)
    {
        init(
            boost::shared_ptr< std::ostream > (&stream, boost::null_deleter()),
            severity);
    }
    stream::stream(
        const std::string    filename,
        const severity_level severity)
    : logger_id_(0),
      filename_(filename),
      debug_count_(0)
    {
        init(
            boost::make_shared< std::ofstream > (filename_),
            severity);
    }
    stream::~stream()
    {
        boost::log::core::get()->remove_sink(sink_);
    }

    void stream::set_severity(
            const severity_level severity)
    {
        // this must be done before updating severity_
        set_auto_flush(severity);

        severity_ = severity;

        // update the filter with the new severity threshold
        sink_->set_filter(logger_id_attr == logger_id_ && (*min_severity_ || severity_attr >= severity_));
    }

    severity_level stream::severity(void)
    {
        return severity_;
    }

    void stream::set_channel_severity(
            const std::string    channel,
            const severity_level severity)
    {
        // this must be done before updating severity_
        set_auto_flush(channel, severity);

        (*min_severity_)[channel] = severity;
        severity_map_[channel]    = severity;

        // update the filter with the new severity threshold
        sink_->set_filter(logger_id_attr == logger_id_ && (*min_severity_ || severity_attr >= severity_));
    }

    void stream::set_logger_id(uint64_t id)
    {
        logger_id_ = id;
    }

    void stream::flush(void)
    {
        sink_->flush();
    }

    /* private methods */
    void stream::init(
            boost::shared_ptr< std::ostream > stream,
            const severity_level              severity)
    {
        stream_   = stream;
        severity_ = severity;

        // Create a minimal severity table filter
        min_severity_ = new min_severity_filter(boost::log::expressions::channel_severity_filter(channel_attr, severity_attr));

        // Create a backend and attach a couple of streams to it
        backend_ = boost::make_shared< boost::log::sinks::text_ostream_backend >();

        if (severity_level::debug == severity_) {
            backend_->auto_flush(true);
        } else {
            backend_->auto_flush(false);
        }

        backend_->add_stream(stream_);

        // Construct the sink
        sink_ = boost::make_shared< text_sink >(backend_);

        sink_->set_formatter
        (
            boost::log::expressions::stream
            << line_id_attr
            << ": <" << severity_attr
            << "> [" << channel_attr << "] "
            << boost::log::expressions::smessage
        );

        // set a filter in this sink
        sink_->set_filter(logger_id_attr == logger_id_ && (*min_severity_ || severity_attr >= severity_));

        boost::shared_ptr< boost::log::core > core = boost::log::core::get();
        core->add_sink(sink_);

        boost::log::add_common_attributes();
    }

    void stream::set_auto_flush(
            const severity_level severity)
    {
        if (severity_ != severity) {
            if (severity_level::debug == severity) {
                debug_count_++;
            } else {
                debug_count_--;
            }
        }
        if (debug_count_ > 0) {
            // Enable auto-flushing after each log record written
            backend_->auto_flush(true);
        }
    }
    void stream::set_auto_flush(
            const std::string    channel,
            const severity_level severity)
    {
        if ((severity_map_.find(channel) == severity_map_.end()) && (severity_level::debug == severity)) {
            debug_count_++;
        } else {
            if (severity_map_[channel] != severity) {
                if (severity_level::debug == severity) {
                    debug_count_++;
                } else {
                    debug_count_--;
                }
            }
        }
        if (debug_count_ > 0) {
            // Enable auto-flushing after each log record written
            backend_->auto_flush(true);
        }
    }

    std::ostream& operator<< (std::ostream& strm, severity_level severity)
    {
        static const char* strings[] =
        {
            "DEBUG",
            "INFO",
            "WARN",
            "ERROR",
            "FATAL"
        };

        if (static_cast< std::size_t >(severity) < sizeof(strings) / sizeof(*strings))
            strm << strings[severity];
        else
            strm << static_cast< int >(severity);

        return strm;
    }
}
