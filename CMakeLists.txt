cmake_minimum_required(VERSION 3.12)
project(iomanager VERSION 1.6.0)

find_package(daq-cmake REQUIRED)

daq_setup_environment()

find_package(Boost COMPONENTS unit_test_framework REQUIRED)
find_package(ipm REQUIRED)
find_package(opmonlib REQUIRED)
find_package(folly REQUIRED)
find_package(serialization REQUIRED)
find_package(utilities REQUIRED)

##############################################################################
set(IOMANAGER_DEPENDENCIES serialization::serialization Folly::folly utilities::utilities opmonlib::opmonlib ipm::ipm)

daq_codegen(connection.jsonnet TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )
daq_codegen(connectioninfo.jsonnet queueinfo.jsonnet DEP_PKGS opmonlib TEMPLATES opmonlib/InfoStructs.hpp.j2 opmonlib/InfoNljs.hpp.j2 )

daq_add_library(IOManager.cpp queue/QueueRegistry.cpp network/NetworkManager.cpp network/ConfigClient.cpp LINK_LIBRARIES ${IOMANAGER_DEPENDENCIES} )

daq_add_application(queue_IO_check            queue_IO_check.cxx         TEST LINK_LIBRARIES iomanager )
daq_add_application(config_client_test        config_client_test.cxx     TEST LINK_LIBRARIES iomanager pthread )
daq_add_application(iomanager_stress_test     iomanager_stress_test.cxx  TEST LINK_LIBRARIES iomanager pthread )
daq_add_application(iomanager_stress_test_pubsub     iomanager_stress_test_pubsub.cxx  TEST LINK_LIBRARIES iomanager pthread )
daq_add_application(reconnection_test     reconnection_test.cxx  TEST LINK_LIBRARIES iomanager pthread )

# DPF-2023-Mar-17: No better way to replace tbb/task.h since Intel deprecated it.
target_compile_options( iomanager_stress_test PRIVATE
	$<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-DTBB_SUPPRESS_DEPRECATED_MESSAGES=1>)
target_compile_options( iomanager_stress_test_pubsub PRIVATE
	$<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-DTBB_SUPPRESS_DEPRECATED_MESSAGES=1>)
target_compile_options( reconnection_test PRIVATE
	$<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-DTBB_SUPPRESS_DEPRECATED_MESSAGES=1>)

daq_add_application( queues_vs_threads_folly          queues_vs_threads_folly.cxx          TEST LINK_LIBRARIES iomanager )
daq_add_application( queues_vs_threads_folly_throwing queues_vs_threads_folly_throwing.cxx TEST LINK_LIBRARIES iomanager )
daq_add_application( queues_vs_threads_iomanager      queues_vs_threads_iomanager.cxx      TEST LINK_LIBRARIES iomanager )

daq_add_unit_test(IOManager_test         LINK_LIBRARIES iomanager )
daq_add_unit_test(performance_test       LINK_LIBRARIES iomanager )
daq_add_unit_test(FollyQueue_test        LINK_LIBRARIES iomanager )
daq_add_unit_test(FollyQueue_metric_test LINK_LIBRARIES iomanager )
daq_add_unit_test(NetworkManager_test    LINK_LIBRARIES iomanager )
daq_add_unit_test(Queue_test             LINK_LIBRARIES iomanager )
daq_add_unit_test(QueueRegistry_test     LINK_LIBRARIES iomanager )
daq_add_unit_test(StdDeQueue_test        LINK_LIBRARIES iomanager )

daq_install()

