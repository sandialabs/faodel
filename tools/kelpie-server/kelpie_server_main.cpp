// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <exception>
#include <iostream>

#include "kelpie/services/PoolServerDriver.hh"


int main(int argc, char **argv) {

  kelpie::PoolServerDriver driver(argc, argv);

  try {
    return driver.run();
  }
  catch(const std::exception &xe) {
    std::cerr << xe.what() << std::endl;
    return -1;
  }

}
