# for static linking and in some limited environment like ci we need to adapt jobs.
option(LOW_RESOURCES "enforce very low resource usage, shouldn't be needed but a quick workaround which will limit jobs to 1 linking / 2 compilation jobs" OFF)
message(STATUS "Low resource option: " ${LOW_RESOURCES})


# get system information
cmake_host_system_information(RESULT system_cores QUERY NUMBER_OF_LOGICAL_CORES)
cmake_host_system_information(RESULT system_ram QUERY AVAILABLE_PHYSICAL_MEMORY)

# is the current system cpu or memory limited?
# -> calculate jobs for it
#
# hints:
# - llvm static needs up to 4GB per job
# - compile jobs and links jobs can run at the same time
math(EXPR system_ram_possible_jobs "${system_ram} / 4000 - 1" OUTPUT_FORMAT DECIMAL)
if (system_ram_possible_jobs LESS 1 OR LOW_RESOURCES)
    set(system_ram_possible_jobs 1)
endif()

get_property(TEMP_JOB_POOLS GLOBAL PROPERTY JOB_POOLS)
if (system_ram_possible_jobs LESS system_cores)
    math(EXPR compile_jobs "${system_ram_possible_jobs} * 2" OUTPUT_FORMAT DECIMAL)
    message("Your System is Memory limited, linking jobs: ${system_ram_possible_jobs} compile jobs: ${compile_jobs}")
    set_property(GLOBAL PROPERTY JOB_POOLS ${TEMP_JOB_POOLS} linking=${system_ram_possible_jobs} compile=${compile_jobs})
else()
    math(EXPR compile_jobs "${system_cores} * 1" OUTPUT_FORMAT DECIMAL)
    message("Your System is CPU limited, linking jobs: ${system_cores} compile jobs: ${compile_jobs}")
    set_property(GLOBAL PROPERTY JOB_POOLS ${TEMP_JOB_POOLS} linking=${system_cores} compile=${compile_jobs})
endif()
unset(TEMP_JOB_POOLS)

# apply created job pools
if (NOT CMAKE_JOB_POOL_LINK)
    set(CMAKE_JOB_POOL_LINK linking)
else()
    message("Using provided job pool list for linking: ${CMAKE_JOB_POOL_LINK}")
endif()
if (NOT CMAKE_JOB_POOL_COMPILE)
    set(CMAKE_JOB_POOL_COMPILE compile)
else()
    message("Using provided job pool list for linking: ${CMAKE_JOB_POOL_COMPILE}")
endif()
