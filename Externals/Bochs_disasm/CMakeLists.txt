set(SRCS dis_decode.cc
         dis_groups.cc
         resolve.cc
         syntax.cc)

add_library(bdisasm STATIC ${SRCS})
