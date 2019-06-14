//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2014 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "faodel-common/NodeID.hh"
#include "faodel-common/Bootstrap.hh"

#include "whookie/Server.hh"
#include "whookie/server/boost/server.hpp"

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
  : LoggingInterface("whookie"),
    configured_(false),
    port_(0),
    num_starters_(0) {

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

  //Extract config info for whookie
  ConfigureLogging(config); //Grab logging
  config.GetString(&app_name,            "whookie.app_name",   "Whookie Application");
  config.GetInt(&port,                   "whookie.port",       "1990");
  config.GetLowercaseString(&address,    "whookie.address",    "0.0.0.0");
  config.GetLowercaseString(&interfaces, "whookie.interfaces", "eth,lo");

  /*
   * How whookie finds an address to bind to:
   *  1) If the application asked for a specific address with whookie.address, then use it.
   *  2) Otherwise, query for all net interfaces and search for an "up" interface that
   *     matches one of the prefixes in whookie.interfaces
   *  3) If no match is found, use the whookie.address fallback address.
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

  asio_ = new asio_resources();
  do_await_stop();

  whookie::Server::updateHook("/config", [this] (const map<string,string> &args, stringstream &results) {
      return HandleWhookieConfig(args, results);
    });

  whookie::Server::updateHook("/bootstraps", [this] (const map<string,string> &args, stringstream &results) {
      return HandleWhookieBootstrap(args, results);
  });

  whookie::Server::updateAppName(app_name);

  //Start the server. This is the only service that should fire up in Init,
  //because we need nodeid to be valid asap.
  dbg("Requesting "+requested_address+":"+to_string(requested_port));
  start(requested_address, requested_port);
  info("Running at "+my_nodeid.GetHttpLink());

  faodel::bootstrap::setNodeID(faodel::internal_use_only, my_nodeid);
  
}



void server::HandleWhookieConfig(const map<string,string> &args, stringstream &results){
  faodel::ReplyStream rs(args, "Whookie Configuration Settings", &results);

  rs.tableBegin("Whookie Node Info",2);
  rs.tableTop({"Parameter", "Value"});
  //rs.tableRow({"Whookie Link", my_nodeid.GetHtmlLink("",my_nodeid.GetHttpLink())});
  rs.tableRow({"Whookie Link", rs.createLink(  my_nodeid.GetHttpLink(), my_nodeid.GetHttpLink(), false)  });
  rs.tableRow({"NodeID", rs.createLink( my_nodeid.GetHex(), my_nodeid.GetHttpLink(), false) }); // my_nodeid.GetHtmlLink()});
  rs.tableEnd();
  
  rs.mkTable(config_entries, "User-Supplied Configuration");
  rs.mkText(rs.createBold("Note:")+"These are the parameters provided to bootstrap. Some values "
            "(eg whookie.port) may have been adjusted due to conflicts\n");
  
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

void server::HandleWhookieBootstrap(const map<string,string> &args, stringstream &results) {
  faodel::ReplyStream rs(args, "Bootstrap", &results);
  faodel::bootstrap::dumpInfo(rs);

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
  name = "whookie";
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
    //cout <<"Already running on port "<<port_<<endl;
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
      boost::asio::ip::tcp::resolver resolver(asio_->io_service_);
      boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve({address, ssport.str()});
      asio_->acceptor_.open(endpoint.protocol());
      asio_->acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
      asio_->acceptor_.bind(endpoint);
      asio_->acceptor_.listen();
      do_accept();
      ok=true;
    } catch(exception &e){
      //cout <<"Exception "<<e.what()<<" (port="<<ssport.str()<<")"<<endl;
      asio_->acceptor_.close();
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
    return asio_->acceptor_.local_endpoint().address().to_string();
}
unsigned long server::address(void){
    return asio_->acceptor_.local_endpoint().address().to_v4().to_ulong();
}
unsigned int server::port(void){
    return asio_->acceptor_.local_endpoint().port();
}


void server::run()
{
  // The io_service::run() call will block until all asynchronous operations
  // have finished. While the server is running, there is always at least one
  // asynchronous operation outstanding: the asynchronous accept call waiting
  // for new incoming connections.
  asio_->io_service_.run();
  //cout << "asio_->io_service_.run() has exited" << endl;
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
  asio_->acceptor_.async_accept(asio_->socket_,
      [this](boost::system::error_code ec)
      {
        // Check whether the server was stopped by a signal before this
        // completion handler had a chance to run.
        if (!asio_->acceptor_.is_open()){
          return;
        }

        if (!ec) {
          asio_->connection_manager_.start(std::make_shared<connection>(
              std::move(asio_->socket_), asio_->connection_manager_, asio_->request_handler_));
        }

        do_accept();
      });
}

void server::do_await_stop()
{
  asio_->signals_.async_wait(
      [this](boost::system::error_code /*ec*/, int /*signo*/)
      {
        // The server is stopped by canceling all outstanding asynchronous
        // operations. Once all operations have finished the io_service::run()
        // call will exit.
        asio_->acceptor_.close();
        asio_->connection_manager_.stop_all();
      });
}

int server::stop(){

  configured_mutex_.lock();
  if(num_starters_) {
    num_starters_--;

    if(num_starters_==0){
      //cout << "Stopping whookie on port " << port_ << endl;
      do_await_stop(); //Kill all the connections
      asio_->io_service_.stop(); //Must manually stop
      //cout << "Joining whookie thread " << th_http_server_.get_id() << endl;
      th_http_server_.join();
      delete asio_;
      configured_=false;
    }
  }
  //cout << "num_starters_ is " << num_starters_ << endl;
  configured_mutex_.unlock();
  return num_starters_;
}

bool server::IsRunning(){
  bool is_running;
  configured_mutex_.lock();
  is_running = configured_;
  //cout << "whookie running? " << configured_ << endl;
  configured_mutex_.unlock();
  return is_running;
}

} // namespace server
} // namespace http
