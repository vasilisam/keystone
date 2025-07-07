# Define the mapping mode
set(MAPPING_MODE "2MB_PAGES")
message(STATUS "Selected mapping mode: ${MAPPING_MODE}")

set(eapp_bin tlbtest)
set(eapp_src eapp/tlbtest.c)
set(host_bin tlbtest-runner)
set(host_src host/host.cpp)
set(package_name "tlbtest.ke")
set(package_script "./tlbtest-runner tlbtest eyrie-rt loader.bin")
set(eyrie_plugins "io_syscall linux_syscall env_setup")

# eapp

add_executable(${eapp_bin} ${eapp_src})
target_link_libraries(${eapp_bin} "-static")

# Define compile-time flags if MAPPING_MODE is non-default
if(MAPPING_MODE STREQUAL "2MB_PAGES")
  target_compile_definitions(${eapp_bin} PUBLIC MEGAPAGE_MAPPING)
  target_link_options(${eapp_bin} PRIVATE -static -Wl,-z,norelro
                                                -Wl,-z,max-page-size=0x200000)
elseif(NOT MAPPING_MODE STREQUAL "4KB_PAGES")
  message(FATAL_ERROR "Unknown MAPPING_MODE: ${MAPPING_MODE}")
endif()


# host

add_executable(${host_bin} ${host_src})
target_link_libraries(${host_bin} ${KEYSTONE_LIB_HOST} ${KEYSTONE_LIB_EDGE})

# add target for Eyrie runtime (see keystone.cmake)

set(eyrie_files_to_copy .options_log eyrie-rt loader.bin)
add_eyrie_runtime(${eapp_bin}-eyrie
  ${eyrie_plugins}
  ${eyrie_files_to_copy})

# add target for packaging (see keystone.cmake)

add_keystone_package(${eapp_bin}-package
  ${package_name}
  ${package_script}
  ${eyrie_files_to_copy} ${eapp_bin} ${host_bin})

add_dependencies(${eapp_bin}-package ${eapp_bin}-eyrie)

# add package to the top-level target
add_dependencies(examples ${eapp_bin}-package)
