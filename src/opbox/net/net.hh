// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef OPBOX_NET_HH
#define OPBOX_NET_HH

/*
 * This header file declares the public interface to the network
 * modules.  Opbox should only use the API declared here.
 *
 */


#include <cstdint>
#include <functional>
#include <string>
#include <sstream>



#include "faodel-common/Configuration.hh"
#include "faodel-common/Bootstrap.hh"
#include "faodel-common/BootstrapInterface.hh"
#include "faodel-common/NodeID.hh"

#include "lunasa/DataObject.hh"

#include "opbox/common/Types.hh"
#include "opbox/common/OpArgs.hh"

#include "opbox/net/peer.hh"
#include "opbox/net/nbr.hh"

using namespace opbox;


namespace opbox {
namespace net {

/**
 * @brief Attributes of the network module.
 */
struct Attrs
{
    /** @brief The hostname the network module is bound to. */
    char     bind_hostname[128];
    /** @brief The port the network module is listening on. */
    char     listen_port[6];
    /** @brief The Maximum Transfer Unit of the underlying network. */
    uint32_t mtu;
    /** @brief The maximum size the network module can send in an eager message. */
    uint32_t max_eager_size;
};

/**
 * @brief Enumerator of atomic operation.
 */
enum AtomicOp
{
    /** @brief Perform a fetch-add atomic. */
    FetchAdd,
    /** @brief Perform a compare-swap atomic. */
    CompareSwap
};

/**
 * @brief A handle to a window into a remote RDMA buffer.
 *
 * This is an opaque handle to a window into a network buffer that
 * can be sent to other processes in order to perform RDMA operations.
 */
struct NetBufferRemote {
    /** @brief The NetBufferLocal gets serialized here. */
    char data[MAX_NET_BUFFER_REMOTE_SIZE];

    /** @brief Return the offset of the window into the remote buffer. */
    uint32_t GetOffset(void);
    /** @brief Return the size of the window into the remote buffer. */
    uint32_t GetLength(void);
    /** @brief Increase the offset of the window by @c addend. */
    int IncreaseOffset(uint32_t addend);
    /** @brief Decrease the length of the window by @c subtrahend. */
    int DecreaseLength(uint32_t subtrahend);
    /** @brief Set the length of the window to @c length. */
    int TrimToLength(uint32_t length);

    /** @brief convert to a hex string for debugging */
    std::string str();
  
    //Serdes function, for when user packs NBR with Boost
    template <typename Archive>
    void serialize(Archive &ar, const unsigned int version){
    ar & data;
  }
};

/**
 * @brief A handle to a local network buffer.
 *
 * This is an opaque handle to a network buffer on the node
 * where it was created.  This is only an interface.  Each
 * network module has it's own implementation.
 */
struct NetBufferLocal {
    /** @brief Generate a NetBufferRemote from this NetBufferLocal. */
    virtual void makeRemoteBuffer (
        size_t           remote_offset,      // in
        size_t           remote_length,      // in
        NetBufferRemote *remote_buffer) = 0; // out
};


/**
 * @brief Get the name of the active network module.
 */
std::string GetDriverName();

/**
 * @brief Initialize the network module using @c config.
 */
void Init(const faodel::Configuration &config);
/**
 * @brief Start the network module.
 */
void Start();
/**
 * @brief Shutdown the network module.
 */
void Finish();

/**
 * @brief Register a callback that is invoked for each message received.
 */
void RegisterRecvCallback(
    std::function<void(opbox::net::peer_ptr_t, opbox::message_t*)> recv_cb);

/**
 * @brief Get the node id of this process.
 *
 * Deprecated since Opbox now has this functionality (@c opbox::GetMyID()).
 */
faodel::nodeid_t GetMyID();
/**
 * @brief Convert a node id to a peer using the network module's map.
 *
 * Deprecated since Opbox is responsible for maintaining it's own map if desired.
 */
faodel::nodeid_t ConvertPeerToNodeID(peer_t *peer);
/**
 * @brief Convert a peer to a node id using the network module's map.
 *
 * Deprecated since Opbox is responsible for maintaining it's own map if desired.
 */
opbox::net::peer_ptr_t   ConvertNodeIDToPeer(faodel::nodeid_t nodeid);

/**
 * @brief Get the attributes of the network module.
 * @param[out] attrs Where attribute info is copied to
 */
void GetAttrs(Attrs *attrs);

/**
 * @brief Prepare for communication with the peer identified by peer_addr and peer_port.
 *
 * \param[out] peer       A handle to a peer that can be used for network operations.
 * \param[in]  peer_addr  The hostname or IP address the peer is bound to.
 * \param[in]  peer_port  The port the peer is listening on.
 * \return A result code
 */
int Connect(
    peer_ptr_t *peer,
    const char *peer_addr,
    const char *peer_port);
/**
 * @brief Prepare for communication with the peer identified by node ID.
 *
 * \param[out] peer         A handle to a peer that can be used for network operations.
 * \param[in]  peer_nodeid  The node ID of the peer.
 * \return A result code
 */
int Connect(
    peer_ptr_t        *peer,
    faodel::nodeid_t  peer_nodeid);

int Disconnect(
    peer_ptr_t peer);
int Disconnect(
    faodel::nodeid_t peer_nodeid);

/*
 * @brief Create a DataObject that can be used for zero copy sends.
 *
 * \param[in]  size  The size in bytes of the data section of the LDO.
 * \return A pointer to the new LDO.
 */
lunasa::DataObject NewMessage(uint64_t size);
/*
 * @brief Explicitly release a message DataObject after use.
 *
 * \param[in]  msg  The message to release.
 */
void ReleaseMessage(lunasa::DataObject msg);

/*
 * @brief From an LDO, create a NetBufferLocal and NetBufferRemote.  The
 * NetBufferRemote will be constructed with the specified offset
 * and length.
 */
int GetRdmaPtr(
    lunasa::DataObject    *ldo,                  // in
    const uint32_t         remote_offset_adjust, // in
    const uint32_t         remote_length,        // in
    opbox::net::NetBufferLocal  **nbl,           // out
    opbox::net::NetBufferRemote  *nbr);          // out

/*
 * @brief From an LDO, create a NetBufferLocal and NetBufferRemote.  The
 * NetBufferRemote will be constructed with an offset that skips the
 * local header and the specified length.
 */
int GetRdmaPtr(
    lunasa::DataObject    *ldo,                  // in
    const uint32_t         remote_length,        // in
    opbox::net::NetBufferLocal  **nbl,           // out
    opbox::net::NetBufferRemote  *nbr);          // out

/*
 * @brief From an LDO, create a NetBufferLocal and NetBufferRemote.  The
 * NetBufferRemote will be constructed with an offset that skips the
 * local header and extends to the end of the LDO.
 */
int GetRdmaPtr(
    lunasa::DataObject    *ldo,                  // in
    opbox::net::NetBufferLocal  **nbl,           // out
    opbox::net::NetBufferRemote  *nbr);          // out

typedef std::function<WaitingType (OpArgs *args)> lambda_net_update_t;

/**
 * @brief Send a message to the peer identified by @c peer.
 *
 * \param[in] peer  A handle to the tagret peer.
 * \param[in] msg   The LDO to send.
 *
 * Send the entire msg to peer.  After the send completes
 * (successfully or not), release msg.
 * opbox gets no feedback about this operations (fire and forget).
 */
void SendMsg(
    peer_ptr_t           peer,
    lunasa::DataObject   msg);

/**
 * @brief Send a message to the peer identified by @c peer.
 *
 * \param[in] peer      A handle to the target peer.
 * \param[in] msg       The LDO to send.
 * \param[in] user_cb   The callback to invoke when events are generated from this send.
 *
 * Send the entire msg to peer.  user_cb is invoked after the send
 * completes.  msg is not automatically released.
 */
void SendMsg(
    peer_ptr_t           peer,
    lunasa::DataObject   msg,
    lambda_net_update_t  user_cb);

/**
 * @brief Read an entire LDO from @c peer.
 *
 * \param[in] peer           A handle to the target peer.
 * \param[in] remote_buffer  The source of the GET.
 * \param[in] local_ldo      The destination of the GET.
 * \param[in] user_cb        The callback to invoke when events are generated from this GET.
 *
 * Execute a one-sided read from remote_buffer on peer to local_ldo.
 * Both remote and local offsets are zero.  The length of the read
 * is the smaller of remote_buffer and local_ldo.  user_cb is invoked after
 * the read completes.
 */
void Get(
    peer_ptr_t           peer,
    NetBufferRemote     *remote_buffer,
    lunasa::DataObject   local_ldo,
    lambda_net_update_t  user_cb);


/**
 * @brief Read a subset of an LDO from @c peer.
 *
 * \param[in] peer           A handle to the target peer.
 * \param[in] remote_buffer  The source of the GET.
 * \param[in] remote_offset  The offset into the source buffer.
 * \param[in] local_ldo      The destination of the GET.
 * \param[in] local_offset   The offset into the destination buffer.
 * \param[in] length         The length of the GET.
 * \param[in] user_cb        The callback to invoke when events are generated from this GET.
 *
 * Execute a one-sided read from remote_buffer on peer to local_ldo.
 * user_cb is invoked after the read completes.
 */
void Get(
    peer_ptr_t           peer,
    NetBufferRemote     *remote_buffer,
    uint64_t             remote_offset,
    lunasa::DataObject   local_ldo,
    uint64_t             local_offset,
    uint64_t             length,
    lambda_net_update_t  user_cb);


/**
 * @brief Write an entire LDO to @c peer.
 *
 * \param[in] peer           A handle to the target peer.
 * \param[in] local_ldo      The source of the PUT.
 * \param[in] remote_buffer  The destination of the PUT.
 * \param[in] user_cb        The callback to invoke when events are generated from this PUT.
 *
 * Execute a one-sided write to remote_buffer on peer from local_ldo.
 * Both remote and local offsets are zero.  The length of the write
 * is the smaller of remote_buffer and local_ldo.  user_cb is invoked after
 * the write completes.
 */
void Put(
    peer_ptr_t           peer,
    lunasa::DataObject   local_ldo,
    NetBufferRemote     *remote_buffer,
    lambda_net_update_t  user_cb);


/**
 * @brief Write a subset of an LDO to @c peer.
 *
 * \param[in] peer           A handle to the target peer.
 * \param[in] local_ldo      The source of the PUT.
 * \param[in] local_offset   The offset into the source buffer.
 * \param[in] remote_buffer  The destination of the PUT.
 * \param[in] remote_offset  The offset into the destination buffer.
 * \param[in] length         The length of the PUT.
 * \param[in] user_cb        The callback to invoke when events are generated from this PUT.
 *
 * Execute a one-sided write to remote_buffer on peer from local_ldo.
 * user_cb is invoked after the write completes.
 */
void Put(
    peer_ptr_t           peer,
    lunasa::DataObject   local_ldo,
    uint64_t             local_offset,
    NetBufferRemote     *remote_buffer,
    uint64_t             remote_offset,
    uint64_t             length,
    lambda_net_update_t  user_cb);

/**
 * @brief Perform an atomic operation on an LDO at @c peer.
 *
 * \param[in] peer           A handle to the target peer.
 * \param[in] local_ldo      The result of the atomic buffer.
 * \param[in] local_offset   The offset into the result buffer.
 * \param[in] remote_buffer  The target of the atomic operation.
 * \param[in] remote_offset  The offset into the target buffer.
 * \param[in] length         The length of the atomic operation.
 * \param[in] user_cb        The callback to invoke when events are generated from this PUT.
 *
 * Execute a one-sided atomic operation on peer at remote_buffer.
 * length is the width of the operands in bits.  user_cb is
 * invoked after the atomic completes.
 */
void Atomic(
    peer_ptr_t           peer,
    AtomicOp             op,
    lunasa::DataObject   local_ldo,
    uint64_t             local_offset,
    NetBufferRemote     *remote_buffer,
    uint64_t             remote_offset,
    uint64_t             length,
    lambda_net_update_t  user_cb);

/**
 * @brief Perform an atomic operation on an LDO at @c peer.
 *
 * \param[in] peer           A handle to the target peer.
 * \param[in] local_ldo      The result of the atomic buffer.
 * \param[in] local_offset   The offset into the result buffer.
 * \param[in] remote_buffer  The target of the atomic operation.
 * \param[in] remote_offset  The offset into the target buffer.
 * \param[in] length         The length of the atomic operation.
 * \param[in] operand        The operand of the atomic operation.
 * \param[in] user_cb        The callback to invoke when events are generated from this PUT.
 *
 * Execute a one-sided atomic operation with one operand on peer at
 * remote_buffer.  length is the width of the operands in bits.  user_cb is
 * invoked after the atomic completes.
 */
void Atomic(
    peer_ptr_t           peer,
    AtomicOp             op,
    lunasa::DataObject   local_ldo,
    uint64_t             local_offset,
    NetBufferRemote     *remote_buffer,
    uint64_t             remote_offset,
    uint64_t             length,
    int64_t              operand,
    lambda_net_update_t  user_cb);

/**
 * @brief Perform an atomic operation on an LDO at @c peer.
 *
 * \param[in] peer           A handle to the target peer.
 * \param[in] local_ldo      The result of the atomic buffer.
 * \param[in] local_offset   The offset into the result buffer.
 * \param[in] remote_buffer  The target of the atomic operation.
 * \param[in] remote_offset  The offset into the target buffer.
 * \param[in] length         The length of the atomic operation.
 * \param[in] operand1       The first operand of the atomic operation.
 * \param[in] operand2       The second operand of the atomic operation.
 * \param[in] user_cb        The callback to invoke when events are generated from this PUT.
 *
 * Execute a one-sided atomic operation with two operands on peer at
 * remote_buffer.  length is the width of the operands in bits.  user_cb is
 * invoked after the atomic completes.
 */
void Atomic(
    peer_ptr_t           peer,
    AtomicOp             op,
    lunasa::DataObject   local_ldo,
    uint64_t             local_offset,
    NetBufferRemote     *remote_buffer,
    uint64_t             remote_offset,
    uint64_t             length,
    int64_t              operand1,
    int64_t              operand2,
    lambda_net_update_t  user_cb);

} // namespace net
} // namespace opbox

#endif // OPBOX_NET_HH
