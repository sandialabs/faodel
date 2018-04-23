#include "Lunasa.h"
#include <iostream>

int main (int argc, char *argv[])
{
  NNTI_transport_t transport;
  NNTI_init (NNTI_DEFAULT_TRANSPORT, 0, &transport);

  Lunasa lunasa (&transport, 1, (NNTI_buf_ops_t)(NNTI_GET_SRC|NNTI_PUT_DST));

  void *mem1 = lunasa.Alloc (100);
  void *mem2 = lunasa.Alloc (100);
  void *mem3 = lunasa.Alloc (100);
  void *mem4 = lunasa.Alloc (100);
  void *mem5 = lunasa.Alloc (100);

  lunasa.Free (mem2);

  void *mem6 = lunasa.Alloc (200);

  if (!lunasa.SanityCheck () ||
      lunasa.TotalPages () != 1 ||
      lunasa.TotalPaged () != 1024 ||
      lunasa.TotalFreeSegments () != 2 || 
      lunasa.TotalAllocated () != 600)
  {
    std::cerr << "Expected: 1, 1024, 2, 600" << std::endl;
    std::cerr << "Got: " << lunasa.TotalPages () 
              << ", " << lunasa.TotalPaged ()
              << ", " << lunasa.TotalFreeSegments ()
              << ", " << lunasa.TotalAllocated () << std::endl;
    return 1;
  }

  void *mem7 = lunasa.Alloc (600);

  if (!lunasa.SanityCheck () ||
      lunasa.TotalPages () != 2 ||
      lunasa.TotalPaged () != 2048 ||
      lunasa.TotalFreeSegments () != 3 || 
      lunasa.TotalAllocated () != 1200)
  {
    std::cerr << "Expected: 2, 2048, 3, 1200" << std::endl;
    std::cerr << "Got: " << lunasa.TotalPages () 
              << ", " << lunasa.TotalPaged ()
              << ", " << lunasa.TotalFreeSegments ()
              << ", " << lunasa.TotalAllocated () << std::endl;
    return 1;
  }

  void *mem8 = lunasa.Alloc (1200);

  if (!lunasa.SanityCheck () ||
      lunasa.TotalPages () != 3 ||
      lunasa.TotalPaged () != 4096 ||
      lunasa.TotalFreeSegments () != 4 || 
      lunasa.TotalAllocated () != 2400)
  {
    std::cerr << "Expected: 3, 4096, 4, 2400" << std::endl;
    std::cerr << "Got: " << lunasa.TotalPages () 
              << ", " << lunasa.TotalPaged ()
              << ", " << lunasa.TotalFreeSegments ()
              << ", " << lunasa.TotalAllocated () << std::endl;
    return 1;
  }


  lunasa.PrintState (std::cout);

  NNTI_fini (&transport);

  return 0;
} 
