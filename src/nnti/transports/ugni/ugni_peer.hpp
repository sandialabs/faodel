// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#ifndef UGNI_PEER_HPP
#define UGNI_PEER_HPP

#include "nnti/nntiConfig.h"

#include <assert.h>

#include <deque>
#include <map>
#include <sstream>
#include <string>

#include "nnti/nnti_serialize.hpp"

#include "nnti/nnti_peer.hpp"
#include "nnti/nnti_url.hpp"


namespace nnti  {
namespace datatype {

class ugni_peer
: public nnti_peer {
private:

public:
    ugni_peer(
        nnti::transports::transport *transport,
        const std::string           &url)
    : nnti_peer(transport,
                url)
    {
        memset(&packable_, 0, sizeof(NNTI_peer_p_t));

        packable_.peer.transport_id                       = transport->id();
        packable_.peer.NNTI_remote_process_p_t_u.ugni.addr = url_.addr();
        packable_.peer.NNTI_remote_process_p_t_u.ugni.port = url_.port_as_ushort();

        packable_.pid = url_.pid();

        log_debug_stream("ugni_peer") << "ugni_peer.url == " << url_;

        return;
    }
    ugni_peer(
        nnti::transports::transport *transport,
        const nnti::core::nnti_url  &url)
    : nnti_peer(transport,
                url)
    {
        memset(&packable_, 0, sizeof(NNTI_peer_p_t));

        packable_.peer.transport_id                       = transport->id();
        packable_.peer.NNTI_remote_process_p_t_u.ugni.addr = url_.addr();
        packable_.peer.NNTI_remote_process_p_t_u.ugni.port = url_.port_as_ushort();

        packable_.pid = url_.pid();

        log_debug_stream("ugni_peer") << "ugni_peer.url == " << url_;

        return;
    }
    ugni_peer(
        nnti::transports::transport *transport,
        const std::string            name,
        const NNTI_ip_addr           addr,
        const NNTI_tcp_port          port)
    : nnti_peer(transport,
                nnti::core::nnti_url(name, port))
    {
        memset(&packable_, 0, sizeof(NNTI_peer_p_t));

        packable_.peer.transport_id                       = transport->id();
        packable_.peer.NNTI_remote_process_p_t_u.ugni.addr = url_.addr();
        packable_.peer.NNTI_remote_process_p_t_u.ugni.port = url_.port_as_ushort();

        packable_.pid = url_.pid();

        log_debug_stream("ugni_peer") << "ugni_peer.url == " << url_;

        return;
    }
    virtual ~ugni_peer()
    {
        return;
    }
};

} /* namespace datatype */
} /* namespace nnti */

#endif /* UGNI_PEER_HPP */
