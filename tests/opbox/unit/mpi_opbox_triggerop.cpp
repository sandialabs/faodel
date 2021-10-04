// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <limits.h>
#include <string>
#include <atomic>
#include <vector>
#include <utility>
#include <algorithm>
#include <mpi.h>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "faodel-common/SerializationHelpersBoost.hh"

#include "whookie/Server.hh"
#include "lunasa/Lunasa.hh"
#include "opbox/OpBox.hh"

#include <stdio.h>


using namespace std;
using namespace faodel;
using namespace opbox;

#include "support/default_config_string.hh"

class OpBoxTriggerOpTest : public testing::Test {
protected:
  void SetUp() override {
    faodel::Configuration config(multitest_config_string);

    //Force this to an mpi implementation to make running easier
    config.Append("net.transport.name", "mpi");


    bootstrap::Start(config, opbox::bootstrap);
  }

  void TearDown() override {
    bootstrap::FinishSoft();
  }

  internal_use_only_t iuo; //Shortcut to getting at node_t ctor
  bool ok;
  int rc;

};

//This is a special action for poking and getting back a value
class OpArgsPoke : public OpArgs {
public:
  OpArgsPoke(int modifier) : OpArgs(UpdateType::user_trigger), modifier(modifier), terminateOp(false) {}

  ~OpArgsPoke() override {}

  int modifier;   //What you want to add to sum
  int value;      //What current sum is

  std::atomic<bool> done;
  std::atomic<bool> terminateOp;
};


class OpTrigger1 : public opbox::Op {
public:

  OpTrigger1(int value) : Op(true), state(0), value(value) {}

  OpTrigger1(op_create_as_target_t t) : Op(t) {}

  //Unique name and id for this op
  const static unsigned int op_id = const_hash("OpTrigger1");
  static constexpr const char *op_name = "OpTrigger1"; //Work around for strings i

  unsigned int getOpID() const override { return op_id; }

  std::string getOpName() const { return op_name; }

  WaitingType UpdateOrigin(OpArgs *args);    //Guts below
  WaitingType UpdateTarget(OpArgs *args) override { return WaitingType::error; } //do nothing

  std::string GetStateName() const override { return "not implemented"; }

private:
  int state;
  mailbox_t my_mailbox;
  int value;
};

WaitingType OpTrigger1::UpdateOrigin(OpArgs *args) {

  WaitingType rc;

  switch(state) {
    case 0: {
      args->VerifyTypeOrDie(UpdateType::start, op_name);

//    cout <<"Received init. Moving to state 1.  Returning wait_on_user.\n";
      state = 1;
      rc = WaitingType::wait_on_user;
      break;
    }
    case 1: //Handle user input
    {
      args->VerifyTypeOrDie(UpdateType::user_trigger, op_name);
      auto pargs = reinterpret_cast<OpArgsPoke *>(args);

      value += pargs->modifier;
      pargs->value = value; //Pass back the result
      if(value<0) {
//        cout <<"State is 1.  Value is "<<value<<".  Moving to state 2.\n";
        state = 2;
      }
      pargs->done = true;
      rc = WaitingType::wait_on_user;
      break;
    }
    case 2: {
      args->VerifyTypeOrDie(UpdateType::user_trigger, op_name);
      auto pargs = reinterpret_cast<OpArgsPoke *>(args);

      if(pargs->terminateOp) {
//        cout <<"State is 2.  Value is "<<value<<".  Returning done_and_destroy.\n";
        rc = WaitingType::done_and_destroy;
      } else {
//        cout <<"State is 2.  Value is "<<value<<".  Returning wait_on_user.\n";
        rc = WaitingType::wait_on_user;
      }
      pargs->done = true;
      break;
    }
    default: {
      args->VerifyTypeOrDie(UpdateType::user_trigger, op_name);
      auto pargs = reinterpret_cast<OpArgsPoke *>(args);

      cerr << "Unknown state for OpTrigger1: " << state << endl;
      pargs->done = true;
      rc = WaitingType::error;
      break;
    }
  }

  return rc;
}


#define TRIGGER_OP_SYNC(_mailbox, _args, _rc)   \
    {                                         \
    _args->done = false;                      \
    _rc = opbox::TriggerOp(_mailbox, _args);  \
    EXPECT_EQ(_rc,0);                         \
    while(_args->done == false) {};           \
    };


TEST_F(OpBoxTriggerOpTest, SimplePoke) {

  mailbox_t mb;
  int64_t expected_val = 5;

  opbox::LaunchOp(new OpTrigger1(expected_val), &mb);

  auto args_get = make_shared<OpArgsPoke>(0);
  auto args_decr = make_shared<OpArgsPoke>(-1);

  //Verify it started at right value
  TRIGGER_OP_SYNC(mb, args_get, rc);
  EXPECT_EQ(expected_val, args_get->value);

  //Keep poking it with decrements until we run out of values
  for(int i = expected_val - 1; i>=0; i--) {
    TRIGGER_OP_SYNC(mb, args_decr, rc);
    EXPECT_EQ(i, args_decr->value);
  }

  //Should be done now. Poke again to make sure we're done
  TRIGGER_OP_SYNC(mb, args_get, rc);
  EXPECT_EQ(0, args_get->value);

  TRIGGER_OP_SYNC(mb, args_decr, rc);
  EXPECT_EQ(-1, args_decr->value);

  TRIGGER_OP_SYNC(mb, args_get, rc);

  args_get->terminateOp = true;
  TRIGGER_OP_SYNC(mb, args_get, rc);
}


TEST_F(OpBoxTriggerOpTest, MultipleOps) {

  //Create 10 ops and set their values to 100
  vector<mailbox_t> mbs;
  vector<int> expected_vals;
  for(int i = 0; i<10; i++) {
    mailbox_t mb;
    opbox::LaunchOp(new OpTrigger1(100), &mb);
    mbs.push_back(mb);
    expected_vals.push_back(100);
  }

  //Use the same commands for get/decr
  auto args_get = make_shared<OpArgsPoke>(0);
  auto args_decr = make_shared<OpArgsPoke>(-1);

  //Pick random ops and decrement them
  for(int i = 0; i<90; i++) {
    int spot = rand()%10;
    TRIGGER_OP_SYNC(mbs[spot], args_decr, rc);
    expected_vals[spot]--;
    EXPECT_EQ(expected_vals[spot], args_decr->value);
  }

  //Check all mailboxes
  for(int i = 0; i<10; i++) {
    TRIGGER_OP_SYNC(mbs[i], args_get, rc);
    EXPECT_EQ(expected_vals[i], args_get->value);
  }

  //Close all mailboxes
  for(int i = 0; i<10; i++) {
    args_decr->modifier = -(1 + expected_vals[i]);
    TRIGGER_OP_SYNC(mbs[i], args_decr, rc);
  }

  for(int i = 0; i<10; i++) {
    TRIGGER_OP_SYNC(mbs[i], args_get, rc);
  }

  for(int i = 0; i<10; i++) {
    args_get->terminateOp = true;
    TRIGGER_OP_SYNC(mbs[i], args_get, rc);
  }

  for(int i = 0; i<10; i++) {
    rc = opbox::TriggerOp(mbs[i], args_get);
    EXPECT_EQ(-1, rc);
  }
}


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

  int mpi_rank, mpi_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);


  //Register once
  opbox::RegisterOp<OpTrigger1>();


  int rc = RUN_ALL_TESTS();

  MPI_Barrier(MPI_COMM_WORLD);

  MPI_Finalize();
  return rc;
}
