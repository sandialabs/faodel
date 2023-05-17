// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_KELPIE_CLIENT_HH
#define FAODEL_KELPIE_CLIENT_HH

#include "KelpieClientAction.hh"

std::string kelpieGetPoolFromEnv();

int kelpieClient_Dispatch(kelpie::Pool &pool, faodel::Configuration &config, const KelpieClientAction &parsed);


#endif //FAODEL_KELPIE_CLIENT_HH
