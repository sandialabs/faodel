// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_ACTION_HH
#define FAODEL_ACTION_HH

#include <iostream>
#include <string>

#include "faodel-common/FaodelTypes.hh"
#include "faodel-common/StringHelpers.hh"

class ActionInterface {

public:

  ActionInterface() {}
  virtual ~ActionInterface() {}

  std::string cmd;
  std::string rank;
  std::string error_message;

  bool Valid() { return !cmd.empty(); }
  bool HasError() { return !error_message.empty(); }

  void exitOnError() { if(HasError()) { std::cerr<<error_message<<std::endl; exit(-1);}}
  void exitOnExtraArgs() {
    if(remaining_args.size()) {
      std::cerr<<"Command has extra arguments: "<<faodel::Join(remaining_args,' ')<<std::endl;
      exit(-1);
    }
  }

  bool RunsOnRank(int test_rank) {
    return ((rank.empty() || (rank==std::to_string(test_rank))));
  }

protected:
  std::vector<std::string> remaining_args;
  faodel:: rc_t setError(const std::string &err) { error_message=err; return EINVAL; }

};


#endif //FAODEL_ACTION_HH
