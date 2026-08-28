cmake_minimum_required(VERSION 3.20)

project(Scheduler LANGUAGES CXX)

function(scheduler_enable_warnings target_name)
  if(MSVC)
    target_compile_options(${target_name} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

set(SCHEDULER_CORE_SOURCES
  # Domain model and interactive protocol
  src/model/Protocol.cpp
  src/model/SystemConfig.cpp
  src/model/TaskTimeTable.cpp
  src/model/WorldState.cpp

  # Scheduling policy stack
  src/scheduling/AdaptiveScheduler.cpp
  src/scheduling/DecodeBatchScheduler.cpp
  src/scheduling/HighScoreScheduler.cpp
  src/scheduling/MultiprocessorScheduler.cpp
  src/scheduling/SchedulerCore.hpp
  src/scheduling/SchedulingAnalysis.cpp
  src/scheduling/ScoreAwareScheduler.cpp
  src/scheduling/SharedLinkScheduler.cpp
  src/scheduling/SingletonScheduler.cpp

  # Local simulation and diagnostics
  src/simulation/SimulationObservability.cpp
  src/simulation/Simulator.cpp
)

set(SCHEDULER_PUBLIC_HEADERS
  include/scheduler/Scheduler.hpp

  # Domain model and interactive protocol
  include/scheduler/model/LinkState.hpp
  include/scheduler/model/Protocol.hpp
  include/scheduler/model/SystemConfig.hpp
  include/scheduler/model/TaskTimeTable.hpp
  include/scheduler/model/WorldState.hpp

  # Scheduling policy interfaces
  include/scheduler/scheduling/AdaptiveScheduler.hpp
  include/scheduler/scheduling/DecodeBatchScheduler.hpp
  include/scheduler/scheduling/HighScoreScheduler.hpp
  include/scheduler/scheduling/MultiprocessorScheduler.hpp
  include/scheduler/scheduling/RemotePlacement.hpp
  include/scheduler/scheduling/SchedulingAnalysis.hpp
  include/scheduler/scheduling/ScoreAwareScheduler.hpp
  include/scheduler/scheduling/SharedLinkScheduler.hpp
  include/scheduler/scheduling/SingletonScheduler.hpp

  # Simulation and diagnostics
  include/scheduler/simulation/SimulationObservability.hpp
  include/scheduler/simulation/Simulator.hpp
)

add_library(scheduler_core ${SCHEDULER_CORE_SOURCES} ${SCHEDULER_PUBLIC_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files" FILES ${SCHEDULER_CORE_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX "Header Files" FILES ${SCHEDULER_PUBLIC_HEADERS})

target_include_directories(scheduler_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(scheduler_core PUBLIC cxx_std_20)
scheduler_enable_warnings(scheduler_core)

add_executable(scheduler src/app/scheduler_main.cpp)
target_link_libraries(scheduler PRIVATE scheduler_core)
scheduler_enable_warnings(scheduler)

add_executable(scheduler_simulator src/app/simulator_main.cpp)
target_link_libraries(scheduler_simulator PRIVATE scheduler_core)
scheduler_enable_warnings(scheduler_simulator)

include(CTest)

function(add_scheduler_test test_name)
  add_executable(${test_name} tests/${test_name}.cpp)
  target_link_libraries(${test_name} PRIVATE scheduler_core)
  scheduler_enable_warnings(${test_name})
  add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

if(BUILD_TESTING)
  add_scheduler_test(SystemConfigTest)
  add_scheduler_test(AdaptiveSchedulerTest)
  add_scheduler_test(HighScoreSchedulerTest)
  add_scheduler_test(MultiprocessorSchedulerTest)
  add_scheduler_test(TaskTimeTableTest)
  add_scheduler_test(ProtocolTest)
  add_scheduler_test(SchedulerTest)
  add_scheduler_test(ScoreAwareSchedulerTest)
  add_scheduler_test(SharedLinkSchedulerTest)
  add_scheduler_test(SimulatorTest)
  add_scheduler_test(SimulationObservabilityTest)
  add_scheduler_test(WorldStateTest)
endif()
