// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef OPBOX_OPHELPERS_HH
#define OPBOX_OPHELPERS_HH

#include "opbox/common/Types.hh"
#include "opbox/common/Message.hh"
#include "opbox/OpBox.hh"

namespace opbox {

/**
 * @brief Designate that network should only notify Op about errors
 */
class ErrorOnlyCallback {
private:
  Op *op;
public:
  explicit ErrorOnlyCallback(Op *op) : op(op) {}
  WaitingType operator()(OpArgs *args) {
    if ((args->type >= UpdateType::send_error) && (args->type <= UpdateType::atomic_error)) {
      opbox::internal::UpdateOp(op, args);
    }
    return WaitingType::done_and_destroy;
  }
};

/**
 * @brief Designate that network should only notify Op about successful events
 */
class SuccessOnlyCallback {
private:
  Op *op;
public:
  explicit SuccessOnlyCallback(Op *op) : op(op) {}
  WaitingType operator()(OpArgs *args) {
    if (args->type < UpdateType::timeout) {
      opbox::internal::UpdateOp(op, args);
    }
    return WaitingType::done_and_destroy;
  }
};

/**
 * @brief Designate that network should only notify Op about unsuccessful events
 */
class UnsuccessfulOnlyCallback {
private:
  Op *op;
public:
  explicit UnsuccessfulOnlyCallback(Op *op) : op(op) {}
  WaitingType operator()(OpArgs *args){
    if (args->type >= UpdateType::timeout) {
      opbox::internal::UpdateOp(op, args);
    }
    return WaitingType::done_and_destroy;
  }
};


/**
 * @brief Designate that network should only notify Op about timeout events
 */
class TimeoutOnlyCallback {
private:
  Op *op;
public:
  explicit TimeoutOnlyCallback(Op *op) : op(op) {}
  WaitingType operator()(OpArgs *args) {
    if (args->type == UpdateType::timeout) {
      opbox::internal::UpdateOp(op, args);
    }
    return WaitingType::done_and_destroy;
  }
};

/**
 * @brief Designate that network should notify Op about all events
 */
class AllEventsCallback {
private:
  Op *op;
public:
  explicit AllEventsCallback(Op *op) : op(op) {}
  WaitingType operator()(OpArgs *args) {
    opbox::internal::UpdateOp(op, args);
    return WaitingType::done_and_destroy;
  }
};

}

#endif // OPBOX_OPHELPERS_HH
