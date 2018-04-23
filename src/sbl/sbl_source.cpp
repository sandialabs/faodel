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

#include "sbl_source.hpp"


namespace sbl  {

    source::source(
        severity_level  severity)
    : logger_id_(0),
      logger_id_attr_(logger_id_),
      severity_(severity)
    {
        boostlogger_.add_attribute("LoggerID", logger_id_attr_);
    }
    source::source(
        uint64_t       logger_id,
        severity_level severity)
    : logger_id_(logger_id),
      logger_id_attr_(logger_id_),
      severity_(severity)
    {
        boostlogger_.add_attribute("LoggerID", logger_id_attr_);
    }
    source::~source()
    {
    }

    sbl_logger& source::boostlogger(void)
    {
        return(boostlogger_);
    }

    sbl_record_pump source::make_record_pump(boost::log::record &rec)
    {
        return sbl_record_pump(boostlogger_, rec);
    }

    severity_level source::severity(void)
    {
        return(severity_);
    }

    void source::set_logger_id(uint64_t id)
    {
        logger_id_ = id;
        logger_id_attr_.set(logger_id_);
    }

    void source::log(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        ...)
    {
        va_list params;

        va_start(params, msg);

        log(
            channel,
            func_name,
            file_name,
            line_num,
            msg,
            params);

        va_end(params);

        return;
    }

    void source::log(
        const char *channel,
        const char *func_name,
        const char *file_name,
        const int   line_num,
        const char *msg,
        va_list     params)
    {
        const char *file;
        char buf1[256];
        char buf2[256];

        /* path from last '/' */
        file = strrchr(file_name, '/');

#ifdef SBL_HAVE_GETTID
        sprintf(buf1, "[%s:%s:%d:t%lu]: ",
                func_name,
                (file == NULL) ? file_name : &(file[1]),
                line_num,
                syscall(SYS_gettid));
#else
        sprintf(buf1, "[%s:%s:%d]: ",
                func_name,
                (file == NULL) ? file_name : &(file[1]),
                line_num);
#endif

        vsprintf(buf2, msg, params);

        output(
            channel,
            buf1,
            buf2);

        return;
    }

    void source::log(
        const char *channel,
        const char *prefix,
        const char *msg)
    {
        output(
            channel,
            prefix,
            msg);

        return;
    }

    void source::log(
        const char *channel,
        const char *msg)
    {
        output(
            channel,
            msg);

        return;
    }

    void source::log(
        const char *channel,
        const char *msg,
        va_list     params)
    {
        output(
            channel,
            msg,
            params);

        return;
    }

    /* private methods */
    void source::output(
            const char *channel,
            const char *msg)
    {
        BOOST_LOG_CHANNEL_SEV(boostlogger_, channel, severity_) << msg;

        return;
    }

    void source::output(
            const char *channel,
            const char *prefix,
            const char *msg)
    {
        BOOST_LOG_CHANNEL_SEV(boostlogger_, channel, severity_) << prefix << msg;

        return;
    }

    void source::output(
            const char *channel,
            const char *msg,
            va_list     params)
    {
        char buf[256];

        vsprintf(buf, msg, params);

        BOOST_LOG_CHANNEL_SEV(boostlogger_, channel, severity_) << buf;

        return;
    }

    void source::output(
            const char *channel,
            const char *prefix,
            const char *msg,
            va_list     params)
    {
        char buf[256];

        vsprintf(buf, msg, params);

        BOOST_LOG_CHANNEL_SEV(boostlogger_, channel, severity_) << prefix << buf;

        return;
    }

}
