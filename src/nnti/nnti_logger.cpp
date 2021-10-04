// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/**
 * @file nnti_logger.cpp
 *
 * @brief nnti_logger.cpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: August 04, 2015
 *
 */

#include "nnti/nnti_pch.hpp"

#include "nnti/nntiConfig.h"

#ifdef NNTI_HAVE_SYSCALL_H
#include <syscall.h>
#endif

#include "nnti/nnti_logger.hpp"

sbl::logger *nnti::core::logger::sbl_logger=nullptr;
bool         nnti::core::logger::include_file_func_line=false;


std::ostream&
operator<< (
    std::ostream       &os,
    const NNTI_event_t &event)
{
    char buf[256];

    buf[0]='\0';
#ifdef NNTI_HAVE_GETTID
        sprintf(buf, "[t%lu] ",
                syscall(SYS_gettid));
#endif

    os << buf
       << "  event=" << (void*)&event
       << "  event.trans_hdl=" << event.trans_hdl
       << "  event.type="      << event.type
       << "  event.result="    << event.result
       << "  event.wid="       << event.wid
       << "  event.op="        << event.op
       << "  event.peer="      << event.peer
       << "  event.start="     << event.start
       << "  event.offset="    << event.offset
       << "  event.length="    << event.length
       << "  event.context="   << event.context;

    return os;
}

std::ostream&
operator<< (
    std::ostream       &os,
    const NNTI_event_t *event)
{
    char buf[256];

    buf[0]='\0';
#ifdef NNTI_HAVE_GETTID
        sprintf(buf, "[t%lu] ",
                syscall(SYS_gettid));
#endif

    os << buf
       << "  event=" << (void*)event
       << "  event->trans_hdl=" << event->trans_hdl
       << "  event->type="      << event->type
       << "  event->result="    << event->result
       << "  event->wid="       << event->wid
       << "  event->op="        << event->op
       << "  event->peer="      << event->peer
       << "  event->start="     << event->start
       << "  event->offset="    << event->offset
       << "  event->length="    << event->length
       << "  event->context="   << event->context;

    return os;
}


void
log_debug_msg(
    const char *channel,
    const char *func_name,
    const char *file_name,
    const int   line_num,
    const char *msg,
    ...)
{
    va_list params;

    va_start(params, msg);

    if (nnti::core::logger::include_file_func_line) {
        nnti::core::logger::get_instance()->debug_source().log(
            channel,
            func_name,
            file_name,
            line_num,
            msg,
            params);
    } else {
        nnti::core::logger::get_instance()->debug_source().log(
            channel,
            msg,
            params);
    }

    va_end(params);

    return;
}

void
log_info_msg(
    const char *channel,
    const char *func_name,
    const char *file_name,
    const int   line_num,
    const char *msg,
    ...)
{
    va_list params;

    va_start(params, msg);

    if (nnti::core::logger::include_file_func_line) {
        nnti::core::logger::get_instance()->info_source().log(
            channel,
            func_name,
            file_name,
            line_num,
            msg,
            params);
    } else {
        nnti::core::logger::get_instance()->info_source().log(
            channel,
            msg,
            params);
    }
    va_end(params);

    return;
}

void
log_warn_msg(
    const char *channel,
    const char *func_name,
    const char *file_name,
    const int   line_num,
    const char *msg,
    ...)
{
    va_list params;

    va_start(params, msg);

    if (nnti::core::logger::include_file_func_line) {
        nnti::core::logger::get_instance()->warning_source().log(
            channel,
            func_name,
            file_name,
            line_num,
            msg,
            params);
    } else {
        nnti::core::logger::get_instance()->warning_source().log(
            channel,
            msg,
            params);
    }

    va_end(params);

    return;
}

void
log_error_msg(
    const char *channel,
    const char *func_name,
    const char *file_name,
    const int   line_num,
    const char *msg,
    ...)
{
    va_list params;

    va_start(params, msg);

    if (nnti::core::logger::include_file_func_line) {
        nnti::core::logger::get_instance()->error_source().log(
            channel,
            func_name,
            file_name,
            line_num,
            msg,
            params);
    } else {
        nnti::core::logger::get_instance()->error_source().log(
            channel,
            msg,
            params);
    }

    va_end(params);

    return;
}

void
log_fatal_msg(
    const char *channel,
    const char *func_name,
    const char *file_name,
    const int   line_num,
    const char *msg,
    ...)
{
    va_list params;

    va_start(params, msg);

    if (nnti::core::logger::include_file_func_line) {
        nnti::core::logger::get_instance()->fatal_source().log(
            channel,
            func_name,
            file_name,
            line_num,
            msg,
            params);
    } else {
        nnti::core::logger::get_instance()->fatal_source().log(
            channel,
            msg,
            params);
    }
    va_end(params);

    return;
}
