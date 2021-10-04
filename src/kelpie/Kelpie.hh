// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_KELPIE_HH
#define KELPIE_KELPIE_HH

#include <iostream>
#include <string>

#include "faodel-common/Configuration.hh"
#include "faodel-common/ResourceURL.hh"

#include "kelpie/common/Types.hh"
#include "kelpie/pools/Pool.hh"

// This header provides some shortcuts for users to interact with different
// Kelpie components. Users can (1) get Kelpie's bootstrap dependencies,
// (2) register their own IOM/Pool drivers, and (3) connect to new/existing
// resource pools.
//
// Most users will just connect to a pool and use its api to access data.

namespace kelpie {

std::string bootstrap();  // Kelpie-specific bootstrap. Registers kelpie-specific dependencies
bool IsUnconfigured(); // Whether kelpie is currently configured

//Shortcuts for...
void RegisterIomConstructor(std::string type, fn_IomConstructor_t ctor_function, fn_IomGetValidSetting_t valid_settings_function); // ..user-defined IOM driver
void RegisterPoolConstructor(std::string pool_name, fn_PoolCreate_t ctor_function); // ..user-defined pools

void RegisterComputeFunction(const std::string &compute_function_name, fn_compute_t function_pointer);

int JoinServerPool(const faodel::ResourceURL &url, const std::string &optional_node_name="");

std::vector<std::string> GetRegisteredIomTypes();
std::vector<std::pair<std::string,std::string>> GetRegisteredIOMTypeParameters(const std::string &type);

std::vector<std::string> GetIomNames();


Pool Connect(const faodel::ResourceURL &resource_url); // ..connecting to a pool
Pool Connect(const std::string url_string); // ..connecting to a pool

}  // namespace kelpie

#endif  // KELPIE_KELPIE_HH
