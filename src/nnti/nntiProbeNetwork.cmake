
include(CheckLibraryExists)
include(CheckFunctionExists)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)
include(CheckTypeSize)


########## INCLUDE FILES ##############
include(CheckIncludeFiles)

check_include_files("malloc.h" NNTI_HAVE_MALLOC_H)
check_include_files("sys/types.h" NNTI_HAVE_SYS_TYPES_H)
check_include_files("sys/param.h" NNTI_HAVE_SYS_PARAM_H)
check_include_files("sys/ioctl.h" NNTI_HAVE_SYS_IOCTL_H)
check_include_files("sys/socket.h" NNTI_HAVE_SYS_SOCKET_H)
check_include_files("sys/sockio.h" NNTI_HAVE_SYS_SOCKIO_H)
check_include_files("netdb.h" NNTI_HAVE_NETDB_H)
check_include_files("sys/socket;net/if.h" NNTI_HAVE_NET_IF_H)
check_include_files("sys/socket.h;net/if_dl.h" NNTI_HAVE_NET_IF_DL_H)
check_include_files("sys/socket.h;net/if_arp.h" NNTI_HAVE_NET_IF_ARP_H)
check_include_files("netinet/in.h" NNTI_HAVE_NETINET_IN_H)
check_include_files("arpa/inet.h" NNTI_HAVE_ARPA_INET_H)
check_include_files("sys/types.h;sys/socket.h;ifaddrs.h" NNTI_HAVE_IFADDRS_H)

## Look for the expanded verbs API
check_include_files("infiniband/verbs_exp.h" NNTI_HAVE_VERBS_EXP_H)

check_include_files("endian.h" NNTI_HAVE_ENDIAN_H)


########## Probe for various network configurations ##############

check_function_exists(getifaddrs NNTI_HAVE_GETIFADDRS)

set(CMAKE_EXTRA_INCLUDE_FILES "netinet/in.h")
check_type_size("struct sockaddr_in" NNTI_HAVE_STRUCT_SOCKADDR_IN)
set(CMAKE_EXTRA_INCLUDE_FILES)

set(save_CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES}")
list(APPEND CMAKE_REQUIRED_LIBRARIES ibverbs)
check_c_source_compiles(
    "#include <infiniband/verbs_exp.h>\nint main(){ibv_exp_create_qp(NULL, NULL);return 0;}"
    NNTI_HAVE_IBV_EXP_CREATE_QP
)
check_c_source_compiles(
    "#include <infiniband/verbs_exp.h>\nint main(){ibv_exp_query_device(NULL, NULL);return 0;}"
    NNTI_HAVE_IBV_EXP_QUERY_DEVICE
)
set(CMAKE_REQUIRED_LIBRARIES "${save_CMAKE_REQUIRED_LIBRARIES}")

check_c_source_compiles(
    "#include <infiniband/verbs_exp.h>\nint main(){enum ibv_exp_qp_init_attr_comp_mask mask = IBV_EXP_QP_INIT_ATTR_ATOMICS_ARG;return 0;}"
    NNTI_HAVE_IBV_EXP_QP_INIT_ATTR_ATOMICS_ARG;
)
check_c_source_compiles(
    "#include <infiniband/verbs_exp.h>\nint main(){enum ibv_exp_qp_create_flags flags = IBV_EXP_QP_CREATE_ATOMIC_BE_REPLY;return 0;}"
    NNTI_HAVE_IBV_EXP_QP_CREATE_ATOMIC_BE_REPLY;
)
check_c_source_compiles(
    "#include <infiniband/verbs_exp.h>\nint main(){enum ibv_exp_atomic_cap caps = IBV_EXP_ATOMIC_HCA_REPLY_BE;return 0;}"
    NNTI_HAVE_IBV_EXP_ATOMIC_HCA_REPLY_BE;
)
check_c_source_compiles(
    "#include <endian.h>\nint main(){be64toh(1LL);return 0;}"
    NNTI_HAVE_BE64TOH;
)
check_c_source_compiles(
    "#include <infiniband/verbs_exp.h>\nint main(){enum ibv_exp_device_attr_comp_mask mask = IBV_EXP_DEVICE_ATTR_ODP;return 0;}"
    NNTI_HAVE_IBV_EXP_DEVICE_ATTR_ODP;
)
check_c_source_compiles(
    "#include <infiniband/verbs_exp.h>\nint main(){enum ibv_exp_access_flags flags = IBV_EXP_ACCESS_ON_DEMAND;return 0;}"
    NNTI_HAVE_IBV_EXP_ACCESS_ON_DEMAND;
)
check_c_source_compiles(
    "#include <infiniband/verbs_exp.h>\nint main(){enum ibv_odp_general_cap_bits caps = IBV_EXP_ODP_SUPPORT_IMPLICIT;return 0;}"
    NNTI_HAVE_IBV_EXP_ODP_SUPPORT_IMPLICIT;
)
