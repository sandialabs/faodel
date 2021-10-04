//
// server.hpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2014 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef WHOOKIE_SERVER_HPP
#define WHOOKIE_SERVER_HPP

#include <boost/asio.hpp>
#include <string>
#include <thread>
#include <mutex>

#include "whookie/server/boost/connection.hpp"
#include "whookie/server/boost/connection_manager.hpp"
#include "whookie/server/boost/request_handler.hpp"

#include "faodel-common/Common.hh"
#include "faodel-common/LoggingInterface.hh"

#include "whookie/Server.hh"



namespace http {
namespace server {

class asio_resources {
public:
    asio_resources()
    : io_service_(),
      signals_(io_service_),
      acceptor_(io_service_),
      connection_manager_(),
      socket_(io_service_),
      request_handler_()
    {
    }
public:
    /// The io_service used to perform asynchronous operations.
    boost::asio::io_service io_service_;

    /// The signal_set is used to register for process termination notifications.
    boost::asio::signal_set signals_;

    /// Acceptor used to listen for incoming connections.
    boost::asio::ip::tcp::acceptor acceptor_;

    /// The connection manager which owns all live connections.
    connection_manager connection_manager_;

    /// The next socket to be accepted.
    boost::asio::ip::tcp::socket socket_;

    /// The handler for all incoming requests.
    request_handler request_handler_;
};

/// The top-level class of the HTTP server.
class server :
    public faodel::bootstrap::BootstrapInterface,
    public faodel::LoggingInterface {

public:
  server(const server&) = delete;
  server& operator=(const server&) = delete;

  ~server() override;

  //Bootstrap standard apis
  //void Init(const faodel::Configuration &config) override;
  void Init(const faodel::Configuration &config) override {}
  void InitAndModifyConfiguration(faodel::Configuration *config) override;
  void Start() override;
  void Finish() override;
  void GetBootstrapDependencies(std::string &name,
                       std::vector<std::string> &requires,
                       std::vector<std::string> &optional) const override;


  /// Construct the server to listen on the specified TCP address and port, and
  /// serve up files from the given directory.
  explicit server();

  /// Run the server's io_service loop. Does not return
  void run();
  
  int start(const std::string &address, unsigned int port);
  int start(unsigned int port);
  int stop();

  std::string   hostname(void);
  unsigned long address(void);
  unsigned int  port(void);

  bool IsRunning();
  faodel::nodeid_t GetNodeID() { return my_nodeid; }

  int updateAppName(std::string app_name_) {
    app_name=app_name_;
    return asio_->request_handler_.updateAppName(app_name);
  }

  int registerHook(const std::string &name, whookie::cb_web_handler_t func) {
    return asio_->request_handler_.registerHook(name, func);
  }
  int updateHook(const std::string &name, whookie::cb_web_handler_t func) {
    return asio_->request_handler_.updateHook(name, func);
  }
  int deregisterHook(const std::string &name) {
    return asio_->request_handler_.deregisterHook(name);
  }
  
private:
  faodel::nodeid_t my_nodeid = faodel::NODE_UNSPECIFIED;
  
  bool configured_;
  faodel::Configuration *bootstrap_config_ptr;

  unsigned int port_;
  std::mutex configured_mutex_;
  int num_starters_;

  std::string app_name;
  std::string requested_address;
  unsigned int requested_port;
  
  //Provide the configuration whookie was given
  void HandleWhookieConfig(const std::map<std::string,std::string> &args, std::stringstream &results);
  void HandleWhookieBootstrap(const std::map<std::string,std::string> &args, std::stringstream &results);

  /// interate over a list of network interfaces looking for an address to use
  std::string search_interfaces(std::string interfaces);

  /// Perform an asynchronous accept operation.
  void do_accept();

  /// Wait for a request to stop the server.
  void do_await_stop();

  asio_resources *asio_;

  std::thread th_http_server_;

};

} // namespace server
} // namespace http

#endif // WHOOKIE_SERVER_HPP
