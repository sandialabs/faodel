// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include "nnti/nntiConfig.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string>

#include "faodel-common/NodeID.hh"

#include "nnti/nnti_types.h"
#include "nnti/nnti_pid.hpp"
#include "nnti/nnti_url.hpp"


namespace nnti  {
namespace datatype {

NNTI_process_id_t
nnti_pid::to_pid(
    const nnti::core::nnti_url &url)
{
    faodel::nodeid_t nodeid(url.addr(), url.port_as_ushort());

    return (NNTI_process_id_t)nodeid.nid;
}

NNTI_process_id_t
nnti_pid::to_pid(
    const std::string &url)
{
    return nnti_pid::to_pid(nnti::core::nnti_url(url));
}

std::string
nnti_pid::to_url(
    NNTI_process_id_t pid)
{
    faodel::nodeid_t nodeid;

    nodeid.nid = pid;

    return nodeid.GetHttpLink();
}

} /* namespace datatype */
} /* namespace nnti */
