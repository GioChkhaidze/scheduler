cmake_minimum_required(VERSION 3.20)

project(Scheduler LANGUAGES CXX)

function(scheduler_enable_warnings target_name)
  if(MSVC)
    target_compile_options(${target_name} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

# Add new production .cpp files to this list.
add_library(scheduler_core
  src/SystemConfig.cpp
  src/TaskTimeTable.cpp
  src/Protocol.cpp
  src/WorldState.cpp
)

target_include_directories(scheduler_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(scheduler_core PUBLIC cxx_std_20)
scheduler_enable_warnings(scheduler_core)

add_executable(scheduler src/main.cpp)
target_link_libraries(scheduler PRIVATE scheduler_core)
scheduler_enable_warnings(scheduler)

include(CTest)

function(add_scheduler_test test_name)
  add_executable(${test_name}
    tests/${test_name}.cpp
  )

  target_link_libraries(${test_name} PRIVATE scheduler_core)
  scheduler_enable_warnings(${test_name})

  add_test(
    NAME ${test_name}
    COMMAND ${test_name}
  )
endfunction()

if(BUILD_TESTING)
  # Add each new tests/FooTest.cpp file with one line here.
  add_scheduler_test(SystemConfigTest)
  add_scheduler_test(TaskTimeTableTest)
  add_scheduler_test(ProtocolTest)
  add_scheduler_test(WorldStateTest)
endif()
