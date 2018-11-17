// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/*
 *  @file: nnti_url.hpp
 *
 *  Created on: Sep 18, 2015
 *      Author: thkorde
 */

#ifndef NNTI_URL_HPP_
#define NNTI_URL_HPP_

#include <errno.h>
#include <netdb.h>

#include <mutex>
#include <cstring>
#include <sstream>

#include <boost/lexical_cast.hpp>

#include "nnti/nnti_packable.h"
#include "nnti/nnti_logger.hpp"
#include "nnti/nnti_pid.hpp"
#include "nnti/nnti_types.h"

namespace nnti  {
namespace core {

class nnti_url {
private:

    static std::mutex hostent_mutex_;

    std::string url_;
    std::string hostname_;
    std::string port_;

    NNTI_ip_addr addr_;

    NNTI_process_id_t pid_;

private:
    NNTI_result_t
    parse(void);

    void
    hostname2addr(void);

public:
    nnti_url(void);

    nnti_url(
        const std::string &url);

    nnti_url(
        const NNTI_process_id_t pid);

    nnti_url(

        const std::string &hostname,
        const std::string &port);

    nnti_url(
        const std::string   &hostname,
        const NNTI_tcp_port  port);

    bool
    has_port(void) const;

    const std::string&
    url(void) const;

    NNTI_process_id_t
    pid(void) const;

    const std::string&

    hostname(void) const;

    NNTI_ip_addr
    addr(void) const;

    const std::string&
    port(void) const;

    NNTI_tcp_port
    port_as_ushort(void) const;

    friend std::ostream&
    operator<< (
        std::ostream   &os,
        const nnti_url &obj);
};

} /* namespace core */
} /* namespace nnti */

#endif /* NNTI_URL_HPP_ */
