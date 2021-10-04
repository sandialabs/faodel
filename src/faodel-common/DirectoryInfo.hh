// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef OPBOX_DIRECTORYINFO_HH
#define OPBOX_DIRECTORYINFO_HH



#include <string>
#include <vector>

#include "faodel-common/Common.hh"
#include "faodel-common/ReplyStream.hh"

namespace faodel {

/**
 * @brief A class for storing directory information for an entry in DirMan
 */
class DirectoryInfo
        : public faodel::InfoInterface {

public:

  faodel::ResourceURL              url;          //!< URL for entry. Should include refnode and bucket
  std::string                      info;         //!< Human text about what this is
  uint32_t                         min_members;  //!< Minimum number of members for this to be viable
  std::vector<faodel::NameAndNode> members;      //!< A list of all the nodes that are part of resource (name and nodeid)


  DirectoryInfo() : url(), info(), min_members(0), members()  {}

  explicit DirectoryInfo(faodel::ResourceURL new_url);
  explicit DirectoryInfo(std::string s_url)
          : DirectoryInfo(faodel::ResourceURL(s_url)) {
  }

  DirectoryInfo(std::string s_url, std::string s_info)
    : url(faodel::ResourceURL(s_url)), info(s_info), min_members(0), members() {
    url.RemoveOption("info");
  }

  ~DirectoryInfo() override = default;

  bool operator<(  const DirectoryInfo &x) const { return url <  x.url; }
  bool operator==( const DirectoryInfo &x) const { return url == x.url; }
  bool operator!=( const DirectoryInfo &x) const { return url != x.url; }

  faodel::nodeid_t GetReferenceNode() { return url.reference_node; }
  bool Valid() const                  { return url.Valid(); }
  bool IsEmpty() const; //True when no url, info, or members. Sometimes used to pass back "no info"
  bool MeetsMinimumSize() const { return members.size() >= min_members; }


  bool GetChildReferenceNode(const std::string &child_name, faodel::nodeid_t *reference_node= nullptr) const;
  bool GetChildNameByReferenceNode(faodel::nodeid_t reference_node, std::string *child_name= nullptr) const;

  bool Join(faodel::nodeid_t node, const std::string &reference_name="");
  bool Leave(const faodel::ResourceURL &child_url);
  bool LeaveByNode(faodel::nodeid_t node);
  bool LeaveByName(const std::string &reference_name="");

  bool ContainsNode(faodel::nodeid_t node) const;

  void whookieInfo(faodel::ReplyStream &rs);

  //Serialization hook
  template <typename Archive>
  void serialize(Archive &ar, const unsigned int version){
    ar & url;
    ar & info;
    ar & min_members;
    ar & members;
  }

  std::string to_string() const;
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;

};

} // namespace opbox

#endif
