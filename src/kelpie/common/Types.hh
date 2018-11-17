// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef KELPIE_TYPES_HH
#define KELPIE_TYPES_HH

#include <vector>
#include <map>
#include <memory>
#include <functional>

#include "lunasa/DataObject.hh"

#include "kelpie/Key.hh"


//Forward references.. plugg
namespace faodel {
class ResourceURL;
}


namespace kelpie {


//Forward references for Kelpie things
class Key;
class Pool;
class PoolBase;
class LocalKVCell;
class LocalKVRow;

namespace internal {
class IomBase;
}

typedef int rc_t;  //!< Kelpie functions return standad return codes, plus some extras

//Ok results
const rc_t KELPIE_OK        =  0;    //!< Function successful
const rc_t KELPIE_WAITING   =  1;    //!< Item was dispatched, but has not resolved yet
const rc_t KELPIE_EEXIST    =  2;    //!< Item not written because it already exists
const rc_t KELPIE_RECHECK   =  3;    //!< Operation worked, but may have caveats the caller should check

//Fail
const rc_t KELPIE_ENOENT    = -2;    //!< Item doesn't exist
const rc_t KELPIE_EIO       = -5;     //!< Input/output error
const rc_t KELPIE_NXIO      = -6;    //!< Not configured
const rc_t KELPIE_EINVAL    = -22;   //!< Bad input
const rc_t KELPIE_ETIMEDOUT = -110;  //!< Timed out
const rc_t KELPIE_EOVERFLOW = -84;   //!< Value too large to be stored
const rc_t KELPIE_TODO      = -1000; //!< Hit something that isn't yet implemented in Kelpie.

//Network fails
const rc_t KELPIE_EBADRPC = -200;    //!< Network told us we had a bad rpc
const rc_t KELPIE_EREMOTE = -201;    //!< RPC completed ok, but remote sent an errorcode


typedef uint32_t iom_hash_t;         //!< Hash used to identify a specific IOM

/**
 * @brief An enumerated type that defines how available a requested item is
 */
enum class Availability : uint8_t {
  Unavailable     = 0,               //!< This item is not known here
  Requested       = 1,               //!< This item is not available, but has been requested
  MixedConditions = 2,               //!< The multiple items in this request have different availabilities

  InLocalMemory   = 3,               //!< This item is available in memory at this node
  InRemoteMemory  = 4,               //!< This item is available in remote memory
  InNVM           = 5,               //!< This item is stored somewhere in non-volatile memory
  InDisk          = 6                //!< This item is stored on disk
};
std::string availability_to_string(const Availability &a);

/**
 * @brief Information about a particular row of items
 */
struct kv_row_info_t {
  ssize_t             row_bytes;              //!< The current total for how big the row is
  uint32_t            num_cols_in_row;        //!< How many columns are filled in this row
  uint32_t            num_row_receiver_nodes; //!< How many nodes are waiting on updates to this row
  uint32_t            num_row_dependencies;   //!< How many actions are waiting if this row has activities
  Availability        availability;           //!< Where the row is available
  std::string str();
  void ChangeAvailabilityFromLocalToRemote();
};

/**
 * @brief Information about a particular column of data
 */
struct kv_col_info_t {
  faodel::nodeid_t    node_origin;            //!< What node generated this item
  ssize_t             num_bytes;              //!< How big this item is
  uint32_t            num_col_receiver_nodes; //!< How many nodes are waiting on updates to this column
  uint32_t            num_col_dependencies;   //!< How many local actions are waiting on this col
  Availability        availability;           //!< Whether item is available locally
  std::string str();
  void ChangeAvailabilityFromLocalToRemote();
};


typedef uint8_t pool_behavior_t;
/**
 * @brief Provides instructions on what actions to take in different stages of communication pipeline
 */
struct PoolBehavior {
  //Individual actions
  static constexpr pool_behavior_t WriteToLocal  = 0x01; //!< Publish writes to local memory
  static constexpr pool_behavior_t WriteToRemote = 0x02; //!< Publish writes to remote memory
  static constexpr pool_behavior_t WriteToIOM    = 0x04; //!< Publish writes to remote IOM
  static constexpr pool_behavior_t ReadToLocal   = 0x08; //!< Want/Need writes to local memory
  static constexpr pool_behavior_t ReadToRemote  = 0x10; //!< Want/Need writes to remote memory

  //Common labels (combine individual actions)
  static constexpr pool_behavior_t WriteAround   = WriteToIOM; //!< Publish only to IOM (skip local/remote memory)
  static constexpr pool_behavior_t WriteToAll    = WriteToLocal | WriteToRemote | WriteToIOM; //!< Publish to all levels
  static constexpr pool_behavior_t ReadToNone    = 0x00; //!< Want/Need isn't cached in local/remote memory
  static constexpr pool_behavior_t NoAction      = 0x00; //!< Don't take any action
  static constexpr pool_behavior_t TODO          = 0x00; //!< Not implemented. Should revisit

  //Default behaviors: assume ioms are fire and forget
  static constexpr pool_behavior_t DefaultBaseClass  = WriteToAll | ReadToLocal |ReadToRemote; //!< Cache everywhere
  static constexpr pool_behavior_t DefaultIOM        = WriteToIOM | ReadToNone; //!< Don't cache writes/reads
  static constexpr pool_behavior_t DefaultLocalIOM   = WriteToIOM | ReadToNone; //!< Don't cache writes/reads
  static constexpr pool_behavior_t DefaultRemoteIOM  = WriteToIOM | ReadToRemote; //!< Only cache reads on remote side
  static constexpr pool_behavior_t DefaultCachingIOM = WriteToAll | ReadToLocal | ReadToRemote;  //!< Cache everywhere

  static pool_behavior_t ChangeRemoteToLocal(pool_behavior_t f);
  static pool_behavior_t ParseString(std::string parse_line);
  static std::string GetString(pool_behavior_t f);
};


//Pool callbacks
using fn_publish_callback_t = std::function<void (kelpie::rc_t result, kv_row_info_t &ri, kv_col_info_t &ci )>;
using fn_want_callback_t    = std::function<void (bool success, Key key, lunasa::DataObject user_ldo, const kv_row_info_t &ri, const kv_col_info_t &ci)>;

using fn_opget_result_t     = std::function<void (bool success, Key &key, lunasa::DataObject &ldo)>;       //!< Lambda for passing back an op get

//LocalKV Operators: advanced users only
using fn_column_op_t        = std::function<rc_t (LocalKVRow &, LocalKVCell &, bool previously_existed)>;  //!< Lambda operator for a column operation
using fn_row_op_t           = std::function<rc_t (LocalKVRow &,  bool previously_existed)>;                //!< Lambda operator for doing a row operation

//Pool creation function pointer
using fn_PoolCreate_t       = std::function<std::shared_ptr<PoolBase> (
                                                  const faodel::ResourceURL &pool_url)>;                   //!< Lambda for creating a pool from a url

//Iom driver constructor function pointer
using fn_IomConstructor_t   = std::function<internal::IomBase * (
                                                  std::string name,
                                                  const std::map<std::string, std::string> &settings)>;    //!< Lambda for creating a new IOM driver

}  // namespace kelpie

#endif

