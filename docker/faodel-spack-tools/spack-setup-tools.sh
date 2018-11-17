#!/bin/bash

TOOLS="cmake doxygen git gdb ninja tree"

. /usr/share/Modules/init/sh 
. ${SPACK_ROOT}/share/spack/setup-env.sh 


# ssl fix: the -k fixes an https download (hwlock)
# unsafe: one of the packages won't build as root
FORCE_UNSAFE_CONFIGURE=1 spack -k install ${TOOLS}

# Python causes conflicts?
spack view --verbose -e python symlink -i ${SPACK_INSTALL} ${TOOLS}
