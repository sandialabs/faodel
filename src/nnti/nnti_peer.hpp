// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

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

#include "nnti/nnti_packable.h"
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
    nnti::core::nnti_url url_;
    NNTI_peer_p_t        packable_;

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
    virtual ~nnti_peer()
    {
        return;
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
        uint64_t packed_len=0;

        xdrproc_t sizeof_fn = (xdrproc_t)&xdr_NNTI_peer_p_t;

        packed_len  = xdr_sizeof(sizeof_fn, &packable_);
        packed_len += sizeof(NNTI_datatype_t);

        return packed_len;
    }

    NNTI_result_t
    pack(
        char           *packed_buf,
        const uint64_t  packed_buflen)
    {
        XDR              encode_xdrs;
        xdrproc_t        encode_fn   = (xdrproc_t)&xdr_NNTI_peer_p_t;
        uint64_t         bytes_left  = packed_buflen;

        *(NNTI_datatype_t*)packed_buf = NNTI_dt_peer;

        packed_buf += sizeof(NNTI_datatype_t);
        bytes_left -= sizeof(NNTI_datatype_t);

        xdrmem_create(
                &encode_xdrs,
                packed_buf,
                bytes_left,
                XDR_ENCODE);

        if (!encode_fn(XDRPROC_ARGS(&encode_xdrs, &packable_))) {
            log_fatal("nnti_peer", "packing failed");
            return NNTI_EENCODE;
        }

        return NNTI_OK;
    }

    NNTI_result_t
    unpack(
        char           *packed_buf,
        const uint64_t  packed_len)
    {
        XDR              decode_xdrs;
        xdrproc_t        decode_fn   = (xdrproc_t)&xdr_NNTI_peer_p_t;
        NNTI_datatype_t *dt          = (NNTI_datatype_t*)packed_buf;
        uint64_t         dt_size     = sizeof(NNTI_peer_p_t);
        uint64_t         bytes_left  = packed_len;

        packed_buf += sizeof(NNTI_datatype_t);
        bytes_left -= sizeof(NNTI_datatype_t);

        memset(&packable_, 0, dt_size);
        xdrmem_create(
                &decode_xdrs,
                packed_buf,
                bytes_left,
                XDR_DECODE);

        if (!decode_fn(XDRPROC_ARGS(&decode_xdrs, &packable_))) {
            log_fatal("nnti_peer", "unpacking failed");
            return NNTI_EDECODE;
        }

        return NNTI_OK;
    }

    NNTI_result_t
    free_packable(void)
    {
        xdrproc_t free_fn = (xdrproc_t)&xdr_NNTI_peer_p_t;

        xdr_free(free_fn, (char*)&packable_);

        return NNTI_OK;
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
