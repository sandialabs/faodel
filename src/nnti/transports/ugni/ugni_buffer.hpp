// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#ifndef UGNI_BUFFER_HPP_
#define UGNI_BUFFER_HPP_

#include "nnti/nntiConfig.h"

#include <assert.h>

#include <map>
#include <sstream>
#include <string>

#include <alps/libalpslli.h>
#include <gni_pub.h>

#include "nnti/nnti_serialize.hpp"
#include "nnti/nnti_types.h"

#include "nnti/nnti_buffer.hpp"
#include "nnti/nnti_callback.hpp"


namespace nnti  {

namespace transports {
    // forward declaration
    class ugni_transport;
}

namespace datatype {

class ugni_buffer
: public nnti_buffer {
private:
    bool registered_;
    gni_mem_handle_t hdl_;
private:
    NNTI_result_t
    register_buffer(void);
    NNTI_result_t
    register_segments(void);
    NNTI_result_t
    unregister_buffer(void);
    NNTI_result_t
    unregister_segments(void);
//    NNTI_result_t
//    post_receive(void);
    uint32_t
    nnti_to_ugni_flags(
        NNTI_buffer_flags_t nnti_flags);
public:
    ugni_buffer(void);
    ugni_buffer(
        nnti::datatype::ugni_buffer &b);
    ugni_buffer(
        nnti::transports::ugni_transport *transport,
        const uint64_t                   size,
        const NNTI_buffer_flags_t        flags,
        NNTI_event_queue_t               eq,
        nnti_event_callback              cb,
        void                            *cb_context);
    ugni_buffer(
        nnti::transports::ugni_transport *transport,
        char                            *buffer,
        const uint64_t                   size,
        const NNTI_buffer_flags_t        flags,
        NNTI_event_queue_t               eq,
        nnti_event_callback              cb,
        void                            *cb_context);
    ugni_buffer(
        nnti::transports::transport *transport,
        char                        *packed_buf,
        const uint64_t               packed_len);
    virtual ~ugni_buffer();

    virtual char*
    payload(void);
    void *
    addr(void);
    size_t
    length(void);
    NNTI_ugni_mem_hdl_p_t
    mem_hdl(void);
};

} /* namespace datatype */
} /* namespace nnti */

#endif /* UGNI_BUFFER_HPP_ */
