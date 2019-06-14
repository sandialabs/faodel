// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#ifndef IBVERBS_CMD_MSG_HPP_
#define IBVERBS_CMD_MSG_HPP_

#include "nnti/nntiConfig.h"

#include <assert.h>

#include <atomic>
#include <map>
#include <vector>
#include <sstream>
#include <string>

#include "nnti/nnti_serialize.hpp"
#include "nnti/nnti_types.h"
#include "nnti/nnti_threads.h"
#include "nnti/nnti_util.hpp"

#include "nnti/nnti_datatype.hpp"
#include "nnti/nnti_op.hpp"
#include "nnti/nnti_wid.hpp"

#include "nnti/transports/ibverbs/ibverbs_transport.hpp"
#include "nnti/transports/ibverbs/ibverbs_buffer.hpp"
#include "nnti/transports/ibverbs/ibverbs_peer.hpp"


namespace nnti {
namespace core {

class ibverbs_cmd_buffer;

class ibverbs_cmd_msg {
private:
    const static uint32_t packed_buffer_size_ = 152;

    struct cmd_msg {
        NNTI_process_id_t initiator; // 8
        uint64_t initiator_offset; // 8
        uint64_t target_offset;    // 8
        uint64_t payload_length;   // 8
        uint64_t target_base_addr; // 8
        uint32_t id;               // 4
        uint32_t src_op_id;          // 4
        char     packed_initiator_hdl[packed_buffer_size_]; // 152
        // total header is 196 bytes
        // eager_payload[] is a just a place holder.  The actual
        // eager payload size is number of bytes allocated to for
        // each command message minus 196 bytes of header.
        char     eager_payload[1];
    };

private:
    nnti::transports::ibverbs_transport *transport_;
    nnti::core::ibverbs_cmd_buffer      *cmd_buf_;
    struct cmd_msg                      *cmd_msg_buf_;
    uint32_t                             cmd_msg_size_;

    bool free_in_dtor_;

    bool unexpected_;
    bool ack_;

    nnti::datatype::ibverbs_peer           *initiator_peer_;
    nnti::datatype::ibverbs_buffer         *initiator_hdl_;
    nnti::datatype::ibverbs_buffer         *target_hdl_;

    bool initiator_hdl_valid_;
    bool target_hdl_valid_;

public:
    ibverbs_cmd_msg(
        nnti::transports::ibverbs_transport *transport,
        const uint32_t                       cmd_msg_size);
    ibverbs_cmd_msg(
        nnti::transports::ibverbs_transport *transport,
        const uint32_t                       cmd_msg_size,
        uint32_t                             id,
        nnti::datatype::nnti_work_id        *wid);
    ibverbs_cmd_msg(
        nnti::transports::ibverbs_transport *transport,
        uint32_t                             id,
        nnti::datatype::nnti_work_id        *wid);
    ibverbs_cmd_msg(
        nnti::transports::ibverbs_transport *transport,
        nnti::core::ibverbs_cmd_buffer      *cmd_buf,
        char                                *buf,
        uint32_t                             buf_size);
    virtual ~ibverbs_cmd_msg();

    void
    set(
        uint32_t                      id,
        nnti::datatype::nnti_work_id *wid);
    void
    set(
        uint32_t id,
        uint32_t source_op_id);

    char *
    buf(void);
    uint32_t
    size(void);

    void
    unpack(void);

    static uint64_t
    header_length(void);

    bool
    ack(void);
    bool
    unexpected(void);

    uint64_t
    initiator_offset(void);
    uint64_t
    target_offset(void);

    nnti::datatype::ibverbs_peer*
    initiator_peer(void);
    nnti::datatype::ibverbs_buffer*
    initiator_buffer(void);
    nnti::datatype::ibverbs_buffer*
    target_buffer(void);
    bool
    eager(void);
    char *
    eager_payload(void);
    uint64_t
    payload_length(void);

    void
    src_op_id(uint32_t soi);
    uint32_t
    src_op_id(void);

    void
    post_recv();

    virtual std::string
    toString(void);

private:
    void
    get_cmd_msg_buffer(void);

    void
    pack(
        uint32_t                      id,
        nnti::datatype::nnti_work_id *wid);
};

} /* namespace core */
} /* namespace nnti */

#endif /* IBVERBS_CMD_MSG_HPP_ */
