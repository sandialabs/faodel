// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef WHOOKIE_CLIENT_HH
#define WHOOKIE_CLIENT_HH

#include <string>

#include "faodel-common/Common.hh"

namespace whookie {

int retrieveData(faodel::nodeid_t nid, const std::string &path, std::string *data=nullptr);
int retrieveData(const std::string &server, const std::string &port, const std::string &path, std::string *data=nullptr);
int retrieveData(const std::string &server, unsigned int port, const std::string &path, std::string *data=nullptr);




} // namespace whookie

#endif // WHOOKIE_CLIENT_HH

