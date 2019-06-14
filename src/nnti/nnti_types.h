// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#ifndef NNTI_TYPES_H_
#define NNTI_TYPES_H_

#include <stdint.h>


/********** Constants **********/

/**
 * @brief Length of a URL
 */
#define NNTI_URL_LEN 128
/**
 * @brief Length of a hostname
 */
#define NNTI_HOSTNAME_LEN 32
/**
 * @brief The size of a buffer used for receiving requests (should be configurable via URL).
 *
 * This should be a multiple of 64 for best cache performance.  1216=64*19
 */
#define NNTI_REQUEST_BUFFER_SIZE 1216
/**
 * @brief The size of a buffer used for receiving results (should be configurable via URL).
 *
 * This should be a multiple of 64 for best cache performance.  1216=64*19
 */
#define NNTI_RESULT_BUFFER_SIZE 1216
/**
 * @brief The number of transport mechanisms supported by NNTI.
 */
#define NNTI_TRANSPORT_COUNT 4


/********** Enumerations **********/

enum NNTI_datatype_t {
    NNTI_dt_peer         = 1111,
    NNTI_dt_buffer       = 1112,
    NNTI_dt_work_id      = 1113,
    NNTI_dt_work_request = 1114,
    NNTI_dt_transport    = 1115,
    NNTI_dt_event_queue  = 1116,
    NNTI_dt_callback     = 1117
};

/**
 * @brief Enumerator of the transport mechanisms supported by NNTI.
 *
 * The <tt>\ref NNTI_transport_id_t</tt> enumerator provides integer values
 * to represent the supported transport mechanisms.
 */
enum NNTI_transport_id_t {
    /** @brief No operations permitted. */
    NNTI_TRANSPORT_NULL,

    /** @brief Use Infiniband to transfer rpc requests. */
    NNTI_TRANSPORT_IBVERBS,

    /** @brief Use Cray ugni to transfer rpc requests. */
    NNTI_TRANSPORT_UGNI,

    /** @brief Use MPI to transfer rpc requests. */
    NNTI_TRANSPORT_MPI
};
typedef enum NNTI_transport_id_t NNTI_transport_id_t;

#if defined(NNTI_BUILD_IBVERBS)
#define NNTI_DEFAULT_TRANSPORT NNTI_TRANSPORT_IBVERBS
#elif defined(NNTI_BUILD_UGNI)
#define NNTI_DEFAULT_TRANSPORT NNTI_TRANSPORT_UGNI
#elif defined(NNTI_BUILD_MPI)
#define NNTI_DEFAULT_TRANSPORT NNTI_TRANSPORT_MPI
#else
#define NNTI_DEFAULT_TRANSPORT NNTI_TRANSPORT_NULL
#endif

/**
 * @brief Enumerator of results that NNTI functions could generate.
 *
 * The <tt>\ref NNTI_result_t</tt> enumerator provides integer values
 * for NNTI function outcomes.
 */
enum NNTI_result_t {
    /** @brief The function completed successfully. */
    NNTI_OK = 0,

    /** @brief Unspecified I/O error. */
    NNTI_EIO = 1001,

    /** @brief The size of the message is larger than the supported maximum size. */
    NNTI_EMSGSIZE,

    /** @brief The operation or process has been canceled. */
    NNTI_ECANCELED,

    /** @brief An operation timed out. */
    NNTI_ETIMEDOUT,

    /** @brief The value of the variable or parameter is invalid. */
    NNTI_EINVAL,

    /** @brief  No memory is available. */
    NNTI_ENOMEM,

    /** @brief No such entry. */
    NNTI_ENOENT,

    /** @brief Unsupported operation. */
    NNTI_ENOTSUP,

    /** @brief The item already exists. */
    NNTI_EEXIST,

    /** @brief Unsuccessful RPC operation. */
    NNTI_EBADRPC,

    /** @brief Not initialized. */
    NNTI_ENOTINIT,

    /** @brief Insufficient privileges to perform operation. */
    NNTI_EPERM,

    /** @brief Either an operation was interrupted by a signal
     *         or NNTI_wait() was interrupted by NNTI_interrupt(). */
    NNTI_EINTR,

    /** @brief An async operation would have blocked. */
    NNTI_EWOULDBLOCK,

    /** @brief The requested resource is temporarily unavailable.  Retry later. */
    NNTI_EAGAIN,

    /** @brief The request could not be delivered. */
    NNTI_EDROPPED,

    /** @brief Error unpacking an NNTI data structure. */
    NNTI_EDECODE,

    /** @brief Error packing an NNTI data structure. */
    NNTI_EENCODE

};
typedef enum NNTI_result_t NNTI_result_t;

/**
  * atomic operations that may be implemented by a transport
  */
enum NNTI_atomic_op_t {
  NNTI_ATOMIC_FADD = 1
};
typedef enum NNTI_atomic_op_t NNTI_atomic_op_t;

/**
 * @brief Enumerator of operation types.
 *
 * The <tt>\ref NNTI_op_t</tt> enumerator describes the
 * operation a work request will perform.
 */
enum NNTI_op_t {
    NNTI_OP_NOOP         =  1,
    NNTI_OP_SEND         =  2,
    NNTI_OP_PUT          =  4,
    NNTI_OP_GET          =  8,
    NNTI_OP_ATOMIC_FADD  = 16,
    NNTI_OP_ATOMIC_CSWAP = 32
};
typedef enum NNTI_op_t NNTI_op_t;

/**
 * @brief Enumerator of operation options.
 *
 * The <tt>\ref NNTI_op_flags_t</tt> enumerator controls the
 * behavior of an operation.
 */
enum NNTI_op_flags_t {
    NNTI_OF_UNSET        =  0,
    NNTI_OF_LOCAL_EVENT  =  1,
    NNTI_OF_REMOTE_EVENT =  2,
    NNTI_OF_NO_ACK       =  4,
    NNTI_OF_USE_WAIT     =  8,
    NNTI_OF_ZERO_COPY    = 16
};
typedef enum NNTI_op_flags_t NNTI_op_flags_t;

/**
 * @brief Enumerator of event queue options.
 *
 * The <tt>\ref NNTI_eq_flags_t</tt> enumerator controls the
 * behavior of an event queue.
 */
enum NNTI_eq_flags_t {
    NNTI_EQF_UNSET      = 0,
    NNTI_EQF_UNEXPECTED = 1,
    NNTI_EQF_LOCKLESS   = 2
};
typedef enum NNTI_eq_flags_t NNTI_eq_flags_t;

/**
 * @brief Enumerator of event types.
 *
 * The <tt>\ref NNTI_event_type_t</tt> enumerator describes the
 * operation that generated the event.
 */
enum NNTI_event_type_t {
    NNTI_EVENT_NOOP        =   1,
    NNTI_EVENT_SEND        =   2,
    NNTI_EVENT_PUT         =   4,
    NNTI_EVENT_GET         =   8,
    NNTI_EVENT_ATOMIC      =  16,
    NNTI_EVENT_OVERFLOW    =  32,
    NNTI_EVENT_UNEXPECTED  =  64,
    NNTI_EVENT_ACK         = 128,
    NNTI_EVENT_RECV        = 256
};
typedef enum NNTI_event_type_t NNTI_event_type_t;

/**
 * @brief Enumerator of memory buffer options.
 *
 * The <tt>\ref NNTI_buffer_flags_t</tt> enumerator controls the
 * operations allowed on a register memory buffer.
 */
enum NNTI_buffer_flags_t {
    NNTI_BF_UNSET        =  0,
    /** @brief a local process/NIC can read from this buffer */
    NNTI_BF_LOCAL_READ   =  1,
    /** @brief a remote process/NIC can read from this buffer */
    NNTI_BF_REMOTE_READ  =  2,
    /** @brief a local process/NIC can write to this buffer */
    NNTI_BF_LOCAL_WRITE  =  4,
    /** @brief a remote process/NIC can write to this buffer */
    NNTI_BF_REMOTE_WRITE =  8,
    /** @brief SENDs to this memory occur at the offset+length of the last SEND */
    NNTI_BF_QUEUING      = 16,
    NNTI_BF_LOCAL_ATOMIC = 32,
    NNTI_BF_REMOTE_ATOMIC = 64
};
typedef enum NNTI_buffer_flags_t NNTI_buffer_flags_t;


/********** Opaque Types **********/

/** @brief Opaque handle to a registered memory region */
typedef uint64_t NNTI_buffer_t;

/** @brief Opaque handle to an event queue */
typedef uint64_t NNTI_event_queue_t;

/** @brief Opaque handle to a process */
typedef uint64_t NNTI_peer_t;

typedef uint64_t NNTI_process_id_t;

/** @brief Opaque handle to a transport */
typedef uint64_t NNTI_transport_t;

/** @brief Opaque handle to an outstanding operation */
typedef uint64_t NNTI_work_id_t;

#define NNTI_INVALID_HANDLE (uint64_t)0


/********** Public Types **********/

/**
 * @brief Describes the current runtime values of a transport.
 */
struct NNTI_attrs_t {
    /** @brief The Maximum Transmission Unit of the NIC. */
    uint32_t mtu;
    /** @brief The maximum size of the NNTI command header. */
    uint32_t max_cmd_header_size;
    /** @brief The maximum size of an eager message. */
    uint32_t max_eager_size;
    /** @brief The size of the per-connection command queue. */
    uint32_t cmd_queue_size;
};
typedef struct NNTI_attrs_t NNTI_attrs_t;

/**
 * @brief The status of an NNTI operation.
 *
 * The <tt>NNTI_status_t</tt> structure contains the result of an NNTI
 * operation.  The result and op fields will always contain reasonable values.
 * If the operation was successful (result==NNTI_OK), then the remaining
 * fields will also contain reasonable values.  If the operation was
 * unsuccessful, then the remaining fields are undefined.
 */
struct NNTI_status_t {
    /** @brief The operation performed on the buffer. */
    NNTI_op_t     op;
    /** @brief The result code for this operation. */
    NNTI_result_t result;

    /** @brief The address of the local buffer used by this operation. */
    void      *start;
    /** @brief The offset into the local buffer used by this operation. */
    uint64_t   offset;
    /** @brief The number of bytes used by this operation. */
    uint64_t   length;

    /** @brief The peer that was the data source for this operation. */
    NNTI_peer_t src;
    /** @brief The peer that was the data destination for this operation. */
    NNTI_peer_t dest;
};
typedef struct NNTI_status_t NNTI_status_t;


/**
 * @brief The result of an NNTI operation.
 *
 * The <tt>NNTI_event_t</tt> structure contains the result of an NNTI
 * operation.  The result and op fields will always contain reasonable values.
 * If the operation was successful (result==NNTI_OK), then the remaining
 * fields will also contain reasonable values.  If the operation was
 * unsuccessful, then the remaining fields are undefined.
 */
struct NNTI_event_t {
    NNTI_transport_t   trans_hdl;
    /** @brief The type of operation that caused the event. */
    NNTI_event_type_t  type;
    /** @brief The result code for this operation. */
    NNTI_result_t      result;
    /** @brief The work id that initiated the operation (only at the initiator). */
    NNTI_work_id_t     wid;
    /** @brief The operation performed on the buffer. */
    NNTI_op_t          op;
    /** @brief The other process involved in this operation. */
    NNTI_peer_t        peer;
    /** @brief The base address of the local buffer used by this operation. */
    void              *start;
    /** @brief The offset into the local buffer used by this operation. */
    uint64_t           offset;
    /** @brief The number of bytes operated on. */
    uint64_t           length;
    /** @brief The user data provided as "event_context" in the work request. */
    void              *context;
};
typedef struct NNTI_event_t NNTI_event_t;


/********** Callback Function Types **********/

/**
 * @brief The signature of callbacks invoked when events are generated.
 *
 * \param[in]  event    the event that caused the callback invocation
 * \param[in]  context  the user data
 */
typedef NNTI_result_t (*NNTI_event_callback_t) (
        NNTI_event_t *event,
        void         *context);


/**
 * @brief The work request prepared by an operation initiator.
 *
 */
struct NNTI_work_request_t {
    /** @brief The operation to perform on the buffer. */
    NNTI_op_t       op;
    NNTI_op_flags_t flags;

    NNTI_transport_t trans_hdl;

    /** @brief The other process involved in this operation. */
    NNTI_peer_t    peer;

    /** @brief The handle to the local buffer for this operation. */
    NNTI_buffer_t  local_hdl;
    /** @brief The offset into the local buffer for this operation. */
    uint64_t       local_offset;

    /** @brief The handle to the remote buffer for this operation. */
    NNTI_buffer_t  remote_hdl;
    /** @brief The offset into the remote buffer for this operation. */
    uint64_t       remote_offset;

    /** @brief The number of bytes for this operation. */
    uint64_t       length;
    /** @brief The add/compare operand for fetch-add/compare-swap atomic operations. */
    int64_t        operand1;
    /** @brief The swap operand for compare-swap atomic operations. */
    int64_t        operand2;

    /** @brief Deliver events to this EQ instead of the default. */
    NNTI_event_queue_t     alt_eq;
    /** @brief The callback to invoke when the operation completes. */
    NNTI_event_callback_t  callback;
    /** @brief The user data to include when invoking callback. */
    void                  *cb_context;
    /** @brief The user data to include in the event. */
    void                  *event_context;
};
typedef struct NNTI_work_request_t NNTI_work_request_t;


/********** Initializers **********/
//Note: these pragmas are here because C++ people can't cope with human readable c99 structures
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
/**
 * @brief A simple initializer to zero-out a work request.
 */
static struct NNTI_work_request_t NNTI_WR_INITIALIZER =
{   .op = NNTI_OP_NOOP,
    .flags = NNTI_OF_UNSET,
    .trans_hdl = 0,
    .peer = 0,
    .local_hdl = 0,
    .local_offset = 0,
    .remote_hdl = 0,
    .remote_offset = 0,
    .length = 0,
    .operand1 = 0,
    .operand2 = 0,
    .alt_eq = 0,
    .callback = 0,
    .cb_context = 0,
    .event_context = 0
};
#pragma GCC diagnostic pop

#endif /* NNTI_TYPES_H_ */
