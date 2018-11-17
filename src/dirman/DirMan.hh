// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef OPBOX_DIRECTORYMANAGER_HH
#define OPBOX_DIRECTORYMANAGER_HH

#include "common/Common.hh"

#include "opbox/services/dirman/DirectoryInfo.hh"
#include "opbox/services/dirman/common/DirectoryCache.hh"
#include "opbox/services/dirman/common/DirectoryOwnerCache.hh"


namespace opbox {
namespace dirman {


bool Locate(const faodel::ResourceURL &search_url, faodel::nodeid_t *reference_node=nullptr);

bool GetDirectoryInfo(const faodel::ResourceURL &url, DirectoryInfo *dir_info);
bool GetLocalDirectoryInfo(const faodel::ResourceURL &url, DirectoryInfo *dir_info);
bool GetRemoteDirectoryInfo(const faodel::ResourceURL &url, DirectoryInfo *dir_info);

bool HostNewDir(const DirectoryInfo &dir_info);
bool HostNewDir(const faodel::ResourceURL &url);

bool JoinDirWithoutName(const faodel::ResourceURL &url, DirectoryInfo *dir_info=nullptr);
bool JoinDirWithName(const faodel::ResourceURL &url, std::string name, DirectoryInfo *dir_info=nullptr);

bool LeaveDir(const faodel::ResourceURL &url, DirectoryInfo *dir_info=nullptr);

} // namespace dirman
} // namespace opbox

#endif // OPBOX_DIRECTORYMANAGER_HH

