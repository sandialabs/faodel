// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef OPBOX_OPCOUNT_HH
#define OPBOX_OPCOUNT_HH

#include <future>

#include "opbox/ops/Op.hh"

namespace opbox {

/**
 * @brief This Op is a simple example of a no-net Op that uses futures
 *
 * OpCount is a simple Op that simply counts down the number of times
 * it has been called.
 *
 * @deprecated This op is deprecated due to API changes and is included for
 *        build compatibilities.
 */
class OpCount : public Op {

  enum class State : int {
      start,
      done};


public:
  explicit OpCount(int num_left);
  ~OpCount() override = default;

  const static unsigned int op_id;
  const static std::string  op_name;
  unsigned int getOpID() const override { return op_id; }
  std::string  getOpName() const override { return op_name; }
  std::string  GetStateName() const override;

  WaitingType Wait();

  WaitingType UpdateOrigin(OpArgs *args) override;
  WaitingType UpdateTarget(OpArgs *args) override { return WaitingType::done_and_destroy; }

private:
  int num_left;
  State state;

  std::promise<int> count_promise;
  std::future<int>  count_future;

};

}




#endif // OPBOX_OPCOUNT_HH
