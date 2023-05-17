// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/**
 * @file nnti_peer.hpp
 *
 * @brief nnti_peer.hpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Aug 13, 2015
 */


#ifndef NNTI_PEER_HPP
#define NNTI_PEER_HPP

#include "nnti/nntiConfig.h"

#include "nnti/nnti_serialize.hpp"
#include "nnti/nnti_datatype.hpp"
#include "nnti/nnti_url.hpp"


namespace nnti  {

    namespace core {
        // forward declaration
        class nnti_connection;
    }

namespace datatype {

class nnti_peer
: public nnti_datatype {
private:

protected:
    nnti::core::nnti_url   url_;
    NNTI_peer_p_t          packable_;
    const static uint64_t  max_packed_size_=256;
    char                   packed_[max_packed_size_];
    uint64_t               packed_size_;

    nnti::core::nnti_connection *conn_;

public:
    nnti_peer(void)
    : nnti_datatype()
    {
        return;
    }
    nnti_peer(
        nnti::transports::transport *transport)
    : nnti_datatype(transport,
                    NNTI_dt_peer)
    {
        return;
    }
    nnti_peer(
        nnti::transports::transport *transport,
        const std::string           &url)
    : nnti_datatype(transport,
                    NNTI_dt_peer),
      url_(url)
    {
        return;
    }
    nnti_peer(
        nnti::transports::transport *transport,
        const nnti::core::nnti_url  &url)
    : nnti_datatype(transport,
                    NNTI_dt_peer),
      url_(url)
    {
        return;
    }
    nnti_peer(
        nnti::transports::transport *transport,
        char                        *packed_buf,
        const uint64_t               packed_len)
    : nnti_datatype(transport,
                    NNTI_dt_peer)
    {
        unpack(packed_buf, packed_len);
        url_ = nnti::core::nnti_url(packable_.pid);

        log_debug_stream("nnti_peer") << "nnti_peer.url == " << url_;

        return;
    }

    ~nnti_peer() override
    {
        return;
    }

    bool operator==(const nnti_peer &rhs)
    {
        return (this->packable_.pid == rhs.packable_.pid);
    }

    bool operator!=(const nnti_peer &rhs)
    {
        return !(*this == rhs);
    }

    nnti::core::nnti_url&
    url(void)
    {
        return url_;
    }

    const nnti::core::nnti_url&
    url(void) const
    {
        return url_;
    }

    NNTI_process_id_t
    pid(void)
    {
        return packable_.pid;
    }

    virtual void
    conn(
        nnti::core::nnti_connection *conn)
    {
        conn_ = conn;
    }
    virtual nnti::core::nnti_connection *
    conn(void)
    {
        return conn_;
    }

    uint64_t
    packed_size(void)
    {
        if (packed_size_ == 0) {
            packed_size_ = nnti::serialize::packed_peer_size(&packable_);
        }
        return packed_size_;
    }

    NNTI_result_t
    pack(
        char           *packed_buf,
        const uint64_t  packed_buflen)
    {
        return nnti::serialize::pack_peer(&packable_, &packed_buf[0], packed_buflen, &packed_size_);
    }

    NNTI_result_t
    unpack(
        char           *packed_buf,
        const uint64_t  packed_buflen)
    {
        packed_size_ = packed_buflen;
        memcpy(&packed_[0], &packed_buf[0], packed_buflen);

        return nnti::serialize::unpack_peer(&packable_, packed_buf, packed_buflen);

    }

    NNTI_result_t
    free_packable(void)
    {
        return nnti::serialize::free_peer(&packable_);
    }

    static inline nnti_peer*
    to_obj(
        NNTI_peer_t peer_hdl)
    {
        return (nnti_peer *)peer_hdl;
    }

    static inline NNTI_peer_t
    to_hdl(
        nnti_peer *peer)
    {
        return (NNTI_peer_t)peer;
    }
};

} /* namespace datatype */
} /* namespace nnti */

#endif /* NNTI_PEER_HPP */
