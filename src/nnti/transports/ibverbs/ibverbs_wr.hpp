// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#ifndef IBVERBS_WR_HPP
#define IBVERBS_WR_HPP

#include "nnti/nntiConfig.h"

#include <sstream>
#include <string>

#include "nnti/nnti_types.h"

#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_datatype.hpp"
#include "nnti/nnti_callback.hpp"
#include "nnti/nnti_peer.hpp"

#include "nnti/transports/ibverbs/ibverbs_buffer.hpp"


namespace nnti  {
namespace datatype {

class ibverbs_work_request
: public nnti_work_request {
private:

public:
    ibverbs_work_request(
        nnti::transports::transport *transport)
    : nnti_work_request(transport)
    {
        return;
    }
    ibverbs_work_request(
        nnti::transports::transport *transport,
        NNTI_work_request_t         &wr)
    : nnti_work_request(transport,
                        wr)
    {
        return;
    }
    virtual ~ibverbs_work_request()
    {
        return;
    }

    void *
    local_addr(void) const
    {
        nnti::datatype::ibverbs_buffer *b = (nnti::datatype::ibverbs_buffer *)local_hdl();
        return b->addr();
    }
    uint32_t
    local_lkey(void) const
    {
        nnti::datatype::ibverbs_buffer *b = (nnti::datatype::ibverbs_buffer *)local_hdl();
        return b->lkey();
    }
    size_t
    local_length(void) const
    {
        nnti::datatype::ibverbs_buffer *b = (nnti::datatype::ibverbs_buffer *)local_hdl();
        return b->length();
    }

    void *
    remote_addr(void) const
    {
        nnti::datatype::ibverbs_buffer *b = (nnti::datatype::ibverbs_buffer *)remote_hdl();
        return b->addr();
    }
    uint32_t
    remote_rkey(void) const
    {
        nnti::datatype::ibverbs_buffer *b = (nnti::datatype::ibverbs_buffer *)remote_hdl();
        return b->rkey();
    }
    size_t
    remote_length(void) const
    {
        nnti::datatype::ibverbs_buffer *b = (nnti::datatype::ibverbs_buffer *)remote_hdl();
        return b->length();
    }
};

} /* namespace datatype */
} /* namespace nnti */

#endif /* IBVERBS_WR_HPP */
