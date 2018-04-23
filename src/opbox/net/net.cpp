// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include "opbox/net/net.hh"
#include "opbox/net/net_internal.hh"

#include <iomanip>
#include <sstream>

/*
 * From an LDO, create a NetBufferLocal and NetBufferRemote.  The
 * NetBufferRemote will be constructed with the specified offset
 * and length.
 *
 * Note: The offset returned by nbr->GetOffset() may not equal the
 * offset specified here even if it is not adjusted with
 * IncreaseOffset().  @c remote_offset_adjust is relative to the
 * base address of LDO (the 0-byte of the local header).  The
 * offset returned by nbr->GetOffset() is relative to the base
 * address of Lunasa memory allocation the LDO is carved from.
 *
 * ldo->GetBaseRdmaHandle() calculates the difference between these
 * two base addresses and returns it in the offset output parameter.
 * The NetBufferRemote offset is the sum of LDO offset and @c
 * remote_offset_adjust.
 */
int opbox::net::GetRdmaPtr(
    lunasa::DataObject    *ldo,                  // in
    const uint32_t         remote_offset_adjust, // in
    const uint32_t         remote_length,        // in
    opbox::net::NetBufferLocal  **nbl,           // out
    opbox::net::NetBufferRemote  *nbr)           // out
{
    if (remote_offset_adjust < 0) {
        return -1;
    }
    uint32_t offset=0;
    int rc = ldo->GetBaseRdmaHandle((void**)nbl, offset);
    if (rc == 0) {
        (*nbl)->makeRemoteBuffer(offset+remote_offset_adjust, remote_length, nbr);
    }
    return rc;
}

/*
 * From an LDO, create a NetBufferLocal and NetBufferRemote.  The
 * NetBufferRemote will be constructed with an offset that skips the
 * local header and the specified length.
 */
int opbox::net::GetRdmaPtr(
    lunasa::DataObject    *ldo,                  // in
    const uint32_t         remote_length,        // in
    opbox::net::NetBufferLocal  **nbl,           // out
    opbox::net::NetBufferRemote  *nbr)           // out
{
    return opbox::net::GetRdmaPtr(ldo, ldo->GetLocalHeaderSize(), remote_length, nbl, nbr);
}

/*
 * From an LDO, create a NetBufferLocal and NetBufferRemote.  The
 * NetBufferRemote will be constructed with an offset that skips the
 * local header and extends to the end of the LDO.
 */
int opbox::net::GetRdmaPtr(
    lunasa::DataObject    *ldo,                  // in
    opbox::net::NetBufferLocal  **nbl,           // out
    opbox::net::NetBufferRemote  *nbr)           // out
{
    uint32_t length = ldo->GetHeaderSize() + ldo->GetMetaSize() + ldo->GetDataSize();
    return opbox::net::GetRdmaPtr(ldo, ldo->GetLocalHeaderSize(), length, nbl, nbr);
}

uint32_t opbox::net::NetBufferRemote::GetOffset(void)                { return opbox::net::internal::GetOffset(this); }
uint32_t opbox::net::NetBufferRemote::GetLength(void)                { return opbox::net::internal::GetLength(this); }
int opbox::net::NetBufferRemote::IncreaseOffset(uint32_t addend)     { return opbox::net::internal::IncreaseOffset(this, addend); }
int opbox::net::NetBufferRemote::DecreaseLength(uint32_t subtrahend) { return opbox::net::internal::DecreaseLength(this, subtrahend); }
int opbox::net::NetBufferRemote::TrimToLength(uint32_t length)       { return opbox::net::internal::TrimToLength(this, length); }

std::string opbox::net::NetBufferRemote::str() {
  std::stringstream ss;
  ss<<"NBR$0x";
  for(int i=0; i<MAX_NET_BUFFER_REMOTE_SIZE; i++) {
    ss<< std::uppercase << std::hex << std::setfill('0') << std::setw(2)
      << ((unsigned int)(data[i] & 0x0FF));
  }
  return ss.str();
}
