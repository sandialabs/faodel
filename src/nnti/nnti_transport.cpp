// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_pid.hpp"

namespace nnti  {
namespace transports {

/**
 * @brief Convert an NNTI URL to an NNTI_process_id_t.
 *
 * \param[in]   url  A string that describes this process's location on the network.
 * \param[out]  pid  Compact binary representation of a process's location on the network.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t
transport::dt_url_to_pid(
    const char        *url,
    NNTI_process_id_t *pid)
{
    NNTI_result_t rc=NNTI_OK;

    *pid = nnti::datatype::nnti_pid::to_pid(std::string(url));

    return(rc);
}

/**
 * @brief Convert an NNTI_process_id_t to an NNTI URL.
 *
 * \param[in]   pid     Compact binary representation of a process's location on the network.
 * \param[out]  url     A string that describes this process's location on the network.
 * \param[in]   maxlen  The maximum length of the URL that can be written into url.
 * \return A result code (NNTI_OK or an error)
 */
NNTI_result_t
transport::dt_pid_to_url(
    const NNTI_process_id_t  pid,
    char                    *url,
    const uint64_t           maxlen)
{
    NNTI_result_t rc=NNTI_OK;

    std::string s = nnti::datatype::nnti_pid::to_url(pid);

    strncpy(url, s.c_str(), maxlen);
    url[maxlen-1] = '\0';

    return(rc);
}

transport*
transport::to_obj(
    NNTI_transport_t trans_hdl)
{
    return (transport *)trans_hdl;
}

NNTI_transport_t
transport::to_hdl(
    transport *transport)
{
    return (NNTI_transport_t)transport;
}

} /* namespace transports */
} /* namespace nnti */
