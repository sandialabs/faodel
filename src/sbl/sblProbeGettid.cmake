
########## CHECK FOR HEADER FILES ############

INCLUDE(CheckIncludeFiles)

# Probe for syscall header files
CHECK_INCLUDE_FILES("unistd.h" SBL_HAVE_UNISTD_H)
CHECK_INCLUDE_FILES("syscall.h" SBL_HAVE_SYSCALL_H)

########## CHECK FOR FUNCTIONS ############

INCLUDE(CheckSymbolExists)
INCLUDE(CheckFunctionExists)
INCLUDE(CheckCSourceCompiles)

# syscall()
IF (SBL_HAVE_UNISTD_H)
    CHECK_FUNCTION_EXISTS(syscall SBL_HAVE_SYSCALL)
ENDIF (SBL_HAVE_UNISTD_H)

# SYS_gettid
IF (SBL_HAVE_SYSCALL_H)
    CHECK_SYMBOL_EXISTS(SYS_gettid "syscall.h" SBL_HAVE_SYS_GETTID)
ENDIF (SBL_HAVE_SYSCALL_H)

IF (SBL_HAVE_SYSCALL)
    IF (SBL_HAVE_SYS_GETTID)
        check_c_source_compiles(
            "#include <unistd.h>\n#include <syscall.h>\nint main(){syscall(SYS_gettid);return 0;}"
            SBL_HAVE_GETTID
        )
    ENDIF (SBL_HAVE_SYS_GETTID)
ENDIF (SBL_HAVE_SYSCALL)
