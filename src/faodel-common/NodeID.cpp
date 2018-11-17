// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <cinttypes>
#include <cstdlib>

#include <netdb.h>

#include <iostream>
#include <sstream>
#include <stdexcept>

#include "common/Bucket.hh"
#include "common/NodeID.hh"
#include "common/StringHelpers.hh"

using namespace std;

namespace faodel {

static NodeIDParseError parseError;


//Functions private to this file
namespace internal {

uint64_t makeNodeID(uint32_t ip, uint16_t port);
uint64_t makeNodeID(const string &host, const string &port);

} // namespace internal


uint64_t internal::makeNodeID(uint32_t ip, uint16_t port) {
  //Pack the items in
  uint64_t nodeid = (((uint64_t)port)<<32) | (ip);
  return nodeid;
}


/**
 * @brief Internal function for converting Node url parts into a 64b Kelpie NodeID
 * @param[out] nodeid The packed 64b value representing the Node url
 * @param[in] transport_id The index into the url prefix types
 * @param[in] host The hostname part of the url (if ip, it will translate to 32b form)
 * @param[in] port The port part of the url (a number). Use "0" if not applicable
 * @retval nodeid The encoded id value
 */
uint64_t internal::makeNodeID(const string &host, const string &port) {

  uint64_t nodeid = 0;

  //Convert port to a valid number
  uint32_t port_id = atoi(port.c_str());
  if(port_id >= 64*1024) throw NodeIDParseError("port out of range"); //port out of range

  //Convert host to a number. Do an ip lookup if given non-numeric
  uint32_t host_id;

  //Test ip address
  if(!IsValidIPString(host)) {
    throw NodeIDParseError("bad ip address given "+host);
  }

  //todo: change to something not deprecated for looking up IPs
  //note: hostent should not be freed as it is part of the lib.
  hostent *he = gethostbyname(host.c_str());
  if(he) {
    host_id = ntohl( *((int *)he->h_addr_list[0]) );
  } else {
    throw NodeIDParseError("gethostbyname failed "+host);
  }

  nodeid = internal::makeNodeID(host_id, port_id);

  return nodeid;

}

/**
 * @brief Create a node id based on a hex string of the form '0xABCD..'
 * @param[in] hex_string The hex-encoded string for the node (eg '0xABCD...')
 */
NodeID::NodeID(const string &hex_string) : nid(0) {

  if(hex_string.compare(0, 2, "0x")==0) {
    //Allow import from hex
    unsigned long val;
    stringstream ss;
    ss << std::hex << hex_string.substr(2); //0x not supported in parse
    ss >> val;
    //Note c++ has std::stoul("0xaaaaa", nullptr, 16)
    nid = static_cast<uint64_t>(val);
    return; //all ok!
  }

  //Didn't find network
  throw NodeIDParseError("nodeid hex string '"+hex_string+"' was not a hex value starting with 0x.");


}
NodeID::NodeID(const char *hex_string) {

  //Yuck. Sometimes we get here by mistake, when a user wants to
  //set an integer value, but the compiler thinks the 0 is a null
  //string. explicit won't catch this, and the error message is
  //hard to track down.
  if(hex_string==0) {
    throw std::invalid_argument("nodeid_t ctor was given a '0' for input. This is usually due to a mistake where the user "
                                "intended to set the nodeid to an integer value, but it is interpreted as a nullpointer.");
  }
  string s = string(hex_string);
  nodeid_t n2(s);
  nid = n2.nid;
}

/**
 * @brief Create a node id based on string identifiers for the host, and port
 * @param[in] hostname A string hostname/ip address
 * @param[in] port     The port to connect to
 */
NodeID::NodeID(const std::string &hostname, const std::string &port) : nid(0) {
   nid = internal::makeNodeID(hostname, port);
}

NodeID::NodeID(uint32_t ip, uint16_t port) {
  nid = internal::makeNodeID(ip,port);
}
/**
 * @brief Extract the node id's IP address into a dotted string notation
 * @return String with the dot-encoded ip address (eg "192.168.1.1")
 */
string NodeID::GetIP() const {
  stringstream ss;
  uint64_t octet;
  octet = nid>>24 & 0x0FF; ss << octet << ".";
  octet = nid>>16 & 0x0FF; ss << octet << ".";
  octet = nid>> 8 & 0x0FF; ss << octet << ".";
  octet = nid>> 0 & 0x0FF; ss << octet;
  return ss.str();
}

/**
 * @brief Extract a node's 16-bit port number as a string
 */
string NodeID::GetPort() const {
  int port = (nid>>32)&0x000FFFF;
  stringstream ss;
  ss<<port;
  return ss.str();
}

/**
 * @brief Get numerical values for the ip address and port
 *
 * @param[out] ip_address 32b binary value
 * @param[out] port       16b binary value
 */
void NodeID::GetIPPort(uint32_t *ip_address, uint16_t *port) const {
  if(ip_address) *ip_address = (nid & 0x0FFFFFFFF);
  if(port)       *port       = ((nid>>32) & 0x0FFFF);
}

/**
 * @brief Get string versions of the ip and port addresses
 *
 * @param[out] ip_string ip address String (eg 192.168.1.1)
 * @param[out] port_string port address
 */
void NodeID::GetIPPort(string *ip_string, string *port_string) const {
  if(ip_string)   *ip_string   = GetIP();
  if(port_string) *port_string = GetPort();
}

/**
 * @brief Generate the http address for accessing a nodeid's web page
 *
 * @param[in] extra_path Additional path info to place at the end of the link (eg "/myobject/reset")
 * @return string (eg "http://1.2.3.4:1990/myobject/reset")
 */
string NodeID::GetHttpLink(std::string extra_path) const {

  stringstream ss;
  ss<<"http://"<<GetIP()<<":"<<GetPort();
  if(extra_path!="") {
    if(extra_path[0]!='/') ss <<"/";
    ss<<extra_path;
  }
  return ss.str();
}


/**
 * @brief Convert this nodeid into an html-wrapperd link (usable in webpages)
 *
 * @param[in] extra_path Any additional path information that should go at the end of the url (eg /myobject/reset)
 * @param[in] link_text Text to display for the link (eg "RESET")
 * @return string (eg, "<a href=http://1.2.3.4:1990/myobject/reset>RESET</a>")
 */
string NodeID::GetHtmlLink(string extra_path, string link_text) const {
  stringstream ss;
  ss<< "<a href=\""<<GetHttpLink(extra_path)<<"\">";

  //Fill in the human link text
  ss<< ((link_text!="") ? link_text : GetHex());

  //Close out link
  ss<<"</a>\n";

  return ss.str();
}



/**
 * @brief Convert a node id to a hex string (useful when node is part of a bigger url)
 * @return hex-string This is a hex-encoded string for the node (eg 0xABCD...)
 */
string NodeID::GetHex() const {

  stringstream ss;
  ss<<"0x"<<std::hex<<nid;
  return ss.str();
}

} // namespace kelpie
