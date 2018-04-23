//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2014 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "common/NodeID.hh"
#include "common/Bootstrap.hh"

#include "webhook/Server.hh"
#include "webhook/server/boost/server.hpp"

#include <sys/types.h>
#include <ifaddrs.h>
#include <signal.h>
#include <utility>
#include <iostream>
#include <sstream>


using namespace std;

namespace http {
namespace server {

server::server()
  : LoggingInterface("webhook"),
    configured_(false),
    port_(0),
    num_starters_(0),
    io_service_(),
    signals_(io_service_),
    acceptor_(io_service_),
    connection_manager_(),
    socket_(io_service_),
    request_handler_() {

  do_await_stop();

  //faodel::bootstrap::RegisterComponent(this);

  // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).  
  //boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve({address, port});
  //acceptor_.open(endpoint.protocol());
  //acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
  //acceptor_.bind(endpoint);
  //acceptor_.listen();
  //do_accept();
}
server::~server(){
  stop();
}
void server::Init(const faodel::Configuration &config){

  int64_t port;
  string interfaces;
  string address;
  string interface_address("");

  //Extract config info for webhook
  ConfigureLogging(config); //Grab logging
  config.GetInt(&port,                   "webhook.port",       "1990");
  config.GetLowercaseString(&address,    "webhook.address",    "0.0.0.0");
  config.GetLowercaseString(&interfaces, "webhook.interfaces", "eth,lo");

  /*
   * How webhook finds an address to bind to:
   *  1) If the application asked for a specific address with webhook.address, then use it.
   *  2) Otherwise, query for all net interfaces and search for an "up" interface that
   *     matches one of the prefixes in webhook.interfaces
   *  3) If no match is found, use the webhook.address fallback address.
   */
  if (address == "0.0.0.0") {
      // search for an interface to use
      interface_address = search_interfaces(interfaces);
      if (interface_address != "") {
          address = interface_address;
      }
  }
  requested_address = address;
  requested_port = port;

  config.GetAllSettings(&config_entries); //Copy all of the config settings for display

  dbg("requested_address "+address);
  dbg("requested_port "+to_string(port));

  webhook::Server::updateHook("/config", [this] (const map<string,string> &args, stringstream &results) {
      return HandleWebhookConfig(args, results);
    });


  //Start the server. This is the only service that should fire up in Init,
  //because we need nodeid to be valid asap.
  info("Starting on requested "+requested_address+":"+to_string(requested_port));
  start(requested_address, requested_port);
  dbg("Webhook running at "+my_nodeid.GetHttpLink());

  faodel::bootstrap::SetNodeID(my_nodeid);
  
}



void server::HandleWebhookConfig(const map<string,string> &args, stringstream &results){
  webhook::ReplyStream rs(args, "Webhook Configuration Settings", &results);

  rs.tableBegin("Webhook Node Info",2);
  rs.tableTop({"Parameter", "Value"});
  rs.tableRow({"Webhook Link", my_nodeid.GetHtmlLink("",my_nodeid.GetHttpLink())});
  rs.tableRow({"NodeID",my_nodeid.GetHtmlLink()});
  rs.tableEnd();
  
  rs.mkTable(config_entries, "User-Supplied Configuration");
  rs.mkText("<b>Note:</b> These are the parameters provided to bootstrap. Some values "
            "(eg <b>webhook.port</b>) may have been adjusted due to conflicts\n");
  
  rs.mkSection("All Application Options");
  rs.mkText("Each component in this application has its own configuration settings."
            "The following is a list of all settings that were requested from Configuration:");


  //Build a table with all of the app parameters that config has seen someone request
  rs.tableBegin("");//"App Params",2);
  rs.tableTop({"Parameter", "Field Type", "Default Value"});
  map<string, array<string,2>> vals = faodel::configlog::GetConfigOptions();
  for(auto &name_vals : vals){
    vector<string> row_entries = {name_vals.first, name_vals.second[0], name_vals.second[1]};
    rs.tableRow(row_entries);
  }
  rs.tableEnd();
  rs.Finish();
}

void server::Start(){
  //Normally this would start the service, but it should already
  //have started in Init in order to guarantee GetNodeID works
}
void server::Finish(){
  stop();
}
void server::GetBootstrapDependencies(
                       std::string &name,
                       std::vector<std::string> &requires,
                       std::vector<std::string> &optional) const {
  name = "webhook";
  requires = {};
  optional = {};
}

int server::start(const std::string &address, unsigned int port){
  int rc=0;
  configured_mutex_.lock();

  num_starters_++;
  
  //See if we're already running on a port. If so, hand back port number
  if(configured_){
    rc=port_;
    configured_mutex_.unlock();
    return rc;
  }

  //Count upwards until we find a port we can use
  bool ok=true;
  do{
    stringstream ssport;
    ssport<<port;
    try{
      //cout <<"Trying port "<<ssport.str()<<endl;
      boost::asio::ip::tcp::resolver resolver(io_service_);
      boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve({address, ssport.str()});
      acceptor_.open(endpoint.protocol());
      acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
      acceptor_.bind(endpoint);
      acceptor_.listen();
      do_accept();
      ok=true;
    } catch(exception &e){
      //cout <<"Exception "<<e.what()<<" (port="<<ssport.str()<<")"<<endl;
      acceptor_.close();
      ok=false;
    }
    if(!ok) port++;
  } while(!ok);

  //cout<<"Opened on port "<<port<<endl;  

  configured_=true;
  port_ = port;
  configured_mutex_.unlock();

  th_http_server_ = std::thread(&http::server::server::run, this);

  my_nodeid = faodel::NodeID(this->address(), this->port());

  return port;  
}

std::string server::hostname(void){
    return acceptor_.local_endpoint().address().to_string();
}
unsigned long server::address(void){
    return acceptor_.local_endpoint().address().to_v4().to_ulong();
}
unsigned int server::port(void){
    return acceptor_.local_endpoint().port();
}


void server::run()
{
  // The io_service::run() call will block until all asynchronous operations
  // have finished. While the server is running, there is always at least one
  // asynchronous operation outstanding: the asynchronous accept call waiting
  // for new incoming connections.
  io_service_.run();
}

// adapted from the Linux getifaddrs(3) manpage
std::string server::search_interfaces(std::string interfaces) {
    struct ifaddrs *ifaddr, *ifa;
    int s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }
    vector<string> interface_list = faodel::Split(interfaces, ',', true);
    for ( string interface : interface_list ) {
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL) {
                continue;
            }

            // we only want IPv4 addresses
            if (ifa->ifa_addr->sa_family == AF_INET) {
                s = getnameinfo(ifa->ifa_addr,
                                sizeof(struct sockaddr_in),
                                host, NI_MAXHOST,
                                NULL, 0, NI_NUMERICHOST);
                if (s != 0) {
                    printf("getnameinfo() failed: %s\n", gai_strerror(s));
                    continue;
                }

                if (string(ifa->ifa_name).find(interface) == 0) {
                    freeifaddrs(ifaddr);
                    return string(host);
                }
            }
        }
    }
    freeifaddrs(ifaddr);
    return string("");
}

void server::do_accept()
{
  acceptor_.async_accept(socket_,
      [this](boost::system::error_code ec)
      {
        // Check whether the server was stopped by a signal before this
        // completion handler had a chance to run.
        if (!acceptor_.is_open()){
          return;
        }

        if (!ec) {
          connection_manager_.start(std::make_shared<connection>(
              std::move(socket_), connection_manager_, request_handler_));
        }

        do_accept();
      });
}

void server::do_await_stop()
{
  signals_.async_wait(
      [this](boost::system::error_code /*ec*/, int /*signo*/)
      {
        // The server is stopped by cancelling all outstanding asynchronous
        // operations. Once all operations have finished the io_service::run()
        // call will exit.
        acceptor_.close();
        connection_manager_.stop_all();
      });
}

int server::stop(){

  configured_mutex_.lock();
  if(num_starters_) {
    num_starters_--;

    if(num_starters_==0){
      do_await_stop(); //Kill all the connections
      io_service_.stop(); //Must manually stop    
      th_http_server_.join();
      configured_=false;
    }
  }
  configured_mutex_.unlock();
  return num_starters_;
}

bool server::IsRunning(){
  bool is_running;
  configured_mutex_.lock();
  is_running = configured_;
  configured_mutex_.unlock();
  return is_running;
}

} // namespace server
} // namespace http
