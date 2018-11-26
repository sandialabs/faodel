/* src/config.h.in.  Generated from configure.ac by autoheader.  */


#ifndef GPERFTOOLS_CONFIG_H_
#define GPERFTOOLS_CONFIG_H_

/*
 * When gperftools was incorporated into Faodel, the option and 
 * feature variables exposed to users were prefixed with 
 * Faodel_PERFTOOLS_ to prevent namespace polution and make it 
 * easier for users to find groups of variables.
 *
 * Here we define a set of boolean variables for those option 
 * and feature variables.
 *
 * Later we will use these to define or undef the original 
 * gperftools variables.
 *
 */
#define Faodel_PERFTOOLS_MALLOC_HOOK_MAYBE_VOLATILE 1
#define Faodel_PERFTOOLS_HAVE_STRUCT_MALLINFO 1
#define Faodel_PERFTOOLS_DLL_DECL 0
#define Faodel_PERFTOOLS_STL_NAMESPACE 1
#define Faodel_PERFTOOLS_TCMALLOC_32K_PAGES 0
#define Faodel_PERFTOOLS_TCMALLOC_64K_PAGES 0
#define Faodel_PERFTOOLS_TCMALLOC_ALIGN_8BYTES 0
#define Faodel_PERFTOOLS_PACKAGE_VERSION 1
#define Faodel_PERFTOOLS_TC_VERSION_MAJOR 1
#define Faodel_PERFTOOLS_TC_VERSION_MINOR 1
#define Faodel_PERFTOOLS_TC_VERSION_PATCH 0
#define Faodel_PERFTOOLS_PRIdS 1
#define Faodel_PERFTOOLS_PRIuS 1
#define Faodel_PERFTOOLS_PRIxS 1


/* Build runtime detection for sized delete */
/* #undef ENABLE_DYNAMIC_SIZED_DELETE */

/* Build sized deletion operators */
/* #undef ENABLE_SIZED_DELETE */

/* Define to 1 if compiler supports __builtin_expect */
/* #undef HAVE_BUILTIN_EXPECT */

/* Define to 1 if compiler supports __builtin_stack_pointer */
/* #undef HAVE_BUILTIN_STACK_POINTER */

/* Define to 1 if you have the <conflict-signal.h> header file. */
/* #undef HAVE_CONFLICT_SIGNAL_H */
/* #undef HAVE_CONFLICT_SIGNAL_H */

/* Define to 1 if you have the <cygwin/signal.h> header file. */
/* #undef HAVE_CYGWIN_SIGNAL_H */

/* Define to 1 if you have the declaration of `backtrace', and to 0 if you
   don't. */
/* #undef HAVE_DECL_BACKTRACE */

/* Define to 1 if you have the declaration of `cfree', and to 0 if you don't.
   */
/* #undef HAVE_DECL_CFREE */

/* Define to 1 if you have the declaration of `memalign', and to 0 if you
   don't. */
/* #undef HAVE_DECL_MEMALIGN */

/* Define to 1 if you have the declaration of `nanosleep', and to 0 if you
   don't. */
/* #undef HAVE_DECL_NANOSLEEP */

/* Define to 1 if you have the declaration of `posix_memalign', and to 0 if
   you don't. */
/* #undef HAVE_DECL_POSIX_MEMALIGN */

/* Define to 1 if you have the declaration of `pvalloc', and to 0 if you
   don't. */
/* #undef HAVE_DECL_PVALLOC */

/* Define to 1 if you have the declaration of `sleep', and to 0 if you don't.
   */
/* #undef HAVE_DECL_SLEEP */

/* Define to 1 if you have the declaration of `uname', and to 0 if you don't.
   */
/* #undef HAVE_DECL_UNAME */

/* Define to 1 if you have the declaration of `valloc', and to 0 if you don't.
   */
/* #undef HAVE_DECL_VALLOC */

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H

/* Define to 1 if the system has the type `Elf32_Versym'. */
/* #undef HAVE_ELF32_VERSYM */

/* Define to 1 if you have the <execinfo.h> header file. */
#define HAVE_EXECINFO_H

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H

/* Define to 1 if you have the <features.h> header file. */
#define HAVE_FEATURES_H

/* Define to 1 if you have the `fork' function. */
/* #undef HAVE_FORK */

/* Define to 1 if you have the `geteuid' function. */
/* #undef HAVE_GETEUID */

/* Define to 1 if you have the `getpagesize' function. */
/* #undef HAVE_GETPAGESIZE */

/* Define to 1 if you have the <glob.h> header file. */
#define HAVE_GLOB_H

/* Define to 1 if you have the <grp.h> header file. */
#define HAVE_GRP_H

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H

/* Define to 1 if you have the <libunwind.h> header file. */
/* #undef HAVE_LIBUNWIND_H */

/* Define to 1 if you have the <linux/ptrace.h> header file. */
#define HAVE_LINUX_PTRACE_H

/* Define if this is Linux that has SIGEV_THREAD_ID */
/* #undef HAVE_LINUX_SIGEV_THREAD_ID */

/* Define to 1 if you have the <malloc.h> header file. */
#define HAVE_MALLOC_H

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H

/* Define to 1 if you have a working `mmap' system call. */
#define HAVE_MMAP

/* Define to 1 if you have a working `memalign' system call. */
#define HAVE_MEMALIGN

/* define if the compiler implements namespaces */
/* #undef HAVE_NAMESPACES */

/* Define to 1 if you have the <poll.h> header file. */
#define HAVE_POLL_H

/* define if libc has program_invocation_name */
/* #undef HAVE_PROGRAM_INVOCATION_NAME */

/* Define if you have POSIX threads libraries and header files. */
#define HAVE_PTHREAD 1

/* defined to 1 if pthread symbols are exposed even without include pthread.h
   */
/* #undef HAVE_PTHREAD_DESPITE_ASKING_FOR */

/* Define to 1 if you have the <pwd.h> header file. */
#define HAVE_PWD_H

/* Define to 1 if you have the `sbrk' function. */
#define HAVE_SBRK

/* Define to 1 if you have the <sched.h> header file. */
#define HAVE_SCHED_H

/* Define to 1 if you have the <stdint.h> header file. */
/*#define HAVE_STDINT_H */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H

/* Define to 1 if the system supports `mallinfo'. */
#if (1 == Faodel_PERFTOOLS_HAVE_STRUCT_MALLINFO)
#define HAVE_STRUCT_MALLINFO 1
#endif

/* Define to 1 if you have the <sys/cdefs.h> header file. */
#define HAVE_SYS_CDEFS_H

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H

/* Define to 1 if you have the <sys/prctl.h> header file. */
#define HAVE_SYS_PRCTL_H

/* Define to 1 if you have the <sys/resource.h> header file. */
#define HAVE_SYS_RESOURCE_H

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H

/* Define to 1 if you have the <sys/syscall.h> header file. */
#define HAVE_SYS_SYSCALL_H

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H

/* Define to 1 if you have the <sys/ucontext.h> header file. */
/* #undef HAVE_SYS_UCONTEXT_H */

/* Define to 1 if you have the <sys/wait.h> header file. */
#define HAVE_SYS_WAIT_H

/* Define to 1 if compiler supports __thread */
/* #undef HAVE_TLS */

/* Define to 1 if you have the <ucontext.h> header file. */
#define HAVE_UCONTEXT_H

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H

/* Whether <unwind.h> contains _Unwind_Backtrace */
/* #undef HAVE_UNWIND_BACKTRACE */

/* Define to 1 if you have the <unwind.h> header file. */
#define HAVE_UNWIND_H

/* Define to 1 if you have the <valgrind.h> header file. */
/* #undef HAVE_VALGRIND_H */

/* define if your compiler has __attribute__ */
/* #undef HAVE___ATTRIBUTE__ */

/* Define to 1 if compiler supports __environ */
/* #undef HAVE___ENVIRON */

/* Define to 1 if the system has the type `__int64'. */
/* #undef HAVE___INT64 */

/* prefix where we look for installed files */
/* #undef INSTALL_PREFIX */

/* Define to 1 if int32_t is equivalent to intptr_t */
/* #undef INT32_EQUALS_INTPTR */

/* Define to the sub-directory where libtool stores uninstalled libraries. */
/* #undef LT_OBJDIR */

/* Define to 'volatile' if __malloc_hook is declared volatile */
#define MALLOC_HOOK_MAYBE_VOLATILE volatile

/* Name of package */
/* #undef PACKAGE */

/* Define to the address where bug reports for this package should be sent. */
/* #undef PACKAGE_BUGREPORT */

/* Define to the full name of this package. */
/* #undef PACKAGE_NAME */

/* Define to the full name and version of this package. */
/* #undef PACKAGE_STRING */

/* Define to the one symbol short name of this package. */
/* #undef PACKAGE_TARNAME */

/* Define to the home page for this package. */
/* #undef PACKAGE_URL */

/* Define to the version of this package. */
#define PACKAGE_VERSION 2.5

/* How to access the PC from a struct ucontext */
/* #undef PC_FROM_UCONTEXT */

/* Always the empty-string on non-windows systems. On windows, should be
   "__declspec(dllexport)". This way, when we compile the dll, we export our
   functions/classes. It's safe to define this here because config.h is only
   used internally, to compile the DLL, and every DLL source file #includes
   "config.h" before anything else. */
#define PERFTOOLS_DLL_DECL

/* printf format code for printing a size_t and ssize_t */
#if (1 == Faodel_PERFTOOLS_PRIdS)
#define PRIdS "%d"
#endif

/* printf format code for printing a size_t and ssize_t */
#if (1 == Faodel_PERFTOOLS_PRIuS)
#define PRIuS "%u"
#endif

/* printf format code for printing a size_t and ssize_t */
#if (1 == Faodel_PERFTOOLS_PRIxS)
#define PRIxS "%x"
#endif

/* Mark the systems where we know it's bad if pthreads runs too
   early before main (before threads are initialized, presumably).  */
#ifdef __FreeBSD__
#define PTHREADS_CRASHES_IF_RUN_TOO_EARLY 1
#endif

/* Define to necessary symbol if this constant uses a non-standard name on
   your system. */
/* #undef PTHREAD_CREATE_JOINABLE */

/* Define to 1 if you have the ANSI C header files. */
/* #undef STDC_HEADERS */

/* the namespace where STL code like vector<> is defined */
#define STL_NAMESPACE std

/* Define 32K of internal pages size for tcmalloc */
#if (1 == Faodel_PERFTOOLS_TCMALLOC_32K_PAGES)
#define TCMALLOC_32K_PAGES OFF
#endif

/* Define 64K of internal pages size for tcmalloc */
#if (1 == Faodel_PERFTOOLS_TCMALLOC_64K_PAGES)
#define TCMALLOC_64K_PAGES OFF
#endif

/* Define 8 bytes of allocation alignment for tcmalloc */
#if (1 == Faodel_PERFTOOLS_TCMALLOC_ALIGN_BYTES)
#define TCMALLOC_ALIGN_BYTES 
#endif

/* Version number of package */
/*
#define VERSION 
*/

/* C99 says: define this to get the PRI... macros from stdint.h */
#ifndef __STDC_FORMAT_MACROS
# define __STDC_FORMAT_MACROS 1
#endif

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif


#ifdef __MINGW32__
#include "windows/mingw.h"
#endif

#endif  /* #ifndef GPERFTOOLS_CONFIG_H_ */
