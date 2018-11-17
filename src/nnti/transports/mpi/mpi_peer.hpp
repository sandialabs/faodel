// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#ifndef MPI_PEER_HPP
#define MPI_PEER_HPP

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

class mpi_peer
: public nnti_peer {
private:

public:
    mpi_peer(
        nnti::transports::transport *transport,
        const std::string           &url,
        const int                    rank)
    : nnti_peer(transport,
                url)
    {
        memset(&packable_, 0, sizeof(NNTI_peer_p_t));

        packable_.peer.transport_id                       = transport->id();
        packable_.peer.NNTI_remote_process_p_t_u.mpi.rank = rank;

        packable_.pid = url_.pid();

        log_debug_stream("mpi_peer") << "mpi_peer.url == " << url_;

        return;
    }
    mpi_peer(
        nnti::transports::transport *transport,
        const nnti::core::nnti_url  &url,
        const int                    rank)
    : nnti_peer(transport,
                url)
    {
        memset(&packable_, 0, sizeof(NNTI_peer_p_t));

        packable_.peer.transport_id                       = transport->id();
        packable_.peer.NNTI_remote_process_p_t_u.mpi.rank = rank;

        packable_.pid = url_.pid();

        log_debug_stream("mpi_peer") << "mpi_peer.url == " << url_;

        return;
    }
    mpi_peer(
        nnti::transports::transport *transport,
        const std::string            name,
        const NNTI_ip_addr           addr,
        const NNTI_tcp_port          port,
        const int                    rank)
    : nnti_peer(transport,
                nnti::core::nnti_url(name, port))
    {
        memset(&packable_, 0, sizeof(NNTI_peer_p_t));

        packable_.peer.transport_id                       = transport->id();
        packable_.peer.NNTI_remote_process_p_t_u.mpi.rank = rank;

        packable_.pid = url_.pid();

        log_debug_stream("mpi_peer") << "mpi_peer.url == " << url_;

        return;
    }

    ~mpi_peer() override {
        return;
    }

    void
    rank(int rank)
    {
        packable_.peer.NNTI_remote_process_p_t_u.mpi.rank = rank;
    }

    int
    rank(void)
    {
        return packable_.peer.NNTI_remote_process_p_t_u.mpi.rank;
    }
};

} /* namespace datatype */
} /* namespace nnti */

#endif /* MPI_PEER_HPP */
