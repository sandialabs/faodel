// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_RESULTCOLLECTOR_HH
#define FAODEL_RESULTCOLLECTOR_HH

#include <atomic>

#include "kelpie/common/Types.hh"

namespace kelpie {

/**
 * @brief A class for gathering the results of multiple asynchronous operations
 *
 * A ResultCollector is used when you want to launch a known-number of
 * asynchronous pool operations and you want to block until all operations
 * complete. Create a ResultCollector, pass it to all the asynchronous
 * operations you perform with a pool, and then call Sync() to block until
 * all ops complete. Each op will insert any returned information in
 * a vector of results that users can query after the operation completes.
 */
class ResultCollector {
public:
  ResultCollector(int num_requests);
  ~ResultCollector();

  void fn_publish_callback(kelpie::rc_t result, kelpie::object_info_t &info );
  void fn_want_callback(bool success, kelpie::Key, lunasa::DataObject user_ldo, const object_info_t &info);
  void fn_compute_callback(kelpie::rc_t result, Key key, lunasa::DataObject user_ldo);

  void Sync();

  //Define what type of operation this operation was
  enum class RequestType : char {
    Publish=1,
    Want=2,
    Compute=3
  };

  //Structure for passing back any result information from the request
  struct Result {
    RequestType        request_type;  //!< Type of operation (publish, want, compute)
    kelpie::rc_t       rc;            //!< The return code sent back from remote node
    object_info_t      info;          //!< Information the remote node returned about the object
    kelpie::Key        key;           //!< The key that was used in this request
    lunasa::DataObject ldo;           //!< An object returned by the remote node
  };

  Result *results;                    //!< A collection of results for the user to examine


private:
  int expected_items;
  std::atomic<int> items_left;

};



} // namespace kelpie

#endif //FAODEL_RESULTCOLLECTOR_HH
