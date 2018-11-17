
#ifdef RPC_HDR
%#include "nnti/nntiConfig.h"
%#include "nnti/nnti_xdr.h"
%#include <stdint.h>
#endif

#ifdef RPC_XDR
%#include "nnti/nntiConfig.h"
%#include "nnti/nnti_xdr.h"
#endif


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


#ifdef RPC_HDR
%#if defined(NNTI_BUILD_IBVERBS)
%#define NNTI_DEFAULT_TRANSPORT NNTI_TRANSPORT_IBVERBS
%#elif defined(NNTI_BUILD_UGNI)
%#define NNTI_DEFAULT_TRANSPORT NNTI_TRANSPORT_UGNI
%#elif defined(NNTI_BUILD_MPI)
%#define NNTI_DEFAULT_TRANSPORT NNTI_TRANSPORT_MPI
%#else
%#define NNTI_DEFAULT_TRANSPORT NNTI_TRANSPORT_NULL
%#endif
#endif


/**
 * @brief Length of a URL
 */
const NNTI_URL_LEN = 128;


/***********  TCP/IP address types  ***********/

/**
 * @brief Binary encoding of a TCP/IP host address.
 *
 * The <tt>\ref NNTI_ip_addr</tt> type identifies a particular node.
 */
typedef uint32_t NNTI_ip_addr;

/**
 * @brief TCP port in NBO.
 *
 * The <tt>\ref NNTI_tcp_addr</tt> type identifies a particular port.
 */
typedef uint16_t NNTI_tcp_port;


/***********  NULL Process Types  ***********/

/**
 * @brief Remote process identifier for the NULL transport.
 *
 */
struct NNTI_null_process_p_t {
    int i;  /* unused, but empty structs are not allowed */
};


/***********  IB Process Types  ***********/

/**
 * @brief Remote process identifier for IB.
 *
 * The <tt>\ref NNTI_ib_process_t</tt> identifies a particular process
 * on a particular node.  If a connection has been established to the
 * represented process, then that connection is identified by 'qp_num'.
 */
struct NNTI_ib_process_p_t {
    /** @brief IP address encoded in Network Byte Order */
    NNTI_ip_addr  addr;
    /** @brief TCP port encoded in Network Byte Order */
    NNTI_tcp_port port;
};


/***********  Gemini Process Types  ***********/

/**
 * @brief The instance ID of a Gemini process.
 *
 * The <tt>\ref NNTI_inst_id</tt> type identifies a particular process
 * within a communication domain.
 */
typedef uint32_t NNTI_instance_id;

/**
 * @brief Remote process identifier for Gemini.
 *
 * The <tt>\ref NNTI_ugni_process_t</tt> identifies a particular process
 * on a particular node.  If a connection has been established to the
 * represented process, then that connection is identified by 'inst_id'.
 */
struct NNTI_ugni_process_p_t {
    /** @brief IP address encoded in Network Byte Order */
    NNTI_ip_addr  addr;
    /** @brief TCP port encoded in Network Byte Order */
    NNTI_tcp_port port;
    /** @brief Gemini process instance ID */
    NNTI_instance_id inst_id;
};


/***********  MPI Process Types  ***********/

/**
 * @brief Remote process identifier for MPI.
 *
 * The <tt>\ref NNTI_mpi_process_t</tt> identifies a particular process
 * on a particular node.
 */
struct NNTI_mpi_process_p_t {
    /** @brief MPI rank. */
    int rank;
};


/***********  Local Process Types  ***********/

/**
 * @brief Remote process identifier for the Local transport.
 *
 */
struct NNTI_local_process_p_t {
    int i;  /* unused, but empty structs are not allowed */
};


/***********  Remote Process Union  ***********/

/**
 * @brief A structure to represent a remote processes.
 *
 * The <tt>NNTI_remote_process_t</tt> structure contains the
 * transport specific info needed to identify a process running
 * on a remote node.
 */
#if defined(RPC_HDR) || defined(RPC_XDR)
union NNTI_remote_process_p_t switch (NNTI_transport_id_t transport_id) {
    /** @brief The NULL representation of a process on the network. */
    case NNTI_TRANSPORT_NULL:    NNTI_null_process_p_t    null;
    /** @brief The IB representation of a process on the network. */
    case NNTI_TRANSPORT_IBVERBS: NNTI_ib_process_p_t      ib;
    /** @brief The Cray UGNI representation of a process on the network. */
    case NNTI_TRANSPORT_UGNI:    NNTI_ugni_process_p_t    ugni;
    /** @brief The MPI representation of a process on the network. */
    case NNTI_TRANSPORT_MPI:     NNTI_mpi_process_p_t     mpi;
};
#else
union NNTI_remote_process_p_t {
    /** @brief The NULL representation of a process on the network. */
    NNTI_null_process_p_t    null;
    /** @brief The IB representation of a process on the network. */
    NNTI_ib_process_p_t      ib;
    /** @brief The Cray UGNI representation of a process on the network. */
    NNTI_ugni_process_p_t    ugni
    /** @brief The MPI representation of a process on the network. */
    NNTI_mpi_process_p_t     mpi;
};
#endif


/***********  Peer Type  ***********/

typedef uint64_t NNTI_process_id_t;

/**
 * @brief Handle to an NNTI process.
 *
 * This is the datatype used by NNTI clients to reference another process.
 * Use this handle to move data to/from the process.
 */
struct NNTI_peer_p_t {
    NNTI_datatype_t datatype;
    
    /** @brief binary encoding of a process's URL */
    NNTI_process_id_t pid;
    
    /** @brief binary encoding of a process on the network */
    NNTI_remote_process_p_t peer;
};


/***********  NULL RDMA Address Types  ***********/

/**
 * @brief RDMA address used for the NULL transport.
 */
struct NNTI_null_rdma_addr_p_t {
    int i;  /* unused, but empty structs are not allowed */
};


/***********  IB RDMA Address Types  ***********/

/**
 * @brief RDMA address used for the InfiniBand implementation.
 */
struct NNTI_ib_rdma_addr_p_t {
    /** @brief Address of the memory buffer cast to a uint64_t. */
    uint64_t  buf;
    /** @brief The key that a remote processes needs to access this buffer. */
    uint32_t  key;
    /** @brief Size of the the memory buffer. */
    uint32_t  size;
};


/***********  Gemini RDMA Address Types  ***********/

struct NNTI_ugni_mem_hdl_p_t {
    uint64_t qword1;
    uint64_t qword2;
};

/**
 * @brief RDMA address used for the Gemini implementation.
 */
struct NNTI_ugni_rdma_addr_p_t {
    /** @brief Address of the memory buffer cast to a uint64_t . */
    uint64_t  buf;
    /** @brief Size of the the memory buffer. */
    uint32_t  size;
    /** @brief The key that a remote processes needs to access this buffer. */
    NNTI_ugni_mem_hdl_p_t mem_hdl;
};


/***********  MPI RDMA Address Types  ***********/

/**
 * @brief Definition for match bits in MPI.
 */
typedef uint64_t NNTI_match_bits;

/**
 * @brief RDMA address used for the MPI implementation.
 */
struct NNTI_mpi_rdma_addr_p_t {
    /** @brief The MPI tag for RTR/RTS msg. */
    NNTI_match_bits cmd_tag;
    /** @brief The MPI tag for GET data msg. */
    NNTI_match_bits get_data_tag;
    /** @brief The MPI tag for PUT data msg. */
    NNTI_match_bits put_data_tag;
    /** @brief The MPI tag for ATOMIC data msg. */
    NNTI_match_bits atomic_data_tag;

    /** @brief Address of the memory buffer cast to a uint64_t . */
    uint64_t buf;
    /** @brief Size of the the memory buffer. */
    uint32_t size;
};


/***********  Local RDMA Address Types  ***********/

/**
 * @brief RDMA address used for the Local transport.
 */
struct NNTI_local_rdma_addr_p_t {
    int i;  /* unused, but empty structs are not allowed */
};


/***********  Remote Address Union  ***********/

/**
 * @brief A structure to represent a remote memory region.
 *
 * The <tt>NNTI_remote_addr_t</tt> structure contains the
 * transport specific info needed to identify a memory region
 * on a remote node.
 */
#if defined(RPC_HDR) || defined(RPC_XDR)
union NNTI_remote_addr_p_t switch (NNTI_transport_id_t transport_id) {
    /** @brief The NULL representation of a memory region. */
    case NNTI_TRANSPORT_NULL:    NNTI_null_rdma_addr_p_t    null;
    /** @brief The IB representation of a memory region. */
    case NNTI_TRANSPORT_IBVERBS: NNTI_ib_rdma_addr_p_t      ib;
    /** @brief The Cray UGNI representation of a memory region. */
    case NNTI_TRANSPORT_UGNI:    NNTI_ugni_rdma_addr_p_t    ugni;
    /** @brief The MPI representation of a memory region. */
    case NNTI_TRANSPORT_MPI:     NNTI_mpi_rdma_addr_p_t     mpi;
};
#else
union NNTI_remote_addr_p_t {
    /** @brief The NULL representation of a memory region. */
    NNTI_null_rdma_addr_p_t    null;
    /** @brief The IB representation of a memory region. */
    NNTI_ib_rdma_addr_p_t      ib;
    /** @brief The Cray UGNI representation of a memory region. */
    NNTI_ugni_rdma_addr_p_t    ugni;
    /** @brief The MPI representation of a memory region. */
    NNTI_mpi_rdma_addr_p_t     mpi;
};
#endif


/***********  Buffer Type  ***********/

/**
 * @brief handle to a memory buffer prepared by NNTI_register_memory
 *
 * The <tt>NNTI_buffer_t</tt> structure contains the
 * location of a buffer on the network.  This is all the info
 *  a peer needs to put/get this buffer.
 */
struct NNTI_buffer_p_t {
    /** @brief Segments that compose a complete buffer. */
    NNTI_remote_addr_p_t buffer;

    uint8_t flags;
};
