// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#ifndef NNTI_WR_HPP
#define NNTI_WR_HPP

#include "nnti/nntiConfig.h"

#include <sstream>
#include <string>

#include "nnti/nnti_types.h"

#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_datatype.hpp"
#include "nnti/nnti_callback.hpp"
#include "nnti/nnti_peer.hpp"


namespace nnti  {
namespace datatype {

class nnti_work_request
: public nnti_datatype {
private:

protected:
    // a copy of the C work request
    NNTI_work_request_t                 wr_;
    nnti::datatype::nnti_event_callback cb_;

public:
    nnti_work_request(
        nnti::transports::transport *transport)
        : cb_(transport),
          nnti_datatype(transport,
                        NNTI_dt_work_request)
    {
        return;
    }
    nnti_work_request(
        nnti::transports::transport *transport,
        NNTI_work_request_t         &wr)
        : cb_(transport,
              wr.callback),
          nnti_datatype(transport,
                        NNTI_dt_work_request)
    {
        wr_ = wr;

        return;
    }
    nnti_work_request(
        nnti::transports::transport         *transport,
        NNTI_work_request_t                 &wr,
        nnti::datatype::nnti_event_callback  cb)
        : cb_(cb),
          nnti_datatype(transport,
                        NNTI_dt_work_request)
    {
        wr_ = wr;

        return;
    }
    virtual ~nnti_work_request()
    {
        return;
    }

    virtual const NNTI_work_request_t&
    wr(void) const
    {
        return wr_;
    }

    virtual NNTI_peer_t
    peer(void) const
    {
        return wr_.peer;
    }
    virtual const NNTI_process_id_t
    peer_pid(void) const
    {
        nnti::datatype::nnti_peer *peer=(nnti::datatype::nnti_peer*)wr_.peer;
        return peer->pid();
    }

    NNTI_op_t
    op(void) const
    {
        return wr_.op;
    }
    NNTI_op_flags_t
    flags(void) const
    {
        return wr_.flags;
    }

    const NNTI_buffer_t&
    local_hdl(void) const
    {
        return wr_.local_hdl;
    }
    const NNTI_buffer_t&
    remote_hdl(void) const
    {
        return wr_.remote_hdl;
    }

    uint64_t
    local_offset(void) const
    {
        return wr_.local_offset;
    }
    uint64_t
    remote_offset(void) const
    {
        return wr_.remote_offset;
    }
    uint64_t
    length(void) const
    {
        return wr_.length;
    }
    uint64_t
    operand1(void) const
    {
        return wr_.operand1;
    }
    uint64_t
    operand2(void) const
    {
        return wr_.operand2;
    }

    NNTI_event_queue_t
    alt_eq(void) const
    {
        return wr_.alt_eq;
    }

    nnti::datatype::nnti_event_callback &
    callback(void)
    {
        return cb_;
    }
    void *
    cb_context(void) const
    {
        return wr_.cb_context;
    }
    NNTI_result_t invoke_cb(NNTI_event_t *event) {
        log_debug("nnti_event_queue", "invoking the WR callback");
        return cb_.invoke(event, wr_.cb_context);
    }

    void *
    event_context(void) const
    {
        return wr_.event_context;
    }

    virtual std::string
    toString(void) const
    {
        std::stringstream out;
        out << "cb_==" << &cb_;
        return out.str();
    }

};

} /* namespace datatype */
} /* namespace nnti */

#endif /* NNTI_WR_HPP */
