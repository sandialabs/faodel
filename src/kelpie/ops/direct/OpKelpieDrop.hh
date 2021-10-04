// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_OPKELPIEDROP_HH
#define FAODEL_OPKELPIEDROP_HH

#include <atomic>

#include "opbox/OpBox.hh"
#include "opbox/ops/OpHelpers.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

#include "kelpie/Kelpie.hh"
#include "kelpie/localkv/LocalKV.hh"

#include "kelpie/ops/direct/msg_direct.hh"

namespace kelpie {

/**
 * @brief An ObBox state machine for dropping objects from remote nodes
 */
class OpKelpieDrop : public opbox::Op {

  //States
  enum class State : int {
    orig_drop_send = 0,
    trgt_drop_start,
    orig_drop_wait_for_results,
    done
  };

public:

  OpKelpieDrop(
          std::vector<std::pair<faodel::nodeid_t, net::peer_ptr_t>> targets,
          const faodel::bucket_t bucket,
          const Key &search_key,
          bool already_dropped_locally,
          fn_drop_callback_t callback);

  explicit OpKelpieDrop(Op::op_create_as_target_t t);

  ~OpKelpieDrop() override;

  //Unique name and id for this op
  const static unsigned int op_id;
  const static std::string op_name;
  static bool debug_enabled; //!< Dump debug messages

  unsigned int getOpID() const override { return op_id; }
  std::string getOpName() const override { return op_name; }

  WaitingType Update(OpArgs *args) override; //Combined use
  WaitingType UpdateOrigin(OpArgs *args) override { return WaitingType::error; }  //Remove
  WaitingType UpdateTarget(OpArgs *args) override { return WaitingType::error; }  //Remove

  std::string GetStateName() const override;

  static void configure(faodel::internal_use_only_t iuo, const faodel::Configuration *config, LocalKV *new_lkv);

private:

  //todo: put this in a standard form so it can be reused
  #if Faodel_LOGGINGINTERFACE_DISABLED==0
  void dbg(const std::string &s) const {
    if(OpKelpieDrop::debug_enabled) {
      std::cout << "\033[1;93mD " << op_name << ": ["<<GetStateName()<<"]:\033[0m\t" << (s) << std::endl;
    }
  }
  #else
  void dbg(std::string s) const {}
  #endif

  static LocalKV *lkv;  //Pointer back to the lkv, set at start time

  std::vector<std::pair<faodel::nodeid_t, net::peer_ptr_t>> targets;
  faodel::bucket_t bucket;
  kelpie::Key search_key;
  fn_drop_callback_t callback;

  std::atomic<size_t> num_targets_left;
  std::atomic<int> successful_drops;

  State state;

  //Origin/Target States (in order)
  WaitingType smo_Drop_Send();
  WaitingType smt_Drop_Start(opbox::OpArgs *args);
  WaitingType smo_Drop_WaitForResults(opbox::OpArgs *args);


};

} // namespace kelpie

#endif //FAODEL_OPKELPIEDROP_HH
