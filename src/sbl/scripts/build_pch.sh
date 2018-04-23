CXX=/usr/bin/g++
BOOST_PATH=${HOME}/local
SBL_PATH=.

${CXX} -std=c++11 -g -I${BOOST_PATH}/include -DBOOST_LOG_DYN_LINK ${SBL_PATH}/sbl/sbl_boost_headers.hpp

