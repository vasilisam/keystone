
#set(MAPPING_MODE "2M_PAGES")
set(MAPPING_MODE "1G_PAGES")

set(CALL_SOURCES sbi.c syscall.c)

if(LINUX_SYSCALL)
    list(APPEND CALL_SOURCES linux_wrap.c)
endif()

if(IO_SYSCALL)
    list(APPEND CALL_SOURCES io_wrap.c)
endif()

if(NET_SYSCALL)
    list(APPEND CALL_SOURCES net_wrap.c)
endif()

add_library(rt_call STATIC ${CALL_SOURCES})

if(MAPPING_MODE STREQUAL "2M_PAGES")
    target_compile_definitions(rt_call PUBLIC MEGAPAGE_MAPPING)
elseif(MAPPING_MODE STREQUAL "1G_PAGES")
    target_compile_definitions(rt_call PUBLIC GIGAPAGE_MAPPING)
endif()
