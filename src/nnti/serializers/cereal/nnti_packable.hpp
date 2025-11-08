// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef NNTI_PACKABLE_HPP
#define NNTI_PACKABLE_HPP

#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/portable_binary.hpp>

#include "nnti/nntiConfig.h"
#include "nnti/nnti_types.h"


/***********  TCP/IP address types  ***********/

/**
 * @brief Binary encoding of a TCP/IP host address.
 *
 * The <tt>\ref NNTI_ip_addr</tt> type identifies a particular node.
 */
using NNTI_ip_addr = uint32_t;

/**
 * @brief TCP port in NBO.
 *
 * The <tt>\ref NNTI_tcp_addr</tt> type identifies a particular port.
 */
using NNTI_tcp_port = uint16_t;


/***********  NULL Process Types  ***********/

/**
 * @brief Remote process identifier for the NULL transport.
 *
 */
struct NNTI_null_process_p_t {
    int i;  /* unused, but empty structs are not allowed */

    template<class Archive>
    void serialize(Archive & archive)
    {
        archive( i );
    }
};


/***********  IB Process Types  ***********/

/**
 * @brief Remote process identifier for IB.
 *
 * The <tt>\ref NNTI_ib_process_p_t</tt> identifies a particular process
 * on a particular node.  If a connection has been established to the
 * represented process, then that connection is identified by 'qp_num'.
 */
struct NNTI_ib_process_p_t {
    /** @brief IP address encoded in Network Byte Order */
    NNTI_ip_addr  addr;
    /** @brief TCP port encoded in Network Byte Order */
    NNTI_tcp_port port;

    template<class Archive>
    void serialize(Archive & archive)
    {
        archive( addr, port );
    }
};


/***********  Gemini Process Types  ***********/

/**
 * @brief The instance ID of a Gemini process.
 *
 * The <tt>\ref NNTI_inst_id</tt> type identifies a particular process
 * within a communication domain.
 */
using NNTI_instance_id = uint32_t;

/**
 * @brief Remote process identifier for Gemini.
 *
 * The <tt>\ref NNTI_ugni_process_p_t</tt> identifies a particular process
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

    template<class Archive>
    void serialize(Archive & archive)
    {
        archive( addr, port, inst_id );
    }
};


/***********  MPI Process Types  ***********/

/**
 * @brief Remote process identifier for MPI.
 *
 * The <tt>\ref NNTI_mpi_process_p_t</tt> identifies a particular process
 * on a particular node.
 */
struct NNTI_mpi_process_p_t {
    /** @brief MPI rank. */
    int rank;

    template<class Archive>
    void serialize(Archive & archive)
    {
        archive( rank );
    }
};


/***********  Local Process Types  ***********/

/**
 * @brief Remote process identifier for the Local transport.
 *
 */
struct NNTI_local_process_p_t {
    int i;  /* unused, but empty structs are not allowed */

    template<class Archive>
    void serialize(Archive & archive)
    {
        archive( i );
    }
};


/***********  Remote Process Union  ***********/

/**
 * @brief A structure to represent a remote processes.
 *
 * The <tt>NNTI_remote_process_p_t</tt> structure contains the
 * transport specific info needed to identify a process running
 * on a remote node.
 */
struct NNTI_remote_process_p_t {
    NNTI_transport_id_t transport_id;
    union {
        /** @brief The NULL representation of a process on the network. */
        NNTI_null_process_p_t    null;
        /** @brief The IB representation of a process on the network. */
        NNTI_ib_process_p_t      ib;
        /** @brief The Cray UGNI representation of a process on the network. */
        NNTI_ugni_process_p_t    ugni;
        /** @brief The MPI representation of a process on the network. */
        NNTI_mpi_process_p_t     mpi;
    } NNTI_remote_process_p_t_u;

    template<class Archive>
    void serialize( Archive & archive )
    {
        archive( transport_id );
        switch( transport_id )
        {
            case NNTI_TRANSPORT_NULL:
                archive( NNTI_remote_process_p_t_u.null );
                break;
                /** @brief The IB representation of a process on the network. */
            case NNTI_TRANSPORT_IBVERBS:
                archive( NNTI_remote_process_p_t_u.ib );
                break;
                /** @brief The Cray UGNI representation of a process on the network. */
            case NNTI_TRANSPORT_UGNI:
                archive( NNTI_remote_process_p_t_u.ugni );
                break;
                /** @brief The MPI representation of a process on the network. */
            case NNTI_TRANSPORT_MPI:
                archive( NNTI_remote_process_p_t_u.mpi );
                break;
        }
    }
};


/***********  Peer Type  ***********/

typedef uint64_t NNTI_process_id_p_t;

/**
 * @brief Handle to an NNTI process.
 *
 * This is the datatype used by NNTI clients to reference another process.
 * Use this handle to move data to/from the process.
 */
struct NNTI_peer_p_t {
    /** @brief binary encoding of a process's URL */
    NNTI_process_id_p_t pid;
    
    /** @brief binary encoding of a process on the network */
    NNTI_remote_process_p_t peer;

    template<class Archive>
    void serialize(Archive & archive)
    {
        archive( pid, peer );
    }
};


/***********  NULL RDMA Address Types  ***********/

/**
 * @brief RDMA address used for the NULL transport.
 */
struct NNTI_null_rdma_addr_p_t {
    int i;  /* unused, but empty structs are not allowed */

    template<class Archive>
    void serialize(Archive & archive)
    {
        archive( i );
    }
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

    template<class Archive>
    void serialize(Archive & archive)
    {
        archive( buf, key, size );
    }
};


/***********  Gemini RDMA Address Types  ***********/

struct NNTI_ugni_mem_hdl_p_t {
    uint64_t qword1;
    uint64_t qword2;

    template<class Archive>
    void serialize(Archive & archive)
    {
        archive( qword1, qword2 );
    }
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

    template<class Archive>
    void serialize(Archive & archive)
    {
        archive( buf, size, mem_hdl );
    }
};


/***********  MPI RDMA Address Types  ***********/

/**
 * @brief Definition for match bits in MPI.
 */
using NNTI_match_bits = uint64_t;

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

    template<class Archive>
    void serialize(Archive & archive)
    {
        archive( cmd_tag, get_data_tag, put_data_tag, atomic_data_tag, buf, size );
    }
};


/***********  Local RDMA Address Types  ***********/

/**
 * @brief RDMA address used for the Local transport.
 */
struct NNTI_local_rdma_addr_p_t {
    int i;  /* unused, but empty structs are not allowed */

    template<class Archive>
    void serialize(Archive & archive)
    {
        archive( i );
    }
};


/***********  Remote Address Union  ***********/

/**
 * @brief A structure to represent a remote memory region.
 *
 * The <tt>NNTI_remote_addr_t</tt> structure contains the
 * transport specific info needed to identify a memory region
 * on a remote node.
 */
struct NNTI_remote_addr_p_t {
    NNTI_transport_id_t transport_id;
    union {
        /** @brief The NULL representation of a memory region. */
        NNTI_null_rdma_addr_p_t    null;
        /** @brief The IB representation of a memory region. */
        NNTI_ib_rdma_addr_p_t      ib;
        /** @brief The Cray UGNI representation of a memory region. */
        NNTI_ugni_rdma_addr_p_t    ugni;
        /** @brief The MPI representation of a memory region. */
        NNTI_mpi_rdma_addr_p_t     mpi;
    } NNTI_remote_addr_p_t_u;

    template<class Archive>
    void serialize( Archive & archive )
    {
        archive( transport_id );
        switch( transport_id )
        {
            case NNTI_TRANSPORT_NULL:
                archive( NNTI_remote_addr_p_t_u.null );
                break;
                /** @brief The IB representation of a process on the network. */
            case NNTI_TRANSPORT_IBVERBS:
                archive( NNTI_remote_addr_p_t_u.ib );
                break;
                /** @brief The Cray UGNI representation of a process on the network. */
            case NNTI_TRANSPORT_UGNI:
                archive( NNTI_remote_addr_p_t_u.ugni );
                break;
                /** @brief The MPI representation of a process on the network. */
            case NNTI_TRANSPORT_MPI:
                archive( NNTI_remote_addr_p_t_u.mpi );
                break;
        }
    }
};

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

    template<class Archive>
    void serialize(Archive & archive)
    {
        archive( buffer, flags );
    }
};

#endif /* NNTI_PACKABLE_HPP */
