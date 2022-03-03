# CMake에서 주석은 #을 사용해 작성합니다
set(AUTHOR_NAME "bradkim06")
cmake_minimum_required(VERSION 3.16) # CMake 버전을 명시
set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# specify the cross compiler
# set(TI_COMPILER /Applications/ti/ccs1110/ccs/tools/compiler/ti-cgt-msp430_21.6.0.LTS/bin/cl430)
# set(CMAKE_C_COMPILER ${TI_COMPILER})
# set(CMAKE_CPP_COMPILER ${TI_COMPILER})
# set(CMAKE_ASM_COMPILER ${TI_COMPILER})

# skip compiler tests
# set(CMAKE_C_COMPILER_WORKS 1)
# set(CMAKE_CPP_COMPILER_WORKS 1)
# set(CMAKE_ASM_COMPILER_WORKS 1)

add_compile_definitions(__MSP430F5419A__)
add_compile_definitions(NBIOT_MODEM_BC95G)
add_compile_definitions(NDEBUG)
add_compile_definitions(NBIOT_DEVICE)
add_compile_definitions(NBIOT_LG_TYPE)


# add_compile_definitions(AUX_REPEATER)
# Windows
# set(PROJECT_ROOT /mnt/c/Users/User/workspace_v8/term/src/Master/SRC)
# Mac
set(CCS_BASE_ROOT /Applications/ti/ccs1110/ccs/ccs_base)
set(CG_TOOL_ROOT /Applications/ti/ccs1110/ccs/tools/compiler/ti-cgt-msp430_21.6.0.LTS)
set(PROJECT_ROOT /Users/bradkim06/study/hitec/ht_terminal/src/Master/SRC)
set(TEST_CODE /Users/bradkim06/study/hitec/ht_terminal/test)
project(terminalV1)

file(GLOB_RECURSE SRC ${PROJECT_ROOT}/*.c ${TEST_CODE}/*.c)
# list(FILTER ENDF6_SRC EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/Test/.*" )
set(App ${SRC})
add_executable(App ${SRC})
include_directories(
  ${PROJECT_ROOT}
  ${CCS_BASE_ROOT}/msp430/include
  ${CG_TOOL_ROOT}/include
  ${PROJECT_ROOT}/app
  ${PROJECT_ROOT}/dev
  ${PROJECT_ROOT}/misc
  ${PROJECT_ROOT}/osal
  ${PROJECT_ROOT}/test
  ${PROJECT_ROOT}/modem/nbiot_lg
  ${PROJECT_ROOT}/modem/nbiot_lg/uuid
  /Library/Ruby/Gems/2.6.0/gems/ceedling-0.31.1/vendor/unity/src/)
