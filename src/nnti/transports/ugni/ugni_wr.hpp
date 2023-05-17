// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#ifndef UGNI_WR_HPP
#define UGNI_WR_HPP

#include "nnti/nntiConfig.h"

#include <sstream>
#include <string>

#include "nnti/nnti_types.h"

#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_datatype.hpp"
#include "nnti/nnti_callback.hpp"
#include "nnti/nnti_peer.hpp"

#include "nnti/transports/ugni/ugni_buffer.hpp"


namespace nnti  {
namespace datatype {

class ugni_work_request
: public nnti_work_request {
private:

public:
    ugni_work_request(
        nnti::transports::transport *transport)
    : nnti_work_request(transport)
    {
        return;
    }
    ugni_work_request(
        nnti::transports::transport *transport,
        NNTI_work_request_t         &wr)
    : nnti_work_request(transport,
                        wr)
    {
        return;
    }
    virtual ~ugni_work_request()
    {
        return;
    }

    void *
    local_addr(void) const
    {
        nnti::datatype::ugni_buffer *b = (nnti::datatype::ugni_buffer *)local_hdl();
        return b->addr();
    }
    NNTI_ugni_mem_hdl_p_t
    local_mem_hdl(void) const
    {
        nnti::datatype::ugni_buffer *b = (nnti::datatype::ugni_buffer *)local_hdl();
        return b->mem_hdl();
    }
    size_t
    local_length(void) const
    {
        nnti::datatype::ugni_buffer *b = (nnti::datatype::ugni_buffer *)local_hdl();
        return b->length();
    }

    void *
    remote_addr(void) const
    {
        nnti::datatype::ugni_buffer *b = (nnti::datatype::ugni_buffer *)remote_hdl();
        return b->addr();
    }
    NNTI_ugni_mem_hdl_p_t
    remote_mem_hdl(void) const
    {
        nnti::datatype::ugni_buffer *b = (nnti::datatype::ugni_buffer *)remote_hdl();
        return b->mem_hdl();
    }
    size_t
    remote_length(void) const
    {
        nnti::datatype::ugni_buffer *b = (nnti::datatype::ugni_buffer *)remote_hdl();
        return b->length();
    }
};

} /* namespace datatype */
} /* namespace nnti */

#endif /* UGNI_WR_HPP */
