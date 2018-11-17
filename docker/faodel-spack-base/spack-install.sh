#!/bin/bash

# Install the basic packages that spack needs
yum install -y \
    automake \
    bzip2 \
    environment-modules \
    gcc \
    gcc-c++ \
    git \
    make \
    patch \
    wget

# get rid of repo cache
yum clean all
rm -rf /var/cache/yum

# Clone the spack repo
cd / &&  git clone https://github.com/spack/spack.git 

# Tell spack about a mirror if one is defined
if [ "x" != "x${SPACK_MIRROR}" ]; then
   spack mirror add my_mirror ${SPACK_MIRROR}
fi


# Disable ssl checks on package downloads. This is not recommended, unless you
# have a proxy that intercepts https and causes curl to fail.
if [ "x${SPACK_INSECURE}" == "xTRUE" ]; then
   sed -i 's/verify_ssl: true/verify_ssl: false/' ${SPACK_ROOT}/etc/spack/defaults/config.yaml 
fi

# Specify that the host's repo should be used
sed -i '/^packages:/a\
  perl:\
    paths:\
      perl: /usr\
    buildable: False' ${SPACK_ROOT}/etc/spack/defaults/packages.yaml


# Use modules 
. /usr/share/Modules/init/sh


# Build initial gcc, which we will eventually throw away
# This is in a retry loop because downloads sometimes fail
for i in $(seq 5); do 
    spack install ${GCC_VERSION} && break
done
if [ $? -ne 0 ]; then
    echo "Spack install of gcc failed"
    exit 1
fi


# Load our environment so we can use spack tools 
. ${SPACK_ROOT}/share/spack/setup-env.sh

# Use gcc8.2.0 and add it to the compilers
spack load ${GCC_VERSION} && spack compiler find

# Make it spack's default compiler
sed -i 's/compiler: \[.*\]/compiler: \['${GCC_VERSION}'\]/' ${SPACK_ROOT}/etc/spack/defaults/packages.yaml

# Use gcc8.2.0 to build gcc8.2.0 (so **all** tools can be under 8.2.0)
for i in $(seq 5); do 
    spack install ${GCC_VERSION} && break
done
if [ $? -ne 0 ]; then
    echo "Spack install of gcc failed"
    exit 1
fi

# Erase all the 4.8.5 modules and use the new compiler
spack uninstall -y --all %gcc@4.8.5
spack compiler remove gcc@4.8.5

# Set the compiler paths
spack load ${GCC_VERSION} && spack compiler find

# Create a link farm that includes gcc
spack view --verbose symlink -i ${SPACK_INSTALL} gcc 

# Get rid of old stuff
spack clean
