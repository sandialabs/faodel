// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/**
 * @file transport_factory.cpp
 *
 * @brief transport_factory.cpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: August 03, 2015
 */


#include "nnti/nnti_pch.hpp"

#include <string>
#include <sstream>
#include <stdexcept>

#include "faodel-common/Configuration.hh"

#include "nnti/nnti_transport.hpp"
#include "nnti/transports/null/null_transport.hpp"

#if (NNTI_BUILD_IBVERBS==1)
#include "nnti/transports/ibverbs/ibverbs_transport.hpp"
#endif
#if (NNTI_BUILD_MPI==1)
#include "nnti/transports/mpi/mpi_transport.hpp"
#endif
#if (NNTI_BUILD_UGNI==1)
#include "nnti/transports/ugni/ugni_transport.hpp"
#endif

#include "nnti/transport_factory.hpp"


/** @addtogroup nnti_impl
 *  @{
 */

namespace nnti  {
namespace transports {

std::map<NNTI_transport_id_t, transport *> factory::transport_map_;

factory::~factory()
{
    return;
}

transport *
factory::get_instance(
    const NNTI_transport_id_t &trans_id)
{
    faodel::Configuration config;
    return get_instance(trans_id, config);
}
transport *
factory::get_instance(
    const std::string &trans_name)
{
    faodel::Configuration config;
    return get_instance(trans_name, config);
}
transport *
factory::get_instance(
    const NNTI_transport_id_t &trans_id,
    faodel::Configuration    &config)
{
    config.Set("nnti.transport.id", trans_id);
    return get_instance(config);
}
transport *
factory::get_instance(
    const std::string      &trans_name,
    faodel::Configuration &config)
{
    config.Set("nnti.transport.name", trans_name);
    return get_instance(config);
}

transport *
factory::get_instance(
    faodel::Configuration &config)
{
    transport           *t        = nullptr;
    NNTI_transport_id_t  trans_id = NNTI_DEFAULT_TRANSPORT;
    std::string          proto("");
    std::string          trans_name("");
    std::string          id_key("nnti.transport.id");
    std::string          proto_key("nnti.transport.protocol");
    std::string          name_key("nnti.transport.name");
    std::string          name_key2("net.transport.name");
    
    if (0 != config.GetInt((int64_t*)&trans_id, id_key)) {
        if ( (0 != config.GetLowercaseString(&trans_name, name_key2)) &&
             (0 != config.GetLowercaseString(&trans_name, name_key))   ) {
            trans_id = NNTI_DEFAULT_TRANSPORT;
            config.Set(id_key, trans_id);
        } else {
            if (trans_name == "null") {
                trans_id = NNTI_TRANSPORT_NULL;
            } else if (trans_name == "ibverbs") {
                trans_id = NNTI_TRANSPORT_IBVERBS;
            } else if (trans_name == "mpi") {
                trans_id = NNTI_TRANSPORT_MPI;
            } else if (trans_name == "ugni") {
                trans_id = NNTI_TRANSPORT_UGNI;
            } else {
              throw std::runtime_error("NNTI does not recognize transport.name '"+trans_name+"' in Configuration");
            }
            config.Set(id_key, trans_id);
        }
    }

    if (trans_id == NNTI_TRANSPORT_NULL) {
        trans_name = "null";
        proto = "null";
        config.Set(name_key, trans_name);
        config.Set(proto_key, proto);
        t = null_transport::get_instance(config);
    }
    if (trans_id == NNTI_TRANSPORT_IBVERBS) {
#if (NNTI_BUILD_IBVERBS==1)
        trans_name = "ibverbs";
        proto = "ib";
        config.Set(name_key, trans_name);
        config.Set(proto_key, proto);
        t = ibverbs_transport::get_instance(config);
#else
        // ibverbs is not configured.  try failing back to MPI.
        std::stringstream ss;
        ss<<
        "------------------------------------------------------------------\n"
        "The FAODEL_CONFIG 'net.transport.name' key is set to 'ibverbs'.\n"
        "The 'ibverbs' transport was not configured into the Faodel network\n"
        "library.  The 'mpi' transport will be used instead.\n"
        "------------------------------------------------------------------";
        std::cerr<<ss.str()<<std::endl;
        trans_id = NNTI_TRANSPORT_MPI;
#endif
    }
    if (trans_id == NNTI_TRANSPORT_UGNI) {
#if (NNTI_BUILD_UGNI==1)
        trans_name = "ugni";
        proto = "ugni";
        config.Set(name_key, trans_name);
        config.Set(proto_key, proto);
        t = ugni_transport::get_instance(config);
#else
        // ugni is not configured.  try failing back to MPI.
        std::stringstream ss;
        ss<<
        "------------------------------------------------------------------\n"
        "The FAODEL_CONFIG 'net.transport.name' key is set to 'ugni'.\n"
        "The 'ugni' transport was not configured into the Faodel network\n"
        "library.  The 'mpi' transport will be used instead.\n"
        "------------------------------------------------------------------";
        std::cerr<<ss.str()<<std::endl;
        trans_id = NNTI_TRANSPORT_MPI;
#endif
    }
    if (trans_id == NNTI_TRANSPORT_MPI) {
#if (NNTI_BUILD_MPI==1)
        trans_name = "mpi";
        proto = "mpi";
        config.Set(name_key, trans_name);
        config.Set(proto_key, proto);
        t = mpi_transport::get_instance(config);
#else
        // mpi is not configured.  there is no fallback.
        std::stringstream ss;
        ss<<
        "------------------------------------------------------------------\n"
        "The FAODEL_CONFIG 'net.transport.name' key is set to 'mpi'.\n"
        "The 'mpi' transport was not configured into the Faodel network\n"
        "library.  There is no fallback.  Aborting.\n"
        "------------------------------------------------------------------";
        std::cerr<<ss.str()<<std::endl;
        abort();
#endif
    }

    transport_map_[trans_id] = t;

    return t;
}

bool
factory::exists(
    const NNTI_transport_id_t &trans_id)
{
    return (transport_map_.find(trans_id) != transport_map_.end());
}

} /* namespace transports */
} /* namespace nnti */


/** @} */
