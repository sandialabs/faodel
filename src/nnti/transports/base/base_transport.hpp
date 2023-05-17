// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/**
 * @file base_transport.hpp
 *
 * @brief base_transport.hpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Jul 07, 2015
 */

#ifndef BASE_TRANSPORT_HPP_
#define BASE_TRANSPORT_HPP_

#include <string>

#include "faodel-common/Configuration.hh"

#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_peer.hpp"
#include "nnti/nnti_url.hpp"


/** @addtogroup nnti_impl
 *  @{
 */

namespace nnti  {
namespace transports {

class base_transport
: public transport {
protected:
    NNTI_transport_id_t       trans_id_;
    nnti::core::nnti_url      url_;
    nnti::datatype::nnti_peer me_;
    uint32_t                  fingerprint_;
    faodel::Configuration     config_;

public:
    /**
     * @brief Initialize NNTI to use a specific transport.
     *
     * \param[in]  trans_id  The ID of the transport the client wants to use.
     * \param[in]  me        An nnti_peer object that references this process.
     * \return A result code (NNTI_OK or an error)
     *
     */
    base_transport(
        const NNTI_transport_id_t        trans_id,
        const nnti::datatype::nnti_peer &me);

    /**
     * @brief Initialize NNTI to use a specific transport.
     *
     * \param[in]  trans_id  The ID of the transport the client wants to use.
     * \param[in]  config    A Configuration object that NNTI should use to configure itself.
     * \return A result code (NNTI_OK or an error)
     *
     */
    base_transport(
        const NNTI_transport_id_t  trans_id,
        faodel::Configuration     &config);

    /**
     * @brief Deactivates a specific transport.
     *
     * \return A result code (NNTI_OK or an error)
     */
    virtual ~base_transport();

    void
    init_logger(faodel::Configuration &config);

    NNTI_transport_id_t
    id(void) override;

    /**
     * @brief Calculate the number of bytes required to store an encoded NNTI data structure.
     *
     * \param[in]  nnti_dt     The NNTI data structure cast to void*.
     * \param[out] packed_len  The number of bytes required to store the encoded data structure.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    dt_sizeof(
        void     *nnti_dt,
        uint64_t *packed_len) override;

    /**
     * @brief Encode an NNTI data structure into an array of bytes.
     *
     * \param[in]  nnti_dt        The NNTI data structure cast to void*.
     * \param[out] packed_buf     A array of bytes to store the encoded data structure.
     * \param[in]  packed_buflen  The length of packed_buf.
     * \return A result code (NNTI_OK or an error)
     */
    NNTI_result_t
    dt_pack(
        void           *nnti_dt,
        char           *packed_buf,
        const uint64_t  packed_buflen) override;

    /**
     * @brief Free a variable size NNTI datatype that was unpacked with NNTI_dt_unpack().
     *
     * \param[in]  nnti_dt        The NNTI data structure cast to void*.
     */
    NNTI_result_t
    dt_free(
        void *nnti_dt) override;

};

} /* namespace transports */
} /* namespace nnti */


/** @} */

#endif /* BASE_TRANSPORT_HPP_*/
