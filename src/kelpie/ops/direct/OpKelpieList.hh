// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef KELPIE_OPKELPIELIST_HH
#define KELPIE_OPKELPIELIST_HH

#include <condition_variable>

#include "opbox/OpBox.hh"
#include "opbox/ops/OpHelpers.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

#include "kelpie/Kelpie.hh"
#include "kelpie/localkv/LocalKV.hh"

#include "kelpie/ops/direct/msg_direct.hh"

namespace kelpie {

/**
 * @brief An OpBox state machine for getting info for objects matching a key
 */
class OpKelpieList : public opbox::Op {

  //States
  enum class State : int {
    orig_list_send = 0,
    trgt_list_start,
    orig_list_wait_for_results,
    done
  };

public:

  OpKelpieList(
          const std::vector<std::pair<faodel::nodeid_t, net::peer_ptr_t>> targets,
          const faodel::bucket_t bucket,
          const Key &search_key,
          ObjectCapacities *object_capacities,
          std::condition_variable *cv,
          int *num_targets_left);

  //A target starts off the same way no matter what command
  OpKelpieList(Op::op_create_as_target_t t);

  ~OpKelpieList() override;

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
  void dbg(std::string s) const {
      if(OpKelpieList::debug_enabled) {
        std::cout << "\033[1;31mD " << op_name << ":\033[0m " << (s) << std::endl;
      }
  }
  #else
  void dbg(std::string s) const {}
  #endif



  static LocalKV *lkv;  //Pointer back to the lkv, set at start time

  //These get used in the sender start state
  std::vector<std::pair<faodel::nodeid_t, net::peer_ptr_t>> targets;
  faodel::bucket_t bucket;
  Key search_key;

  ObjectCapacities *user_object_capacities;
  std::condition_variable *user_cv; //FIXME
  int *user_num_targets_left;


  State state;


  //Origin/Target States (in order)
  WaitingType smo_List_Send();
  WaitingType smt_List_Start(opbox::OpArgs *args);
  WaitingType smo_List_WaitForResults(opbox::OpArgs *args);

  WaitingType updateState(State new_state, WaitingType waiting_conditions) {
    state=new_state;
    return waiting_conditions;
  }
  WaitingType updateStateDone(){
    state=State::done;
    return WaitingType::done_and_destroy;
  }

};

} // namespace kelpie

#endif //KELPIE_OPKELPIELIST_HH
