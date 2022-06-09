cmake_minimum_required(VERSION 3.12)
project(iomanager VERSION 1.0.1)

find_package(daq-cmake REQUIRED)

daq_setup_environment()

find_package(Boost COMPONENTS unit_test_framework REQUIRED)
find_package(ipm REQUIRED)
find_package(opmonlib REQUIRED)
find_package(folly REQUIRED)
find_package(serialization REQUIRED)
find_package(utilities REQUIRED)
find_package(networkmanager REQUIRED)

##############################################################################
set(IOMANAGER_DEPENDENCIES serialization::serialization Folly::folly utilities::utilities opmonlib::opmonlib ipm::ipm networkmanager::networkmanager)

daq_codegen(connection.jsonnet queue.jsonnet TEMPLATES Structs.hpp.j2 Nljs.hpp.j2)
daq_codegen(queueinfo.jsonnet DEP_PKGS opmonlib TEMPLATES opmonlib/InfoStructs.hpp.j2 opmonlib/InfoNljs.hpp.j2 )

daq_add_library(QueueRegistry.cpp IOManager.cpp LINK_LIBRARIES ${IOMANAGER_DEPENDENCIES})

daq_add_application(iomanager_test_io_manager test_io_manager_app.cxx TEST LINK_LIBRARIES iomanager)
daq_add_application( queue_IO_check queue_IO_check.cxx TEST LINK_LIBRARIES iomanager )

daq_add_unit_test(IOManager_test LINK_LIBRARIES iomanager)
daq_add_unit_test(performance_test LINK_LIBRARIES iomanager)
daq_add_unit_test(FollyQueue_test             LINK_LIBRARIES iomanager )
daq_add_unit_test(FollyQueue_metric_test      LINK_LIBRARIES iomanager )
daq_add_unit_test(Queue_test                  LINK_LIBRARIES iomanager )
daq_add_unit_test(QueueRegistry_test          LINK_LIBRARIES iomanager )
daq_add_unit_test(StdDeQueue_test             LINK_LIBRARIES iomanager )

daq_install()

