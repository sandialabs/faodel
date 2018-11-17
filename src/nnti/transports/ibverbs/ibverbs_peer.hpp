// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/**
 * @file ibverbs_peer.hpp
 *
 * @brief ibverbs_peer.hpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Aug 13, 2015
 */


#ifndef IBVERBS_PEER_HPP
#define IBVERBS_PEER_HPP

#include "nnti/nntiConfig.h"

#include <assert.h>

#include <deque>
#include <map>
#include <sstream>
#include <string>

#include "nnti/nnti_packable.h"

#include "nnti/nnti_peer.hpp"
#include "nnti/nnti_url.hpp"


namespace nnti  {
namespace datatype {

class ibverbs_peer
: public nnti_peer {
private:

public:
    ibverbs_peer(
        nnti::transports::transport *transport,
        const std::string           &url)
    : nnti_peer(transport,
                url)
    {
        memset(&packable_, 0, sizeof(NNTI_peer_p_t));

        packable_.peer.transport_id                      = transport->id();
        packable_.peer.NNTI_remote_process_p_t_u.ib.addr = url_.addr();
        packable_.peer.NNTI_remote_process_p_t_u.ib.port = url_.port_as_ushort();

        packable_.pid = url_.pid();

        log_debug_stream("ibverbs_peer") << "ibverbs_peer.url == " << url_;

        return;
    }
    ibverbs_peer(
        nnti::transports::transport *transport,
        const nnti::core::nnti_url  &url)
    : nnti_peer(transport,
                url)
    {
        memset(&packable_, 0, sizeof(NNTI_peer_p_t));

        packable_.peer.transport_id                      = transport->id();
        packable_.peer.NNTI_remote_process_p_t_u.ib.addr = url_.addr();
        packable_.peer.NNTI_remote_process_p_t_u.ib.port = url_.port_as_ushort();

        packable_.pid = url_.pid();

        log_debug_stream("ibverbs_peer") << "ibverbs_peer.url == " << url_;

        return;
    }
    ibverbs_peer(
        nnti::transports::transport *transport,
        const std::string            name,
        const NNTI_ip_addr           addr,
        const NNTI_tcp_port          port)
    : nnti_peer(transport,
                nnti::core::nnti_url(name, port))
    {
        memset(&packable_, 0, sizeof(NNTI_peer_p_t));

        packable_.peer.transport_id                      = transport->id();
        packable_.peer.NNTI_remote_process_p_t_u.ib.addr = url_.addr();
        packable_.peer.NNTI_remote_process_p_t_u.ib.port = url_.port_as_ushort();

        packable_.pid = url_.pid();

        log_debug_stream("ibverbs_peer") << "ibverbs_peer.url == " << url_;

        return;
    }

  ~ibverbs_peer() override {
        return;
    }
};

} /* namespace datatype */
} /* namespace nnti */

#endif /* IBVERBS_PEER_HPP */
