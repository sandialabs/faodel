// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#ifndef UGNI_CMD_MSG_HPP_
#define UGNI_CMD_MSG_HPP_

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

#include "nnti/transports/ugni/ugni_transport.hpp"
#include "nnti/transports/ugni/ugni_buffer.hpp"
#include "nnti/transports/ugni/ugni_cmd_buffer.hpp"
#include "nnti/transports/ugni/ugni_peer.hpp"


namespace nnti {
namespace core {

class ugni_cmd_msg {
private:
    const static uint32_t packed_buffer_size_ = 164;

    struct cmd_msg {
        NNTI_process_id_t initiator; // 8
        uint64_t initiator_offset;   // 8
        uint64_t target_offset;      // 8
        uint64_t payload_length;     // 8
        uint64_t target_base_addr;   // 8
        uint32_t id;                 // 4
        uint32_t src_op_id;          // 4
        char     packed_initiator_hdl[packed_buffer_size_]; // 164
        // total header is 212 bytes
        // eager_payload[] is a just a place holder.  The actual
        // eager payload size is number of bytes allocated to for
        // each command message minus 212 bytes of header.
        char     eager_payload[1];
    };

private:
    nnti::transports::ugni_transport *transport_;
    struct cmd_msg                   *cmd_msg_buf_;
    uint32_t                          cmd_msg_size_;

    struct cmd_msg *internal_cmd_msg_buf_;

    bool free_in_dtor_;

    bool unexpected_;

    nnti::datatype::ugni_peer   *initiator_peer_;
    nnti::datatype::ugni_buffer *initiator_hdl_;
    nnti::datatype::ugni_peer   *target_peer_;
    nnti::datatype::ugni_buffer *target_hdl_;

    gni_post_descriptor_t post_desc_;

    bool initiator_hdl_valid_;
    bool target_hdl_valid_;

public:
    ugni_cmd_msg(
        nnti::transports::ugni_transport *transport,
        const uint32_t                    cmd_msg_size);
    ugni_cmd_msg(
        nnti::transports::ugni_transport *transport,
        const uint32_t                    cmd_msg_size,
        uint32_t                          id,
        nnti::datatype::nnti_work_id     *wid);
    ugni_cmd_msg(
        nnti::transports::ugni_transport *transport,
        uint32_t                          id,
        nnti::datatype::nnti_work_id     *wid);
    ugni_cmd_msg(
        nnti::transports::ugni_transport *transport,
        char                             *buf,
        uint32_t                          buf_size,
        bool                              copy_buf);
    ugni_cmd_msg(
        nnti::transports::ugni_transport *transport,
        char                             *buf,
        uint32_t                          buf_size);
    virtual ~ugni_cmd_msg();

    void
    set(
        uint32_t                      id,
        nnti::datatype::nnti_work_id *wid);
    void
    set(
        char     *buf,
        uint32_t  buf_size,
        bool      copy_buf);

    char *
    buf(void);
    uint32_t
    size(void);

    void
    unpack(void);

    static uint64_t
    header_length(void);

    bool
    unexpected(void);

    uint64_t
    initiator_offset(void);
    uint64_t
    target_offset(void);

    nnti::datatype::ugni_peer*
    initiator_peer(void);
    nnti::datatype::ugni_buffer*
    initiator_buffer(void);
    nnti::datatype::ugni_peer*
    target_peer(void);
    nnti::datatype::ugni_buffer*
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

    uint32_t
    id(void);

    void
    post_desc(gni_post_descriptor_t *post_desc);
    gni_post_descriptor_t *
    post_desc(void);

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

#endif /* UGNI_CMD_MSG_HPP_ */
