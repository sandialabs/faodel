// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#ifndef MPI_CONNECTION_HPP_
#define MPI_CONNECTION_HPP_

#include "nnti/nntiConfig.h"

#include <list>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <sstream>
#include <string>

#include <mpi.h>

#include "nnti/nnti_connection.hpp"
#include "nnti/nnti_util.hpp"

#include "nnti/transports/mpi/mpi_transport.hpp"
#include "nnti/transports/mpi/mpi_cmd_buffer.hpp"


namespace nnti {
namespace core {

class mpi_connection
: public nnti_connection {
private:
    struct connection_params {
        std::string hostname;
        uint32_t    addr;
        uint32_t    port;
        int         rank;

        connection_params(void) {
            return;
        }
        connection_params(const std::map<std::string,std::string> &peer) {
            for(auto &k_v : peer){
                log_debug_stream("connection_params") << "Key: " << k_v.first << " val: " << k_v.second;
            }

            try {
                hostname = peer.at("hostname");
                addr     = nnti::util::str2uint32(peer.at("addr"));
                port     = nnti::util::str2uint32(peer.at("port"));
                rank     = nnti::util::str2int32(peer.at("rank"));
            }
            catch (const std::out_of_range& oor) {
                log_error_stream("connection_params") << "Out of Range error: " << oor.what();
            }
        }
    };

private:
    nnti::transports::mpi_transport *transport_;
    connection_params                peer_params_;

public:
    mpi_connection(
        nnti::transports::mpi_transport *transport)
    : nnti_connection(),
      transport_(transport)
    {
    }
    mpi_connection(
        nnti::transports::mpi_transport         *transport,
        const std::map<std::string,std::string> &peer)
    : nnti_connection(),
      transport_(transport),
      peer_params_(peer)
    {
        int rc;

        nnti::core::nnti_url url = nnti::core::nnti_url(peer_params_.hostname, peer_params_.port);
        peer_pid_ = url.pid();
        peer_     = new nnti::datatype::mpi_peer(transport, url, peer_params_.rank);
        peer_->conn(this);

        log_debug("mpi_connection param_map", "hostname = %s",  peer_params_.hostname.c_str());
        log_debug("mpi_connection param_map", "addr     = %lu", peer_params_.addr);
        log_debug("mpi_connection param_map", "port     = %d",  peer_params_.port);
        log_debug("mpi_connection param_map", "rank     = %d",  peer_params_.rank);
    }
    mpi_connection(
        nnti::transports::mpi_transport *transport,
        const std::string               &params)
    : nnti_connection(),
      transport_(transport)
    {
        int rc;

        peer_params(params);
        nnti::core::nnti_url url = nnti::core::nnti_url(peer_params_.hostname, peer_params_.port);
        peer_ = new nnti::datatype::mpi_peer(transport, url, peer_params_.rank);
        peer_->conn(this);

        log_debug("mpi_connection param_str", "hostname = %s",  peer_params_.hostname.c_str());
        log_debug("mpi_connection param_str", "addr     = %lu", peer_params_.addr);
        log_debug("mpi_connection param_str", "port     = %d",  peer_params_.port);
        log_debug("mpi_connection param_str", "rank     = %d",  peer_params_.rank);
    }
    virtual ~mpi_connection()
    {
        return;
    }

    void
    peer_params(
        const std::map<std::string,std::string> &params)
    {
        int rc;

        peer_params_ = connection_params(params);

        nnti::core::nnti_url url = nnti::core::nnti_url(peer_params_.hostname, peer_params_.port);
        peer_pid_ = url.pid();

        log_debug("peer_params", "hostname = %s",  peer_params_.hostname.c_str());
        log_debug("peer_params", "addr     = %lu", peer_params_.addr);
        log_debug("peer_params", "port     = %d",  peer_params_.port);
        log_debug("peer_params", "rank     = %d",  peer_params_.rank);
    }

    void
    peer_params(
        const std::string &params)
    {
        int rc;
        std::map<std::string,std::string> param_map;

        std::istringstream iss(params);
        std::string line;
        while (std::getline(iss, line)) {
            std::pair<std::string,std::string> res = split_string(line, '=');
            param_map[res.first] = res.second;
        }

        peer_params_ = connection_params(param_map);

        nnti::core::nnti_url url = nnti::core::nnti_url(peer_params_.hostname, peer_params_.port);
        peer_pid_ = url.pid();

        log_debug("peer_params", "hostname = %s",  peer_params_.hostname.c_str());
        log_debug("peer_params", "addr     = %lu", peer_params_.addr);
        log_debug("peer_params", "port     = %d",  peer_params_.port);
        log_debug("peer_params", "rank     = %d",  peer_params_.rank);
    }

private:
    std::pair<std::string,std::string>
    split_string(
        const std::string &item,
        const char         delim)
    {
        size_t p1 = item.find(delim);

        if(p1==std::string::npos) return std::pair<std::string,std::string>(item,"");
        if(p1==item.size())       return std::pair<std::string,std::string>(item.substr(0,p1),"");
        if(p1==0)                 return std::pair<std::string,std::string>("",item.substr(p1+1));

        return std::pair<std::string,std::string>(item.substr(0,p1), item.substr(p1+1));
    }
};

} /* namespace core */
} /* namespace nnti */

#endif /* MPI_CONNECTION_HPP_ */
