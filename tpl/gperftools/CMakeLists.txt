INCLUDE (CheckIncludeFiles)
INCLUDE (CheckSymbolExists)
INCLUDE (CheckFunctionExists)
INCLUDE (CheckCXXSourceCompiles)

# CHECK_SYMBOL_EXISTS ("backtrace" HAVE_DECL_BACKTRACE)

CHECK_INCLUDE_FILES ("conflict-signal.h" HAVE_CONFLICT_SIGNAL_H)
CHECK_INCLUDE_FILES ("cygwin/signal.h" HAVE_CYGWIN_SIGNAL_H)
CHECK_INCLUDE_FILES ("dlfcn.h" HAVE_DLFCN_H)
CHECK_INCLUDE_FILES ("execinfo.h" HAVE_EXECINFO_H)
CHECK_INCLUDE_FILES ("fcntl.h" HAVE_FCNTL_H)
CHECK_INCLUDE_FILES ("features.h" HAVE_FEATURES_H)
CHECK_INCLUDE_FILES ("glob.h" HAVE_GLOB_H)
CHECK_INCLUDE_FILES ("grp.h" HAVE_GRP_H)
CHECK_INCLUDE_FILES ("inttypes.h" HAVE_INTTYPES_H)
CHECK_INCLUDE_FILES ("libunwind.h" HAVE_LIBUNWIND_H)
CHECK_INCLUDE_FILES ("linux/ptrace.h" HAVE_LINUX_PTRACE_H)
CHECK_INCLUDE_FILES ("malloc.h" HAVE_MALLOC_H)
CHECK_INCLUDE_FILES ("malloc/malloc.h" HAVE_MALLOC_MALLOC_H)
CHECK_INCLUDE_FILES ("memory.h" HAVE_MEMORY_H)
CHECK_INCLUDE_FILES ("poll.h" HAVE_POLL_H)
CHECK_INCLUDE_FILES ("pthread.h" HAVE_PTHREAD)
CHECK_INCLUDE_FILES ("pwd.h" HAVE_PWD_H)
CHECK_INCLUDE_FILES ("sched.h" HAVE_SCHED_H)
CHECK_INCLUDE_FILES ("stdint.h" HAVE_STDINT_H)
CHECK_INCLUDE_FILES ("stdlib.h" HAVE_STDLIB_H)
CHECK_INCLUDE_FILES ("strings.h" HAVE_STRINGS_H)
CHECK_INCLUDE_FILES ("string.h" HAVE_STRING_H)
CHECK_INCLUDE_FILES ("sys/cdefs.h" HAVE_SYS_CDEFS_H)
CHECK_INCLUDE_FILES ("sys/malloc.h" HAVE_SYS_MALLOC_H)
CHECK_INCLUDE_FILES ("sys/param.h" HAVE_SYS_PARAM_H)
CHECK_INCLUDE_FILES ("sys/prctl.h" HAVE_SYS_PRCTL_H)
CHECK_INCLUDE_FILES ("sys/resource.h" HAVE_SYS_RESOURCE_H)
CHECK_INCLUDE_FILES ("sys/socket.h" HAVE_SYS_SOCKET_H)
CHECK_INCLUDE_FILES ("sys/stat.h" HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILES ("sys/syscall.h" HAVE_SYS_SYSCALL_H)
CHECK_INCLUDE_FILES ("sys/types.h" HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILES ("sys/ucontext.h" HAVE_SYS_CONTEXT_H)
CHECK_INCLUDE_FILES ("sys/wait.h" HAVE_SYS_WAIT_H)
CHECK_INCLUDE_FILES ("ucontext.h" HAVE_UCONTEXT_H)
CHECK_INCLUDE_FILES ("unistd.h" HAVE_UNISTD_H)
CHECK_INCLUDE_FILES ("unwind.h" HAVE_UNWIND_H)
CHECK_INCLUDE_FILES ("valgrind.h" HAVE_VALGRIND_H)

CHECK_FUNCTION_EXISTS ("sbrk" HAVE_SBRK)
CHECK_FUNCTION_EXISTS ("mmap" HAVE_MMAP)
CHECK_FUNCTION_EXISTS ("posix_memalign" HAVE_MEMALIGN)

CHECK_CXX_SOURCE_COMPILES(
  "#include <unwind.h>
   int main(int argc, char **argv)
   {
     &_Unwind_Backtrace;
     return 0;
   }"
  unwind_backtrace_exists)

IF (unwind_backtrace_exists)
  SET(HAVE_UNWIND_BACKTRACE 1 CACHE STRING "unwind_backtrace")
  message( STATUS "UNWIND BACKTRACE works" )
ELSE()
  message( STATUS "UNWIND BACKTRACE **doesn't** work" )
ENDIF()

CHECK_CXX_SOURCE_COMPILES(
  "#include <stdlib.h>
   static void foo(void) __attribute__ ((unused));
   void foo(void) 
   { 
      return; 
   }
   int main(int argc, char **argv)
   {
     return 0;
   }"
  supports__attribute__)

IF (supports__attribute__)
  SET(HAVE___ATTRIBUTE__ 1 CACHE STRING "have_attribute")
  message( STATUS "Supports ATTRIBUTE" )
ELSE()
  message( STATUS "*DOESN'T support ATTRIBUTE" )
ENDIF()


CHECK_CXX_SOURCE_COMPILES(
  "#include <malloc.h>
   void* (* volatile __malloc_hook)(size_t, const void*) = 0;
   int main(int argc, char **argv)
   {
       return 0;
   }"
  malloc_hook_volatile)

IF (malloc_hook_volatile)
  SET(Faodel_PERFTOOLS_MALLOC_HOOK_MAYBE_VOLATILE "volatile" CACHE STRING "does malloc hook need volatile")
ELSE ()
  SET(Faodel_PERFTOOLS_MALLOC_HOOK_MAYBE_VOLATILE "" CACHE STRING "does malloc hook need volatile")
ENDIF ()
mark_as_advanced(Faodel_PERFTOOLS_MALLOC_HOOK_MAYBE_VOLATILE)

CHECK_CXX_SOURCE_COMPILES(
  "#include <malloc.h>
   int main(int argc, char **argv)
   {
       struct mallinfo m = mallinfo();
       return 0;
   }"
  malloc_mallinfo)

IF (malloc_mallinfo)
  SET(Faodel_PERFTOOLS_HAVE_STRUCT_MALLINFO 1 CACHE STRING "support for mallinfo")
ENDIF ()
mark_as_advanced(Faodel_PERFTOOLS_HAVE_STRUCT_MALLINFO)

set(Faodel_PERFTOOLS_DLL_DECL "" CACHE STRING "On windows, should be __declspec(dllexport)")
set(Faodel_PERFTOOLS_STL_NAMESPACE "std" CACHE STRING "namespace where STL code like vector<> is defined")
option(Faodel_PERFTOOLS_TCMALLOC_32K_PAGES "32K of internal page size" OFF)
option(Faodel_PERFTOOLS_TCMALLOC_64K_PAGES "64K of internal page size" OFF)
option(Faodel_PERFTOOLS_TCMALLOC_ALIGN_8BYTES "8 bytes of allocation alignment" OFF)
# option(HAVE_MMAP "System has MMAP" ON)
# option(NO_HEAP_CHECK "Don't check the heap" ON)
#set(VERSION "lunasa" CACHE STRING "version string for the config")
mark_as_advanced(Faodel_PERFTOOLS_DLL_DECL)
mark_as_advanced(Faodel_PERFTOOLS_STL_NAMESPACE)
mark_as_advanced(Faodel_PERFTOOLS_TCMALLOC_32K_PAGES)
mark_as_advanced(Faodel_PERFTOOLS_TCMALLOC_64K_PAGES)
mark_as_advanced(Faodel_PERFTOOLS_TCMALLOC_ALIGN_8BYTES)

set(Faodel_PERFTOOLS_PACKAGE_VERSION  "2.5" CACHE STRING "version of this package")
mark_as_advanced(Faodel_PERFTOOLS_PACKAGE_VERSION)
execute_process(COMMAND expr "${Faodel_PERFTOOLS_PACKAGE_VERSION}" : "\\([[0-9]]*\\)" OUTPUT_VARIABLE MAJOR)
execute_process(COMMAND expr "${Faodel_PERFTOOLS_PACKAGE_VERSION}" : "[[0-9]]*\\.\\([[0-9]]*\\)" OUTPUT_VARIABLE MINOR)
execute_process(COMMAND expr "${Faodel_PERFTOOLS_PACKAGE_VERSION}" : "[[0-9]]*\\.[[0-9]]*\\(.*\\)$" OUTPUT_VARIABLE PATCH)

# In some cases, the assignment of these variables includes a newline which 
# is problematic for their subsequent use.  This should solve that problem
# where it exists
string(STRIP "${MAJOR}" MAJOR)
string(STRIP "${MINOR}" MINOR)
string(STRIP "${PATCH}" PATCH)

set(Faodel_PERFTOOLS_TC_VERSION_MAJOR "${MAJOR}" CACHE STRING "major version of this package")
set(Faodel_PERFTOOLS_TC_VERSION_MINOR "${MINOR}" CACHE STRING "minor version of this package")
set(Faodel_PERFTOOLS_TC_VERSION_PATCH "${PATCH}" CACHE STRING "patch version of this package")
mark_as_advanced(Faodel_PERFTOOLS_TC_VERSION_MAJOR)
mark_as_advanced(Faodel_PERFTOOLS_TC_VERSION_MINOR)
mark_as_advanced(Faodel_PERFTOOLS_TC_VERSION_PATCH)

set(Faodel_PERFTOOLS_PRIdS "%d" CACHE STRING "printf format code for printing size_t and ssize_t")
set(Faodel_PERFTOOLS_PRIuS "%u" CACHE STRING "printf format code for printing size_t and ssize_t")
set(Faodel_PERFTOOLS_PRIxS "%x" CACHE STRING "printf format code for printing size_t and ssize_t")
mark_as_advanced(Faodel_PERFTOOLS_PRIdS)
mark_as_advanced(Faodel_PERFTOOLS_PRIuS)
mark_as_advanced(Faodel_PERFTOOLS_PRIxS)

configure_file(src/config.h.in "${CMAKE_CURRENT_SOURCE_DIR}/src/config.h")
configure_file(src/gperftools/tcmalloc.h.in "${CMAKE_CURRENT_SOURCE_DIR}/src/gperftools/tcmalloc.h")

set(LOGGING_INCLUDES
  src/base/logging.h
  src/base/commandlineflags.h
  src/base/basictypes.h
  src/base/dynamic_annotations.h)

set(LOGGING_SOURCES
  src/base/logging.cc
  src/base/dynamic_annotations.c)

set(SYSINFO_INCLUDES
  src/base/sysinfo.h
  src/getenv_safe.h
  src/base/logging.h
  src/base/commandlineflags.h
  src/base/arm_instruction_set_select.h
  src/base/basictypes.h)

set(SYSINFO_SOURCES
  src/base/sysinfo.cc)

set(SPINLOCK_INCLUDES
  src/base/spinlock.h
  src/base/spinlock_internal.h
  src/base/atomicops.h 
  src/base/atomicops-internals-macosx.h
  src/base/atomicops-internals-linuxppc.h
  src/base/atomicops-internals-windows.h
  src/base/atomicops-internals-x86.h)

set(SPINLOCK_SOURCES
  src/base/spinlock.cc
  src/base/spinlock_internal.cc
  src/base/atomicops-internals-x86.cc)

set(spinlock_lib_target spinlock)
add_library(spinlock src/config.h.in
  ${SPINLOCK_INCLUDES} 
  ${SYSINFO_INCLUDES} 
  ${LOGGING_INCLUDES} 
  ${SPINLOCK_SOURCES} 
  ${SYSINFO_SOURCES}
  ${LOGGING_SOURCES})

target_include_directories( spinlock
  PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  INTERFACE $<INSTALL_INTERFACE:include/faodel> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  )
      

set(GPERF_HEADERS
  src/gperftools/tcmalloc.h
  src/gperftools/heap-checker.h
  src/gperftools/heap-profiler.h
  src/gperftools/malloc_extension.h
  src/gperftools/malloc_extension_c.h
  src/gperftools/malloc_hook.h
  src/gperftools/malloc_hook_c.h
  src/gperftools/profiler.h
  src/gperftools/stacktrace.h)

set(TCMALLOC_MINIMAL_SOURCES
  src/tcmalloc.cc
  src/system-alloc.cc
  src/common.cc
  src/fake_stacktrace_scope.cc
  src/internal_logging.cc
  src/memfs_malloc.cc
  src/central_freelist.cc
  src/page_heap.cc
  src/sampler.cc
  src/span.cc
  src/stacktrace.cc
  src/stack_trace_table.cc
  src/static_vars.cc
  src/symbolize.cc
  src/maybe_threads.cc
  src/thread_cache.cc
  src/malloc_hook.cc
  src/malloc_extension.cc
  src/memory_region_map.cc
  src/heap-checker.cc
  src/heap-checker-bcad.cc
  src/heap-profile-table.cc
  src/base/linuxthreads.cc
  src/base/thread_lister.c
  src/raw_printer.cc
  src/base/low_level_alloc.cc)


add_library( tcmalloc src/config.h.in ${GPERF_HEADERS} ${TCMALLOC_MINIMAL_SOURCES} )
target_link_libraries( tcmalloc spinlock Threads::Threads )
set_target_properties( tcmalloc PROPERTIES LINKER_LANGUAGE  CXX )
target_include_directories( tcmalloc
  PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  INTERFACE $<INSTALL_INTERFACE:include/faodel> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  )

install(TARGETS tcmalloc spinlock
  EXPORT        FaodelTargets
  RUNTIME       DESTINATION "${BINARY_INSTALL_DIR}" COMPONENT bin
  ARCHIVE       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  LIBRARY       DESTINATION "${LIBRARY_INSTALL_DIR}" COMPONENT lib
  PUBLIC_HEADER DESTINATION "${INCLUDE_INSTALL_DIR}/faodel"
  COMPONENT dev
  INCLUDES      DESTINATION "${INCLUDE_INSTALL_DIR}"
  )


