// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#ifndef IBVERBS_BUFFER_HPP_
#define IBVERBS_BUFFER_HPP_

#include "nnti/nntiConfig.h"

#include <assert.h>

#include <map>
#include <sstream>
#include <string>

#include <infiniband/verbs.h>

#include "nnti/nnti_packable.h"
#include "nnti/nnti_types.h"

#include "nnti/nnti_buffer.hpp"
#include "nnti/nnti_callback.hpp"


namespace nnti  {

namespace transports {
    // forward declaration
    class ibverbs_transport;
}

namespace datatype {

class ibverbs_buffer
: public nnti_buffer {
private:
    bool registered_;
    struct ibv_mr *mr;
private:
    NNTI_result_t
    register_buffer(void);
    NNTI_result_t
    unregister_buffer(void);
    NNTI_result_t
    post_receive(void);
    enum ibv_access_flags
    nnti_to_ib_flags(
        NNTI_buffer_flags_t nnti_flags);
public:
    ibverbs_buffer(void);
//    ibverbs_buffer(
//        nnti::transports::ibverbs_transport *transport,
//        NNTI_buffer_p_t                     *packable);
    ibverbs_buffer(
        nnti::datatype::ibverbs_buffer &b);
    ibverbs_buffer(
        nnti::transports::ibverbs_transport *transport,
        const uint64_t                       size,
        const NNTI_buffer_flags_t            flags,
        NNTI_event_queue_t                   eq,
        nnti_event_callback                  cb,
        void                                *cb_context);
    ibverbs_buffer(
        nnti::transports::ibverbs_transport *transport,
        char                                *buffer,
        const uint64_t                       size,
        const NNTI_buffer_flags_t            flags,
        NNTI_event_queue_t                   eq,
        nnti_event_callback                  cb,
        void                                *cb_context);
    ibverbs_buffer(
        nnti::transports::transport *transport,
        char                        *packed_buf,
        const uint64_t               packed_len);
    virtual ~ibverbs_buffer();

    virtual char*
    payload(void);
    void *
    addr(void);
    size_t
    length(void);
    uint32_t
    lkey(void);
    uint32_t
    rkey(void);
};

} /* namespace datatype */
} /* namespace nnti */

#endif /* IBVERBS_BUFFER_HPP_ */
