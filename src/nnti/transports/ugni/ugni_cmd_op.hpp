// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#ifndef UGNI_CMD_OP_HPP_
#define UGNI_CMD_OP_HPP_

#include "nnti/nntiConfig.h"

#include <assert.h>

#include <atomic>
#include <map>
#include <vector>
#include <sstream>
#include <string>

#include "faodel-common/MutexWrapper.hh"

#include "nnti/nnti_serialize.hpp"
#include "nnti/nnti_types.h"
#include "nnti/nnti_threads.h"
#include "nnti/nnti_util.hpp"

#include "nnti/nnti_datatype.hpp"
#include "nnti/nnti_buffer.hpp"
#include "nnti/nnti_op.hpp"
#include "nnti/nnti_wid.hpp"
#include "nnti/nnti_logger.hpp"

#include "nnti/nnti_state_machine.hpp"

#include "nnti/transports/ugni/ugni_transport.hpp"
#include "nnti/transports/ugni/ugni_cmd_msg.hpp"
#include "nnti/transports/ugni/ugni_connection.hpp"


namespace nnti {
namespace core {

class ugni_cmd_op
: public nnti_op, public state_machine {
private:
    nnti::transports::ugni_transport *transport_;
    nnti::core::ugni_cmd_msg          cmd_msg_;

    faodel::MutexWrapper *sm_lock_;

public:
    ugni_cmd_op(
        nnti::transports::ugni_transport *transport,
        const uint32_t                    cmd_msg_size);
    ugni_cmd_op(
        nnti::transports::ugni_transport *transport,
        const uint32_t                    cmd_msg_size,
        nnti::datatype::nnti_work_id     *wid);
    ugni_cmd_op(
        nnti::transports::ugni_transport *transport,
        nnti::datatype::nnti_work_id     *wid);
    virtual ~ugni_cmd_op();

    void
    set(nnti::datatype::nnti_work_id *wid);

    bool
    eager(void);

    char *
    cmd_buf(void);

    uint32_t
    cmd_size(void);

    void
    src_op_id(uint32_t soi);
    uint32_t
    src_op_id(void);

    nnti::datatype::ugni_peer*
    target_peer(void);

    virtual std::string
    toString(void);

    /*
     * Implement the state_machine interface
     */
private:
    enum op_state : int {
        INIT = 0,
        EXECUTE_SEND,

        NEED_SEND_CREDITS,
        WAIT_SEND_CREDITS,

        NEED_SEND_COMPLETE,
        WAIT_SEND_COMPLETE,
        WAIT_RDMA_ACK,
        ISSUE_SEND_EVENT,

        CLEANUP,
        DONE
    };

    op_state state_;

    op_state
    execute_send(void);
    NNTI_event_t *
    create_event(void);
    op_state
    issue_send_event(void);
    op_state
    update_stats(void);

public:
    int
    update(NNTI_event_t *event);
};

} /* namespace core */
} /* namespace nnti */

#endif /* UGNI_CMD_OP_HPP_ */
