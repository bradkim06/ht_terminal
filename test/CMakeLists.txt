# CMake에서 주석은 #을 사용해 작성합니다
set(AUTHOR_NAME "bradkim06")
cmake_minimum_required(VERSION 3.16) # CMake 버전을 명시
set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_definitions(__MSP430F5419A__)
add_compile_definitions(NBIOT_MODEM_BC95G)
add_compile_definitions(NDEBUG)
add_compile_definitions(NBIOT_LG_TYPE)
add_compile_definitions(TDD_TEST)
add_compile_definitions(TDD_DEBUG)


# add_compile_definitions(AUX_REPEATER)
# Windows
# set(PROJECT_ROOT /mnt/c/Users/User/workspace_v8/term/src/Master/SRC)
# Mac
set(PROJECT_ROOT /Users/bradkim06/study/hitec/ht_terminal/src/Master/SRC)
project(terminalV1)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.h" "*.c")
set(App ${SRC})
add_executable(App ${SRC})
include_directories(
  ${PROJECT_ROOT}
  ${PROJECT_ROOT}/app
  ${PROJECT_ROOT}/dev
  ${PROJECT_ROOT}/misc
  ${PROJECT_ROOT}/osal
  ${PROJECT_ROOT}/test
  ${PROJECT_ROOT}/modem/nbiot_lg
  ${PROJECT_ROOT}/modem/nbiot_lg/uuid
  /Library/Ruby/Gems/2.6.0/gems/ceedling-0.31.1/vendor/unity/src/)
