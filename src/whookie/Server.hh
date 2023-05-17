// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef WHOOKIE_SERVER_HH
#define WHOOKIE_SERVER_HH

#include <string>
#include <map>
#include <functional>

#include "faodel-common/Common.hh"


namespace whookie {

std::string bootstrap(); //Function handed to bootstrap for dependency injection


//Lambda callback: given a k/v list of args, pass results back through a stringstream
typedef std::function<void (const std::map<std::string, std::string>&, std::stringstream&)> cb_web_handler_t;


class ServerImpl; //Forward reference for server implementation


/**
 * @brief A Whookie server that maintains hooks.
 */
class Server {

public:
  Server();
  ~Server();

  static int updateAppName(std::string app_name);
  static int registerHook(std::string name, cb_web_handler_t func);
  static int updateHook(std::string name, cb_web_handler_t func);
  static int deregisterHook(std::string name);

  static bool IsRunning();
  static faodel::nodeid_t GetNodeID();
  
//private:
  static ServerImpl server_impl; 

};





} // namespace whookie

#endif // WHOOKIE_SERVER_HH
