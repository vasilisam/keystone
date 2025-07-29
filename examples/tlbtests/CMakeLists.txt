# Define the mapping mode
#set(MAPPING_MODE "2MB_PAGES")
set(MAPPING_MODE "4KB_PAGES")
message(STATUS "Selected mapping mode: ${MAPPING_MODE}")

set(host_bin tlbtest-runner)
set(host_src host/host.cpp)
set(eyrie_plugins "io_syscall linux_syscall env_setup")

set(package_name "tlbtests.ke")
set(package_script "./run-tlbtest.sh")

###############################################
#                TLB-Test Apps                    #
###############################################
# to add a test, (1) add the test to the following variable
set(all_tlbtest_bins
  tlbtest-8
  tlbtest-16
  tlbtest-32
  tlbtest-64
  tlbtest-128
  tlbtest-256
  tlbtest-512
  tlbtest-1024)

# and (2) define the recipe of the test below:

# tlbtest-8
add_executable(tlbtest-8 tlbtest_8/tlbtest_8.c)
target_link_libraries(tlbtest-8 "-static")

# tlbtest-16
add_executable(tlbtest-16 tlbtest_16/tlbtest_16.c)
target_link_libraries(tlbtest-16 "-static")

# tlbtest-32
add_executable(tlbtest-32 tlbtest_32/tlbtest_32.c)
target_link_libraries(tlbtest-32 "-static")

# tlbtest-64
add_executable(tlbtest-64 tlbtest_64/tlbtest_64.c)
target_link_libraries(tlbtest-64 "-static")

# tlbtest-128
add_executable(tlbtest-128 tlbtest_128/tlbtest_128.c)
target_link_libraries(tlbtest-128 "-static")

# tlbtest-256
add_executable(tlbtest-256 tlbtest_256/tlbtest_256.c)
target_link_libraries(tlbtest-256 "-static")

# tlbtest-512
add_executable(tlbtest-512 tlbtest_512/tlbtest_512.c)
target_link_libraries(tlbtest-512 "-static")

# tlbtest-1024
add_executable(tlbtest-1024 tlbtest_1024/tlbtest_1024.c)
target_link_libraries(tlbtest-1024 "-static")

###############################################
# a script for running all test enclaves
set(test_script run-tlbtest.sh)

# Define compile-time flags if MAPPING_MODE is non-default
# and linker flags for all tlbtests
if(MAPPING_MODE STREQUAL "2MB_PAGES")
  foreach (tlbtest IN ITEMS ${all_tlbtest_bins})
    target_compile_definitions(${tlbtest} PUBLIC MEGAPAGE_MAPPING)
    target_link_options(${tlbtest} PRIVATE -static -Wl,-z,norelro
                                                  -Wl,-z,max-page-size=0x200000)
  endforeach(tlbtest)
elseif(NOT MAPPING_MODE STREQUAL "4KB_PAGES")
  message(FATAL_ERROR "Unknown MAPPING_MODE: ${MAPPING_MODE}")
endif()


# host

add_executable(${host_bin} ${host_src})
target_link_libraries(${host_bin} ${KEYSTONE_LIB_HOST} ${KEYSTONE_LIB_EDGE})

# add target for Eyrie runtime (see keystone.cmake)

set(eyrie_files_to_copy .options_log eyrie-rt loader.bin)
add_eyrie_runtime(tlbtest-eyrie
  ${eyrie_plugins}
  ${eyrie_files_to_copy})

# add target for packaging (see keystone.cmake)

add_keystone_package(tlbtest-package
  ${package_name}
  ${package_script}
  ${test_script} ${eyrie_files_to_copy} ${all_tlbtest_bins} ${host_bin})

add_dependencies(tlbtest-package tlbtest-eyrie)

# add package to the top-level target
add_dependencies(examples tlbtest-package)