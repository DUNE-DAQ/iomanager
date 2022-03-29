cmake_minimum_required(VERSION 3.12)
project(iomanager VERSION 0.0.0)

find_package(daq-cmake REQUIRED)

daq_setup_environment()

find_package(Boost COMPONENTS unit_test_framework REQUIRED)
find_package(appfwk REQUIRED)
find_package(serialization REQUIRED)
find_package(dfmessages REQUIRED)

##############################################################################
daq_add_library(LINK_LIBRARIES appfwk::appfwk serialization::serialization)

daq_add_application(iomanager_test_io_manager test_io_manager_app.cxx TEST LINK_LIBRARIES iomanager)
daq_add_application(iomanager_test_serreg test_serreg_app.cxx TEST LINK_LIBRARIES iomanager)

daq_add_unit_test(FakeTRB_test LINK_LIBRARIES iomanager dfmessages::dfmessages)
daq_add_unit_test(IOManager_test LINK_LIBRARIES iomanager)
daq_add_unit_test(performance_test LINK_LIBRARIES iomanager)

daq_install()

