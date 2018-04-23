// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <string>

#include "kelpie/Kelpie.hh"

#include "kelpie/core/KelpieCoreBase.hh"

#include "kelpie/core/Singleton.hh"

using std::string;


namespace kelpie {


/**
 * @brief Report on whether Kelpie is currently configured or not
 *
 * @retval TRUE  Kelpie has been initialized with a configuration
 * @retval FALSE Kelpie has not been initialized yet
 */
bool IsUnconfigured() {
  return kelpie::internal::Singleton::impl.IsUnconfigured();
}


/**
 * @brief Registration for users to add user-defined pools to Kelpie
 *
 * @param[in] pool_name The name of the pool type (eg mydht)
 * @param[in] ctor_function Function handler for creating a new pool
 */
void RegisterPoolConstructor(std::string pool_name, fn_PoolCreate_t ctor_function) {
  kelpie::internal::Singleton::impl.core->RegisterPoolConstructor(
                                        pool_name, ctor_function);
}


/** @brief Establish a connection to a particular resource based on its url
 *
 * @param[in] pool_url URL for the resource
 * @retval Pool A handle for publishing and retrieving data
 * @note Kelpie will reuse a Pool if one already exists
 */
Pool Connect(const faodel::ResourceURL &pool_url) {
  return kelpie::internal::Singleton::impl.core->Connect(pool_url);
}


/**
 * @brief Establish a connection to a particular resource based on its url
 *
 * @param[in] url_string URL for the resource
 * @retval Pool A handle for publishing and retrieving data
 * @note Kelpie will reuse a Pool if one already exists
 */
Pool Connect(string url_string) {
  return kelpie::internal::Singleton::impl.core->Connect(faodel::ResourceURL(url_string));
}


/**
 * @brief Register a new user-defined I/O Module (IOM)
 *
 * @param[in] type A name for this category of IOM
 * @param[in] ctor_function A function for constructing new instances of this IOM
 */
void RegisterIomConstructor(std::string type, fn_IomConstructor_t ctor_function) {
  return kelpie::internal::Singleton::impl.core->RegisterIomConstructor(type, ctor_function);
}


}  // namespace kelpie
