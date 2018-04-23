#include "Lunasa.h"
#include <iostream>
#include <vector>

#include <stdlib.h>
#include <limits.h>

int main (int argc, char *argv[])
{
  NNTI_transport_t transport;
  NNTI_init (NNTI_DEFAULT_TRANSPORT, 0, &transport);

  Lunasa lunasa (&transport, 1, (NNTI_buf_ops_t)(NNTI_GET_SRC|NNTI_PUT_DST));

  int allocations = (argc >= 2 ? atoi (argv[1]) : 10000);
  std::vector<void*> memory; 

  for (int i = 0; i < allocations; i ++) {
    float r = static_cast<float>(random ()) / static_cast<float>(INT_MAX);
    memory.push_back (lunasa.Alloc (r * lunasa.PageSize () * 4));
  }

  if (!lunasa.SanityCheck ()) {
    return 1;
  }

  lunasa.PrintState (std::cout);

  std::cout << "TotalPages:         " << lunasa.TotalPages () << std::endl;
  std::cout << "TotalPaged:         " << lunasa.TotalPaged () << std::endl;
  std::cout << "TotalAllocated:     " << lunasa.TotalAllocated () << std::endl;
  std::cout << "TotalAllocSegments: " << lunasa.TotalAllocSegments () << std::endl;
  std::cout << "TotalFreeSegments:  " << lunasa.TotalFreeSegments () << std::endl;

  float alloc = static_cast<float>(lunasa.TotalAllocated ()) / static_cast<float>(lunasa.TotalAllocSegments ());
  float free = static_cast<float>(lunasa.TotalPaged () - lunasa.TotalAllocated ()) / static_cast<float>(lunasa.TotalFreeSegments ());
  std::cout << "Avg Alloc Size:     " << alloc << " bytes" << std::endl;
  std::cout << "Avg Free Size:      " << free << " bytes" << std::endl;

  float overhead = static_cast<float>(lunasa.TotalPaged ()) / static_cast<float>(lunasa.TotalAllocated ()) - 1.0;
  std::cout << "Overhead:           " << static_cast<int>(overhead * 100) << "%" << std::endl;

  NNTI_fini (&transport);

  return 0;
}
