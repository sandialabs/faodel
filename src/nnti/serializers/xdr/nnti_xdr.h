// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/**
 *   @file nnti_xdr.h
 *
 *   @brief Essential definitions and include files for code that uses XDR.
 *
 *   @author Ron Oldfield (raoldfi\@sandia.gov)
 */

#ifndef _NNTI_XDR_H_
#define _NNTI_XDR_H_

#include "nnti/nntiConfig.h"

#include <rpc/types.h>
#include <rpc/xdr.h>

/* Some systems do not have xdr functions for uint16_t, uint32_t, uint64_t.
 * To fix the problem, define the missing function names here.
 */

#if defined(__MACH__)
/* For some insane reason Apple doesn't include the int8 XDR functions 
 * uint8_t is a synonym for unsigned char there, so replace with the XDR
 * for that type.*/
#define xdr_uint8_t xdr_u_char
#else
#ifdef NNTI_HAVE_XDR_U_INT8_T
#define xdr_uint8_t xdr_u_int8_t
#endif
#endif

#ifdef NNTI_HAVE_XDR_U_INT16_T
#define xdr_uint16_t xdr_u_int16_t
#endif
#ifdef NNTI_HAVE_XDR_U_INT32_T
#define xdr_uint32_t xdr_u_int32_t
#endif
#ifdef NNTI_HAVE_XDR_U_INT64_T
#define xdr_uint64_t xdr_u_int64_t
#endif

/* Handle pedantic definition of xdrproc_t args on Darwin */
#if defined(__MACH__)
#define XDRPROC_ARGS(arg1, arg2) arg1, arg2, 0
#else
#define XDRPROC_ARGS(arg1, arg2) arg1, arg2
#endif


#ifdef __cplusplus
extern "C" {
#endif

#if defined(__STDC__) || defined(__cplusplus)


/* On some systems (mac) xdr_sizeof is not in header file,
 * but it is in the library.
 */
#ifndef NNTI_HAVE_XDR_SIZEOF
extern unsigned long xdr_sizeof (xdrproc_t func, void *data);
#endif


#else /* K&R C */
#endif



#ifdef __cplusplus
}
#endif

#endif

