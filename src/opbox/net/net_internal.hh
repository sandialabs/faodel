// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef OPBOX_NET_INTERNAL_HH
#define OPBOX_NET_INTERNAL_HH

/*
 * This header file declares the private interface to the network
 * modules.  As such, this header should not be installed.
 *
 * The implementation must be defined in the individual network
 * modules.
 *
 */


namespace opbox {
namespace net {
namespace internal {

/*
 * @brief This method returns the offset of the NetBufferRemote.
 *
 * @param[in]  nbr  the nbr to query
 * @return the offset of the NetBufferRemote
 */
uint32_t GetOffset(
    opbox::net::NetBufferRemote *nbr);        // in
/*
 * @brief This method returns the length of the NetBufferRemote.
 *
 * @param[in]  nbr  the nbr to query
 * @return the length of the NetBufferRemote
 */
uint32_t GetLength(
    opbox::net::NetBufferRemote *nbr);        // in
/*
 * @brief This method increases the offset of the NetBufferRemote.
 *
 * @param[in]  nbr     the nbr to modify
 * @param[in]  addend  the offset adjustment
 * @return 0 if success, otherwise nonzero
 *
 * This method increases the offset of the NetBufferRemote.  As a
 * side effect, the length will be reduced by the same amount so
 * that the window doesn't slide.  addend must be greater than or
 * equal to zero and less than the current length.
 */
int IncreaseOffset(
    opbox::net::NetBufferRemote *nbr,         // in
    uint32_t              addend);     // in
/*
 * @brief This method decreases the length of the NetBufferRemote.
 *
 * @param[in]  nbr         the nbr to modify
 * @param[in]  subtrahend  the length adjustment
 * @return 0 if success, otherwise nonzero
 *
 * This method decreases the length of the NetBufferRemote.  The
 * offset is not adjusted.  subtrahend must be greater than or
 * equal to zero and less than the current length.
 */
int DecreaseLength(
    opbox::net::NetBufferRemote *nbr,         // in
    uint32_t              subtrahend); // in
/*
 * @brief This method sets the length of the NetBufferRemote.
 *
 * @param[in]  nbr     the nbr to modify
 * @param[in]  length  the new length
 * @return 0 if success, otherwise nonzero
 *
 * This method sets the length of the NetBufferRemote.  The offset
 * is not adjusted.  subtrahend must be greater than zero and less
 * than the current length.
 */
int TrimToLength(
    opbox::net::NetBufferRemote *nbr,         // in
    uint32_t              length);     // in

}
}
}

#endif // OPBOX_NET_INTERNAL_HH
