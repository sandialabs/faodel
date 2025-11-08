// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#ifndef NNTI_WR_HPP
#define NNTI_WR_HPP

#include "nnti/nntiConfig.h"

#include "nnti/nnti_types.h"
#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_datatype.hpp"
#include "nnti/nnti_callback.hpp"


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
        nnti::transports::transport *transport);
    nnti_work_request(
        nnti::transports::transport *transport,
        NNTI_work_request_t         &wr);
    nnti_work_request(
        nnti::transports::transport         *transport,
        NNTI_work_request_t                 &wr,
        nnti::datatype::nnti_event_callback  cb);
    ~nnti_work_request() override;

    virtual const NNTI_work_request_t&
    wr(void) const;

    virtual NNTI_peer_t
    peer(void) const;
    virtual NNTI_process_id_t
    peer_pid(void) const;

    NNTI_op_t
    op(void) const;
    NNTI_op_flags_t
    flags(void) const;

    const NNTI_buffer_t&
    local_hdl(void) const;
    const NNTI_buffer_t&
    remote_hdl(void) const;

    uint64_t
    local_offset(void) const;
    uint64_t
    remote_offset(void) const;
    uint64_t
    length(void) const;
    uint64_t
    operand1(void) const;
    uint64_t
    operand2(void) const;

    NNTI_event_queue_t
    alt_eq(void) const;

    nnti::datatype::nnti_event_callback &
    callback(void);
    void *
    cb_context(void) const;
    NNTI_result_t
    invoke_cb(NNTI_event_t *event);

    void *
    event_context(void) const;

    virtual std::string
    toString(void) const override;
};

} /* namespace datatype */
} /* namespace nnti */

#endif /* NNTI_WR_HPP */
