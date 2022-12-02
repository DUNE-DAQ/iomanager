/**
 * @file IOManager.cpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/IOManager.hpp"

#include <memory>

std::shared_ptr<dunedaq::iomanager::IOManager> dunedaq::iomanager::IOManager::s_instance = nullptr;