// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <thread>

#include "kelpie/pools/ResultCollector.hh"

using namespace std;
using namespace faodel;

namespace kelpie {

/**
 * @brief ResultCollector constructor
 * @param[in] num_requests The number of requests that will write to this collector
 */
ResultCollector::ResultCollector(int num_requests)
  : expected_items(num_requests), items_left(num_requests) {

  results = new Result[num_requests];
}
ResultCollector::~ResultCollector() {
  delete [] results;
}

void ResultCollector::fn_publish_callback(kelpie::rc_t result, object_info_t &info ) {
  int spot = expected_items - (items_left--);
  results[spot].request_type = RequestType::Publish;
  results[spot].rc = result;
  results[spot].info = info;
  if(spot==expected_items-1) items_left--; //Signal we've recorded last item
}
void ResultCollector::fn_want_callback(bool success, kelpie::Key key, lunasa::DataObject user_ldo, const object_info_t &info) {
  int spot = expected_items - (items_left--);
  results[spot].request_type = RequestType::Want;
  results[spot].rc = (success) ? KELPIE_OK : KELPIE_EINVAL;
  results[spot].info = info;
  results[spot].key = key;
  results[spot].ldo = user_ldo;
  if(spot==expected_items-1) items_left--; //Signal we've recorded last item
}

void ResultCollector::fn_compute_callback(kelpie::rc_t result, Key key, lunasa::DataObject user_ldo) {
  int spot = expected_items - (items_left--);
  results[spot].request_type = RequestType::Compute;
  results[spot].rc = result;
  results[spot].info = {};
  results[spot].key = key;
  results[spot].ldo = user_ldo;
  if(spot==expected_items-1) items_left--; //Signal we've recorded last item
}

void ResultCollector::Sync() {
  while(items_left>=0) {  std::this_thread::yield(); }
}



}  // namespace kelpie
