// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include "whookie/Server.hh"
#include "whookie/server/boost/server.hpp"

using namespace std;

//Alias the boost server class to be whookie's internal server


//Initialize the static server implementation
//static whookie::internal::server whookie::Server::server_impl();

namespace whookie {

//The boost implementation has everything built into its server,
//so just plug it in.
class ServerImpl {
public:
  http::server::server boost_server;
};


//Initialize the static server implementation
ServerImpl whookie::Server::server_impl;


/**
 * @brief Bootstrap function used to manually register whookie (and
 *        dependencies) with bootstrap
 *
 * @retval "whookie"
 *
 * @note Users pass this to bootstrap's Start/Init. Only the last 
 *       bootstap dependenciy needs to be supplied.
 */
std::string bootstrap() {

  //No dependencies

  //TODO: server_impl was private, couldn't get at it from here
  faodel::bootstrap::RegisterComponent(&Server::server_impl.boost_server, true);
  return "whookie";
}

int Server::updateAppName(std::string app_name) {
  return server_impl.boost_server.updateAppName(app_name);
}

int Server::registerHook(string name, cb_web_handler_t func) {
  return server_impl.boost_server.registerHook(name, func);
}
int Server::updateHook(string name, cb_web_handler_t func) {
  return server_impl.boost_server.updateHook(name, func);
}
int Server::deregisterHook(string name) {
  return server_impl.boost_server.deregisterHook(name);
}

bool Server::IsRunning(){
  return server_impl.boost_server.IsRunning();
}

faodel::nodeid_t Server::GetNodeID(){
  return server_impl.boost_server.GetNodeID();
}

} // namespace whookie

