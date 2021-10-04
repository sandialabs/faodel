// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include "nnti/nntiConfig.h"

#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <zlib.h>

#include <assert.h>

#include "nnti/nnti.h"
#include "nnti/nnti_logger.h"


#ifdef __cplusplus
extern "C" {
#endif

void
test_bootstrap_start(void);
void
test_bootstrap_finish(void);

NNTI_result_t
cb_func(NNTI_event_t *event, void *context);

NNTI_result_t
send_target_hdl(NNTI_transport_t  transport,
    NNTI_buffer_t                 send_hdl,
    char                         *send_base,
    uint64_t                      send_size,
    NNTI_buffer_t                 target_hdl,
    NNTI_peer_t                   peer_hdl,
    NNTI_event_queue_t            eq);

NNTI_result_t
recv_target_hdl(NNTI_transport_t transport,
                NNTI_buffer_t                recv_hdl,
                char                        *recv_base,
                NNTI_buffer_t               *target_hdl,
                NNTI_peer_t                 *peer_hdl,
                NNTI_event_queue_t           eq);

NNTI_result_t
send_hdl(NNTI_transport_t    transport,
         NNTI_buffer_t       hdl,
         char               *hdl_base,
         uint64_t            hdl_size,
         NNTI_peer_t         peer_hdl,
         NNTI_event_queue_t  eq);

NNTI_result_t
recv_hdl(NNTI_transport_t    transport,
         NNTI_buffer_t       recv_hdl,
         char               *recv_base,
         uint32_t            recv_size,
         NNTI_buffer_t      *hdl,
         NNTI_peer_t        *peer_hdl,
         NNTI_event_queue_t  eq);

NNTI_result_t
send_ack(NNTI_transport_t    transport,
         NNTI_buffer_t       hdl,
         NNTI_buffer_t       ack_hdl,
         NNTI_peer_t         peer_hdl,
         NNTI_event_queue_t  eq);

NNTI_result_t
recv_ack(NNTI_transport_t    transport,
         NNTI_buffer_t       ack_hdl,
         NNTI_peer_t        *peer_hdl,
         NNTI_event_queue_t  eq);

NNTI_result_t
populate_buffer(NNTI_transport_t transport,
                uint32_t         seed,
                uint64_t         offset_multiplier,
                NNTI_buffer_t    buf_hdl,
                char            *buf_base,
                uint64_t         buf_size);

int
verify_buffer(char          *buf_base,
              uint64_t       buf_offset,
              uint64_t       buf_size);

NNTI_result_t
send_data(NNTI_transport_t   transport,
          uint64_t           offset_multiplier,
          NNTI_buffer_t      src_hdl,
          NNTI_buffer_t      dst_hdl,
          NNTI_peer_t        peer_hdl,
          NNTI_event_queue_t eq);

NNTI_result_t
recv_data(NNTI_transport_t transport,
          NNTI_event_queue_t           eq,
          NNTI_event_t                *event);

NNTI_result_t
get_data(NNTI_transport_t   transport,
         NNTI_buffer_t      src_hdl,
         NNTI_buffer_t      dst_hdl,
         NNTI_peer_t        peer_hdl,
         NNTI_event_queue_t eq);

NNTI_result_t
put_data(NNTI_transport_t   transport,
         NNTI_buffer_t      src_hdl,
         NNTI_buffer_t      dst_hdl,
         NNTI_peer_t        peer_hdl,
         NNTI_event_queue_t eq);

#ifdef __cplusplus
}
#endif

#endif /* TEST_UTILS_H */
