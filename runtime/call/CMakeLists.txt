
set(MAPPING_MODE "2MB_PAGES")
#set(MAPPING_MODE "4KB_PAGES")
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

if(MAPPING_MODE STREQUAL "2MB_PAGES")
    target_compile_definitions(rt_call PUBLIC MEGAPAGE_MAPPING)
endif()
