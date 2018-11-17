#!/bin/bash

if [ -z "$SPECIALIZATION_EXTENSION" ]; then
    SPECIALIZATION_EXTENSION=x86_64
fi

echo "Using extension: $SPECIALIZATION_EXTENSION"


stages=(faodel-spack-base faodel-spack-tools faodel-spack-tpls)


# We specialize to certain platforms in the faodel-spack-base directory. Check
# to make sure there's a dockerfile with that specialization extension, and 
# that there's a symlink to it. Make the symlink if the specialization file
# exists and we're missing the correct symlink.
cd faodel-spack-base
target=Dockerfile.${SPECIALIZATION_EXTENSION}
if [ ! -e "${target}" ]; then
  echo "Error: faodel-spack-base/$target does not exist"
  exit 1
fi
if [ "$(readlink -- Dockerfile)" != "${target}" ]; then
  rm -f Dockerfile
  ln -s ${target} Dockerfile
fi
cd ..


# Now build all the containers
for i in ${stages[*]}; do
    echo "=======> Building $i using dockerfile $fname"
    docker build $i
done


