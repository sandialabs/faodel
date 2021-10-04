// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef OPBOX_NBR_HH
#define OPBOX_NBR_HH

#include "faodelConfig.h"

#if Faodel_OPBOX_NET_NNTI
#include "nnti/nntiConfig.h"

#if NNTI_BUILD_MPI
#define MAX_NET_BUFFER_REMOTE_SIZE 68 /* 4 + 4 + 60 */
#elif NNTI_BUILD_UGNI
#define MAX_NET_BUFFER_REMOTE_SIZE 48 /* 4 + 4 + 40 */
#elif NNTI_BUILD_IBVERBS
#define MAX_NET_BUFFER_REMOTE_SIZE 36 /* 4 + 4 + 28 */
#else
#error NNTI did not have a valid transport. OpBox cannot be built.
#endif

#elif Faodel_OPBOX_NET_LIBFABRIC
#define MAX_NET_BUFFER_REMOTE_SIZE 32 /* TODO: 32 = xx + yy... show your work */

#else
#error Opbox must be configured with either nnti or libfabric
#endif

#endif // OPBOX_NBR_HH
