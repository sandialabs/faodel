FROM faodel-spack-tools
MAINTAINER Craig Ulmer

COPY ./spack-setup-faodel-tpls.sh /tmp/
RUN . /tmp/spack-setup-faodel-tpls.sh

# Set the default environment so login is ready to go
RUN echo ". ${SPACK_ROOT}/share/spack/setup-env.sh"      >> /root/.bashrc
RUN echo "export PKG_CONFIG_PATH=/install/lib/pkgconfig" >> /root/.bashrc
RUN echo "export LD_LIBRARY_PATH=/install/lib"           >> /root/.bashrc
RUN echo "export PATH=/install/bin:${PATH}"              >> /root/.bashrc
RUN useradd test -p test

# Builds always need envs installed
ENV BOOST_ROOT /install
ENV GTEST_ROOT /install
