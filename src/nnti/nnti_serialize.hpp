// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef NNTI_SERIALIZE_HPP
#define NNTI_SERIALIZE_HPP

#include "nnti/nntiConfig.h"

#include <cstring>

#if NNTI_USE_XDR == 1
#include "nnti/serializers/xdr/nnti_xdr.hpp"
#elif NNTI_USE_CEREAL == 1
#include "nnti/serializers/cereal/nnti_cereal.hpp"
#endif

namespace nnti {
namespace serialize {

NNTI_datatype_t
get_datatype(
    char           *packed_buf,
    const uint64_t  packed_len);

uint64_t
packed_peer_size(
    NNTI_peer_p_t *packable);

NNTI_result_t
pack_peer(
    NNTI_peer_p_t  *packable,
    char           *packed_buf,
    const uint64_t  packed_buflen,
    uint64_t       *packed_len);

NNTI_result_t
unpack_peer(
    NNTI_peer_p_t  *packable,
    char           *packed_buf,
    const uint64_t  packed_buflen);

NNTI_result_t
free_peer(
    NNTI_peer_p_t *packable);

uint64_t
packed_buffer_size(
    NNTI_buffer_p_t *packable);

NNTI_result_t
pack_buffer(
    NNTI_buffer_p_t  *packable,
    char             *packed_buf,
    const uint64_t    packed_buflen,
    uint64_t         *packed_len);

NNTI_result_t
unpack_buffer(
    NNTI_buffer_p_t  *packable,
    char             *packed_buf,
    const uint64_t    packed_buflen);

NNTI_result_t
free_buffer(
    NNTI_buffer_p_t *packable);


} // namespace serialize
} // namespace nnti

#endif /* NNTI_SERIALIZE_HPP */
