// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/**
 * @file base_transport.cpp
 *
 * @brief base_transport.cpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Jul 07, 2015
 */


#include "nnti/nnti_pch.hpp"

#include "nnti/nntiConfig.h"

#include <string>

#include "common/Configuration.hh"
#include "common/NodeID.hh"

#include "webhook/Server.hh"

#include "nnti/nnti_transport.hpp"
#include "nnti/transports/base/base_transport.hpp"
#include "nnti/nnti_buffer.hpp"
#include "nnti/nnti_logger.hpp"
#include "nnti/nnti_peer.hpp"
#include "nnti/nnti_wid.hpp"


namespace nnti {
namespace transports {

/**
 * @brief Initialize NNTI to use a specific transport.
 *
 * \param[in]  trans_id  The ID of the transport the client wants to use.
 * \param[in]  my_url    A string that describes the transport parameters.
 * \return A result code (NNTI_OK or an error)
 *
 */
base_transport::base_transport(
    const NNTI_transport_id_t        trans_id,
    const nnti::datatype::nnti_peer &me)
    : trans_id_(trans_id),
      url_(me.url()),
      me_(me)
{
    faodel::Configuration config;
    init_logger(config);

    log_debug_stream("base_transport") << "me_ = " << me_.url().url();

    return;
}

/**
 * @brief Initialize NNTI to use a specific transport.
 *
 * \param[in]  trans_id  The ID of the transport the client wants to use.
 * \return A result code (NNTI_OK or an error)
 *
 */
base_transport::base_transport(
    const NNTI_transport_id_t  trans_id,
    faodel::Configuration    &config)
    : trans_id_(trans_id)
{
    faodel::rc_t rc;

    std::string protocol;
    std::string addr;
    std::string port;

    config_ = config;

    rc = config_.GetString(&protocol, "nnti.transport.protocol");
    assert(rc==0 && "transport factory didn't set nnti.transport.protocol");

    init_logger(config);

    faodel::nodeid_t nodeid = webhook::Server::GetNodeID();
    addr = nodeid.GetIP();
    port = nodeid.GetPort();

    url_ = nnti::core::nnti_url(addr, port);

    me_  = nnti::datatype::nnti_peer(this, url_);

    log_debug_stream("base_transport") << "me_ = " << me_.url().url();

    return;
}

/**
 * @brief Deactivates a specific transport.
 *
 * \return A result code (NNTI_OK or an error)
 */
base_transport::~base_transport()
{
    nnti::core::logger::fini();

    return;
}

void
base_transport::init_logger(
    faodel::Configuration &config)
{
    faodel::rc_t rc;

    std::string log_level_str;
    rc = config.GetString(&log_level_str, "nnti.logger.severity", "warning");

    sbl::severity_level log_level = sbl::severity_level::warning;
    if (log_level_str == "debug") {
        log_level = sbl::severity_level::debug;
    } else if (log_level_str == "info") {
        log_level = sbl::severity_level::info;
    } else if ((log_level_str == "warn") || (log_level_str == "warning")) {
        log_level = sbl::severity_level::warning;
    } else if (log_level_str == "error") {
        log_level = sbl::severity_level::error;
    } else if (log_level_str == "fatal") {
        log_level = sbl::severity_level::fatal;
    }

    std::string logfile_str;
    rc = config.GetString(&logfile_str, "nnti.logger.filename");
    if (rc == 0) {
        size_t index;

        index = faodel::ToLowercase(logfile_str).find("%h", 0);
        if (index != std::string::npos) {
            char my_hostname[NNTI_HOSTNAME_LEN];
            gethostname(my_hostname, NNTI_HOSTNAME_LEN);
            logfile_str.replace(index, 2, my_hostname);
        }

        index = faodel::ToLowercase(logfile_str).find("%p", 0);
        if (index != std::string::npos) {
            int mypid = getpid();
            logfile_str.replace(index, 2, std::to_string(mypid));
        }
        nnti::core::logger::init(logfile_str, true, log_level);
    } else {
        nnti::core::logger::init(true, log_level);
    }
}

NNTI_transport_id_t
base_transport::id(void)
{
    return trans_id_;
}

/**
 * @brief Calculate the number of bytes required to store an encoded NNTI data structure.
 *
 * \param[in]  nnti_dt     The NNTI data structure cast to void*.
 * \param[out] packed_len  The number of bytes required to store the encoded data structure.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t
base_transport::dt_sizeof(
    void     *nnti_dt,
    uint64_t *packed_len)
{
    NNTI_result_t                  rc = NNTI_OK;
    nnti::datatype::nnti_datatype *dt = (nnti::datatype::nnti_datatype *)nnti_dt;

    switch (dt->datatype()) {
        case NNTI_dt_buffer:
            log_debug("base_transport", "dt is a buffer");
            *packed_len  = ((nnti::datatype::nnti_buffer*)dt)->packed_size();
            break;
        case NNTI_dt_peer:
            log_debug("base_transport", "dt is a peer");
            *packed_len  = ((nnti::datatype::nnti_peer*)dt)->packed_size();
            break;
        default:
            // unsupported datatype
            rc = NNTI_EINVAL;
            break;
    }

    log_debug("base_transport", "dt_sizeof(packed_len=%lu): rc=%d", *packed_len, rc);

    return(rc);
}

/**
 * @brief Encode an NNTI data structure into an array of bytes.
 *
 * \param[in]  nnti_dt        The NNTI data structure cast to void*.
 * \param[out] packed_buf     A array of bytes to store the encoded data structure.
 * \param[in]  packed_buflen  The length of packed_buf.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t
base_transport::dt_pack(
    void           *nnti_dt,
    char           *packed_buf,
    const uint64_t  packed_buflen)
{
    NNTI_result_t                  rc = NNTI_OK;
    nnti::datatype::nnti_datatype *dt = (nnti::datatype::nnti_datatype *)nnti_dt;

    switch (dt->datatype()) {
        case NNTI_dt_buffer:
            log_debug("base_transport", "dt is a buffer");
            rc  = ((nnti::datatype::nnti_buffer*)dt)->pack(packed_buf, packed_buflen);
            break;
        case NNTI_dt_peer:
            log_debug("base_transport", "dt is a peer");
            rc  = ((nnti::datatype::nnti_peer*)dt)->pack(packed_buf, packed_buflen);
            break;
        default:
            // unsupported datatype
            rc = NNTI_EINVAL;
            break;
    }

    return(rc);
}

/**
 * @brief Free a variable size NNTI datatype that was unpacked with NNTI_dt_unpack().
 *
 * \param[in]  nnti_dt        The NNTI data structure cast to void*.
 */
NNTI_result_t
base_transport::dt_free(
    void *nnti_dt)
{
    NNTI_result_t                  rc = NNTI_OK;
    nnti::datatype::nnti_datatype *dt = (nnti::datatype::nnti_datatype *)nnti_dt;

    switch (dt->datatype()) {
        case NNTI_dt_buffer:
            log_debug("base_transport", "dt is a buffer");
            rc  = ((nnti::datatype::nnti_buffer*)dt)->free_packable();
            break;
        case NNTI_dt_peer:
            log_debug("base_transport", "dt is a peer");
            rc  = ((nnti::datatype::nnti_peer*)dt)->free_packable();
            break;
        default:
            // unsupported datatype
            rc = NNTI_EINVAL;
            break;
    }

    return(rc);
}

} /* namespace transports */
} /* namespace nnti */
