// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include "nnti/nntiConfig.h"

#include <exception>

#include <nnti/nnti_serialize.hpp>
#include <nnti/nnti_logger.hpp>


namespace nnti {
namespace serialize {

NNTI_datatype_t
get_datatype(
    char           *packed_buf,
    const uint64_t  packed_buflen)
{
    NNTI_datatype_t t;

#if NNTI_USE_XDR == 1
    t = *(NNTI_datatype_t*)packed_buf;
#elif NNTI_USE_CEREAL == 1
    {
    std::istringstream iss(std::string(packed_buf, packed_buflen));
    cereal::PortableBinaryInputArchive iarchive(iss);
    iarchive( t );
    }
#endif

    return t;
}

uint64_t
packed_peer_size(
    NNTI_peer_p_t *packable)
{
    uint64_t packed_len=0;

#if NNTI_USE_XDR == 1
    xdrproc_t sizeof_fn = (xdrproc_t)&xdr_NNTI_peer_p_t;

    packed_len  = xdr_sizeof(sizeof_fn, packable);
    packed_len += sizeof(NNTI_datatype_t);
#endif

    return packed_len;
}

NNTI_result_t
pack_peer(
    NNTI_peer_p_t  *packable,
    char           *packed_buf,
    const uint64_t  packed_buflen,
    uint64_t       *packed_len)
{
#if NNTI_USE_XDR == 1
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

    if (!encode_fn(XDRPROC_ARGS(&encode_xdrs, packable))) {
        log_fatal("nnti_peer", "packing failed");
        return NNTI_EENCODE;
    }
    *packed_len = packed_peer_size(packable);
#elif NNTI_USE_CEREAL == 1
    std::ostringstream oss;
    {
        NNTI_datatype_t t = NNTI_dt_peer;
        cereal::PortableBinaryOutputArchive oarchive(oss);
        oarchive( t );
        oarchive( *packable );
    }
    *packed_len = oss.str().length();
    memcpy(packed_buf, oss.str().c_str(), *packed_len);
#endif

    return NNTI_OK;
}

NNTI_result_t
unpack_peer(
    NNTI_peer_p_t  *packable,
    char           *packed_buf,
    const uint64_t  packed_buflen)
{
#if NNTI_USE_XDR == 1
    XDR              decode_xdrs;
    xdrproc_t        decode_fn   = (xdrproc_t)&xdr_NNTI_peer_p_t;
    NNTI_datatype_t *dt          = (NNTI_datatype_t*)packed_buf;
    uint64_t         dt_size     = sizeof(NNTI_peer_p_t);
    uint64_t         bytes_left  = packed_buflen;

    packed_buf += sizeof(NNTI_datatype_t);
    bytes_left -= sizeof(NNTI_datatype_t);

    memset(packable, 0, dt_size);
    xdrmem_create(
        &decode_xdrs,
        packed_buf,
        bytes_left,
        XDR_DECODE);

    if (!decode_fn(XDRPROC_ARGS(&decode_xdrs, packable))) {
        log_fatal("nnti_peer", "unpacking failed");
        return NNTI_EDECODE;
    }
#elif NNTI_USE_CEREAL == 1
    {
        NNTI_datatype_t t;
        std::istringstream iss(std::string(packed_buf, packed_buflen));
        cereal::PortableBinaryInputArchive iarchive(iss);
        iarchive( t );
        iarchive( *packable );
    }
#endif
    return NNTI_OK;
}

NNTI_result_t
free_peer(
    NNTI_peer_p_t *packable)
{
#if NNTI_USE_XDR == 1
    xdrproc_t free_fn = (xdrproc_t)&xdr_NNTI_peer_p_t;
    xdr_free(free_fn, (char*)packable);
#endif
    return NNTI_OK;
}

uint64_t
packed_buffer_size(
    NNTI_buffer_p_t *packable)
{
    uint64_t packed_len=0;

#if NNTI_USE_XDR == 1
    xdrproc_t sizeof_fn = (xdrproc_t)&xdr_NNTI_buffer_p_t;

    packed_len  = xdr_sizeof(sizeof_fn, packable);
    packed_len += sizeof(NNTI_datatype_t);
#endif

    return packed_len;
}

NNTI_result_t
pack_buffer(
    NNTI_buffer_p_t  *packable,
    char             *packed_buf,
    const uint64_t    packed_buflen,
    uint64_t         *packed_len)
{
#if NNTI_USE_XDR == 1
    XDR              encode_xdrs;
    xdrproc_t        encode_fn   = (xdrproc_t)&xdr_NNTI_buffer_p_t;
    uint64_t         bytes_left  = packed_buflen;

    *(NNTI_datatype_t*)packed_buf = NNTI_dt_buffer;

    packed_buf += sizeof(NNTI_datatype_t);
    bytes_left -= sizeof(NNTI_datatype_t);

    xdrmem_create(
        &encode_xdrs,
        packed_buf,
        bytes_left,
        XDR_ENCODE);

    if (!encode_fn(XDRPROC_ARGS(&encode_xdrs, packable))) {
        log_fatal("nnti_buffer", "packing failed");
        return NNTI_EENCODE;
    }
    *packed_len = packed_buffer_size(packable);
#elif NNTI_USE_CEREAL == 1
    std::ostringstream oss;
    {
        NNTI_datatype_t t = NNTI_dt_buffer;
        cereal::PortableBinaryOutputArchive oarchive(oss);
        oarchive( t );
        oarchive( *packable );
    }
    *packed_len = oss.str().length();
    memcpy(packed_buf, oss.str().c_str(), *packed_len);
#endif

    return NNTI_OK;
}

NNTI_result_t
unpack_buffer(
    NNTI_buffer_p_t  *packable,
    char             *packed_buf,
    const uint64_t    packed_buflen)
{
#if NNTI_USE_XDR == 1
    XDR              decode_xdrs;
    xdrproc_t        decode_fn   = (xdrproc_t)&xdr_NNTI_buffer_p_t;
    NNTI_datatype_t *dt          = (NNTI_datatype_t*)packed_buf;
    uint64_t         dt_size     = sizeof(NNTI_buffer_p_t);
    uint64_t         bytes_left  = packed_buflen;

    packed_buf += sizeof(NNTI_datatype_t);
    bytes_left -= sizeof(NNTI_datatype_t);

    memset(packable, 0, dt_size);
    xdrmem_create(
        &decode_xdrs,
        packed_buf,
        bytes_left,
        XDR_DECODE);

    if (!decode_fn(XDRPROC_ARGS(&decode_xdrs, packable))) {
        log_fatal("nnti_buffer", "unpacking failed");
        return NNTI_EDECODE;
    }
#elif NNTI_USE_CEREAL == 1
    {
        NNTI_datatype_t t;
        std::istringstream iss(std::string(packed_buf, packed_buflen));
        cereal::PortableBinaryInputArchive iarchive(iss);
        iarchive( t );
        iarchive( *packable );
    }
#endif
    return NNTI_OK;
}

NNTI_result_t
free_buffer(
    NNTI_buffer_p_t *packable)
{
#if NNTI_USE_XDR == 1
    xdrproc_t free_fn = (xdrproc_t)&xdr_NNTI_buffer_p_t;
    xdr_free(free_fn, (char*)packable);
#endif
    return NNTI_OK;
}

} // namespace serialize
} // namespace nnti
