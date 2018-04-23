CXX=/usr/bin/g++
BOOST_PATH=${HOME}/local
SBL_PATH=${HOME}/projects/atdm/install
NNTI_PATH=.

${CXX} -std=c++11 -g -I${BOOST_PATH}/include -I${SBL_PATH}/include -pthread -D_GNU_SOURCE -DBOOST_LOG_DYN_LINK ${NNTI_PATH}/nnti/nnti_pch.hpp

