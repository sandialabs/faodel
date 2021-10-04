// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/*
 *  @file: nnti_url.cpp
 *
 *  Created on: Sep 18, 2015
 *      Author: thkorde
 */

#include <errno.h>
#include <netdb.h>

#include <mutex>
#include <cstring>
#include <sstream>

#include <boost/lexical_cast.hpp>

#include "nnti/nnti_url.hpp"

#include "nnti/nnti_serialize.hpp"
#include "nnti/nnti_logger.hpp"
#include "nnti/nnti_pid.hpp"
#include "nnti/nnti_types.h"

namespace nnti  {
namespace core {

std::mutex nnti_url::hostent_mutex_;

NNTI_result_t
nnti_url::parse(void)
{
    const std::string prot_pattern("http://");
    const char        path_start_pattern  = '/';
    const char        port_start_pattern  = ':';

    size_t last_pos = url_.length();

    size_t prot_start_pos = 0;
    size_t prot_end_pos   = url_.find(prot_pattern);

    log_debug("nnti_url", "proto_start_pos=%d  proto_end_pos=%d", prot_start_pos, prot_end_pos);

    if (std::string::npos == prot_end_pos) {
        return NNTI_EINVAL;
    }

    size_t hostname_start_pos = 7;  // the proto is now fixed at 'nnti://'

    size_t path_start_pos = url_.find(path_start_pattern, hostname_start_pos); // string::npos, if not found
    if (std::string::npos == path_start_pos) {
        path_start_pos = last_pos;
    }

    size_t port_start_pos = url_.rfind(port_start_pattern, path_start_pos);

    log_debug("nnti_url", "hostname_start_pos=%d  port_start_pos=%d", hostname_start_pos, port_start_pos);

    if ((std::string::npos == port_start_pos) || (port_start_pos < hostname_start_pos)) {
        hostname_ = url_.substr(hostname_start_pos, path_start_pos-hostname_start_pos);
    } else {
        hostname_ = url_.substr(hostname_start_pos, port_start_pos-hostname_start_pos);
    }

    if ((std::string::npos != port_start_pos) && (port_start_pos > hostname_start_pos)) {
        if (url_[path_start_pos] == '/') {
            port_ = url_.substr(port_start_pos+1, path_start_pos-port_start_pos-1);
        } else {
            port_ = url_.substr(port_start_pos+1, path_start_pos-port_start_pos);
        }
    }

    return NNTI_OK;
}

void
nnti_url::hostname2addr(void)
{
    struct hostent     *host_entry;

    std::lock_guard<std::mutex> lock(hostent_mutex_);
    host_entry = gethostbyname(hostname_.c_str());
    if (!host_entry) {
        log_warn("nnti_url", "failed to resolve hostname (%s): %s", hostname_.c_str(), strerror(errno));
        return;
    }
    addr_ = ntohl( *((int *)host_entry->h_addr_list[0]) );
}


nnti_url::nnti_url(void)
{
    return;
}

nnti_url::nnti_url(
    const std::string &url)
: url_(url),
  hostname_(""),
  port_("")
{
    parse();
    hostname2addr();
    pid_ = nnti::datatype::nnti_pid::to_pid(*this);
}

nnti_url::nnti_url(
    const NNTI_process_id_t pid)
: hostname_(""),
  port_(""),
  pid_(pid)
{
    url_ = nnti::datatype::nnti_pid::to_url(pid);

    parse();
}

nnti_url::nnti_url(
    const std::string &hostname,
    const std::string &port)
: hostname_(hostname),
  port_(port)
{
    std::stringstream ss;
    ss << "http://" << hostname_ << ":" << port_ << "/";
    url_ = ss.str();

    hostname2addr();
    pid_ = nnti::datatype::nnti_pid::to_pid(*this);
    log_debug("nnti_url", "url.url_=%s  url.pid_=%016lx", url_.c_str(), pid_);
}

nnti_url::nnti_url(
    const std::string   &hostname,
    const NNTI_tcp_port  port)
: nnti_url(hostname, std::to_string(port))
{
    pid_ = nnti::datatype::nnti_pid::to_pid(*this);
}

bool
nnti_url::has_port(void) const
{
    return (!port_.empty());
}

const std::string&
nnti_url::url(void) const
{
    return url_;
}

NNTI_process_id_t
nnti_url::pid(void) const
{
    return pid_;
}

const std::string&
nnti_url::hostname(void) const
{
    return hostname_;
}

NNTI_ip_addr
nnti_url::addr(void) const
{
    return addr_;
}

const std::string&
nnti_url::port(void) const
{
    return port_;
}

NNTI_tcp_port
nnti_url::port_as_ushort(void) const
{
    NNTI_tcp_port p = 0;

    if (has_port()) {
        try {
            p = boost::lexical_cast<uint16_t>(port_);
        }
        catch (boost::bad_lexical_cast &) {
            // p is already 0.  do nothing.
            // p = 0;
        }
    }

    return p;
}

std::ostream&
operator<< (
    std::ostream   &os,
    const nnti_url &obj)
{
    os << obj.url() << " => " << obj.hostname() << " + " << obj.port() << "(" << obj.port_as_ushort() << ")";
    return os;
}

} /* namespace core */
} /* namespace nnti */
