// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_COMMON_NODEID_HH
#define FAODEL_COMMON_NODEID_HH

#include <string>
#include <sstream>

#include <exception>
#include <stdexcept>
#include <utility>

#include "faodel-common/FaodelTypes.hh"

namespace faodel {

/**
 * @brief POD struct for holding a unique identifier for a particular faodel node (ie, a rank)
 *
 * FAODEL components often need a simple way to reference different
 * ranks that are running in the system. The nodeid_t provides a simple and
 * concise (64b) value for referencing other ranks. Internally, it is just
 * the IP address and port number a particular rank uses for its Whookie
 * instance. Users are expected to pass the nodeid_t around by itself without
 * looking into its internal value.
 *
 * Since a 64b number can be cumbersome to pass around, the struct has
 * mechanisms for converting the value to/from a human-readable hex string.
 *
 * It is expected that users obtain a nodeid by querying webhhok.
 */
struct NodeID {
  uint64_t nid;

  NodeID() : nid(0) {} //Empty when given nothing
  explicit NodeID(const std::string &hex_string);

  explicit NodeID(const char *hex_string);
  NodeID(const std::string &hostname, const std::string &port);
  NodeID(const uint32_t ip_address, uint16_t port);

  NodeID(uint64_t n, internal_use_only_t iuo) {nid=n;} //<! Reserved for internal use (eg testing)

  bool operator== (const NodeID &other) const { return (nid == other.nid); }
  bool operator!= (const NodeID &other) const { return (nid != other.nid); }
  bool operator<  (const NodeID &other) const { return (nid < other.nid); }

  bool        Unspecified() const { return (nid==0); }      //<! True if this nodeid's value is NODE_UNSPECIFIED
  bool        Valid() const { return (nid!=0); }            //<! True if this nodeid's value is not NODE_UNSPECIFIED
  bool        ValidIP() const;
  bool        ValidPort() const;

  std::string GetIP() const;
  std::string GetPort() const;
  void        GetIPPort(std::string *ip_string, std::string *port_string) const;
  void        GetIPPort(uint32_t *ip_address, uint16_t *port) const;
  std::string GetHttpLink(std::string extra_path="") const;
  std::string GetHtmlLink(std::string extra_path="", std::string link_text="") const;
  std::string GetHex() const;


  /** @brief A template for serializing nodeid when using boost (or cereal) serialization*/
  template <typename Archive>
  void serialize(Archive &ar, const unsigned int version) {
    ar & nid;
  }

};

using nodeid_t = NodeID;


const nodeid_t NODE_LOCALHOST(0x01, internal_use_only);   //!< Specifies local host (usually to defer a node id lookup)
const nodeid_t NODE_UNSPECIFIED(0x00, internal_use_only); //!< For designating that this field has not been set



/**
 * @brief An exception for capturing error info when parsing a nodeid string
 */
class NodeIDParseError : public std::runtime_error {
public:
  NodeIDParseError()
    : std::runtime_error( "Format problem while parsing NodeID string" ) {}
  explicit NodeIDParseError( const std::string& s )
    : std::runtime_error( "Format problem while parsing NodeID string: " + s ) {}
};





/**
 * @brief Simple structure for holding a string name and a nodeid_t
 *
 * Often we need to associate a label with particular node. This structure
 * just provides a dedicated pair for the two and includes a template for
 * boost serialization.
 */
struct NameAndNode {
  NameAndNode() : name(), node(NODE_UNSPECIFIED) {}
  NameAndNode(std::string name, nodeid_t node) : name(std::move(name)), node(node) {}
  std::string name; //!< A simple string label for a node (usually human readable)
  nodeid_t node;    //!< An id for the node in binary form that components can use for making connections
  bool operator<(  const NameAndNode &x) const { return name < x.name; }
  bool operator==( const NameAndNode &x) const { return name == x.name; }

  /** @brief A template for serializing nodeid when using boost (or cereal) serialization*/
  template <typename Archive>
  void serialize(Archive &ar, const unsigned int version) {
    ar & node;
    ar & name;
  }
};



} // namespace faodel

#endif // FAODEL_COMMON_NODEID_HH
