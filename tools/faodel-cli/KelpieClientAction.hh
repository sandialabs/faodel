// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_KELPIECLIENTACTION_HH
#define FAODEL_KELPIECLIENTACTION_HH

#include <string>
#include "kelpie/Kelpie.hh"

#include "ActionInterface.hh"

class KelpieClientAction
        : public ActionInterface {
public:
  KelpieClientAction() : kget_meta_only(false), kload_all_dir_entries(false) {}
  KelpieClientAction(const std::string &long_or_short_cmd);

  std::string pool_name;
  std::string file_name;
  std::string dir_name;

  uint64_t generate_meta_size;
  uint64_t generate_data_size;

  std::string meta;
  bool kget_meta_only;
  bool kload_all_dir_entries;

  std::vector<kelpie::Key> keys;

  uint16_t getMetaCapacity() const;

  faodel::rc_t ParseArgs(const std::vector<std::string> &args, const std::string &default_pool);


private:
  faodel::rc_t appendKey(const std::string &k1, const std::string &k2);
  faodel::rc_t appendKey(const std::string &string_separated_by_pipe);

  bool parseArgValue(std::string *result, const std::vector<std::string> &args, size_t *iptr, const std::string &s1, const std::string &s2);


};





#endif //FAODEL_KELPIECLIENTACTION_HH
