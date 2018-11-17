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
