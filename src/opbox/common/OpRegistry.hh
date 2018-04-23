// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef OPBOX_OPREGISTRY_HH
#define OPBOX_OPREGISTRY_HH

#include "common/Common.hh"

#include "webhook/WebHook.hh"
#include "webhook/Server.hh"

#include "opbox/ops/Op.hh"
#include "opbox/common/Types.hh"

namespace opbox {

/**
 * @brief Registry for storing information about Ops available to the node
 *
 * This registry stores a list of Op classes that the system is configured
 * to handle, as well as constructor functions for each op. When an
 * application registers a new Op type, the information is stored here.
 *
 * The OpRegistry keeps separate lists for ops depending on when they are
 * registered with the system. If the ops are registered before start time,
 * they are placed in a pre-start area that does not incur lock overheads
 * at runtime. If the ops are registered after start, they are placed in
 * a post-start area that incurs a lock operation on use.
 */

class OpRegistry :
    public faodel::InfoInterface {

public:
  OpRegistry();
  ~OpRegistry() override { if(mutex) delete mutex; }

  void RegisterOp(int op_id, std::string op_name, fn_OpCreate_t func);
  void DeregisterOp(int op_id, bool ignore_lock_warning=false);


  void Start() { finalized=true; }
  void Finish();

  Op * CreateOp(unsigned int op_id);

  void HandleWebhookStatus(const std::map<std::string,std::string> &args, std::stringstream &results);
  void webhookInfo(webhook::ReplyStream &rs);

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const;

private:

  faodel::MutexWrapper *mutex;

  bool finalized;
  std::map<unsigned int, fn_OpCreate_t> known_ops_pre;
  std::map<unsigned int, fn_OpCreate_t> known_ops_post;

  std::map<unsigned int, std::string> op_names_pre;
  std::map<unsigned int, std::string> op_names_post;

};

} // namespace opbox

#endif // OPBOX_OPREGISTRY_HH
