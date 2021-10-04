// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef DIRMAN_DIRMAN_HH
#define DIRMAN_DIRMAN_HH

#include "faodel-common/Common.hh"
#include "faodel-common/DirectoryInfo.hh"

#include "dirman/common/DirectoryCache.hh"
#include "dirman/common/DirectoryOwnerCache.hh"


namespace dirman {

std::string bootstrap();

bool Locate(const faodel::ResourceURL &search_url, faodel::nodeid_t *reference_node=nullptr);

bool GetDirectoryInfo(const faodel::ResourceURL &url, faodel::DirectoryInfo *dir_info);
bool GetLocalDirectoryInfo(const faodel::ResourceURL &url, faodel::DirectoryInfo *dir_info);
bool GetRemoteDirectoryInfo(const faodel::ResourceURL &url, faodel::DirectoryInfo *dir_info);

bool DefineNewDir(const faodel::ResourceURL &url);

bool HostNewDir(const faodel::DirectoryInfo &dir_info);
bool HostNewDir(const faodel::ResourceURL &url);

bool JoinDirWithoutName(const faodel::ResourceURL &url, faodel::DirectoryInfo *dir_info=nullptr);
bool JoinDirWithName(const faodel::ResourceURL &url, const std::string &name, faodel::DirectoryInfo *dir_info = nullptr);

bool LeaveDir(const faodel::ResourceURL &url, faodel::DirectoryInfo *dir_info=nullptr);

bool DropDir(const faodel::ResourceURL &url);

faodel::nodeid_t GetAuthorityNode();

void GetCachedNames(std::vector<std::string> *names);

} // namespace dirman


#endif // DIRMAN_DIRMAN_HH

