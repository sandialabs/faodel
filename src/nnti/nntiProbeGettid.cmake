
########## CHECK FOR HEADER FILES ############

INCLUDE(CheckIncludeFiles)

# Probe for syscall header files
CHECK_INCLUDE_FILES("unistd.h" NNTI_HAVE_UNISTD_H)
CHECK_INCLUDE_FILES("syscall.h" NNTI_HAVE_SYSCALL_H)

########## CHECK FOR FUNCTIONS ############

INCLUDE(CheckSymbolExists)
INCLUDE(CheckFunctionExists)
INCLUDE(CheckCSourceCompiles)

# syscall()
IF (NNTI_HAVE_UNISTD_H)
    CHECK_FUNCTION_EXISTS(syscall NNTI_HAVE_SYSCALL)
ENDIF (NNTI_HAVE_UNISTD_H)

# SYS_gettid
IF (NNTI_HAVE_SYSCALL_H)
    CHECK_SYMBOL_EXISTS(SYS_gettid "syscall.h" NNTI_HAVE_SYS_GETTID)
ENDIF (NNTI_HAVE_SYSCALL_H)

IF (NNTI_HAVE_SYSCALL)
    IF (NNTI_HAVE_SYS_GETTID)
        check_c_source_compiles(
            "#include <unistd.h>\n#include <syscall.h>\nint main(){syscall(SYS_gettid);return 0;}"
            NNTI_HAVE_GETTID
        )
    ENDIF (NNTI_HAVE_SYS_GETTID)
ENDIF (NNTI_HAVE_SYSCALL)
