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

void
dunedaq::iomanager::IOManager::configure(Queues_t queues,
                                         Connections_t connections,
                                         bool use_config_client,
                                         std::chrono::milliseconds config_client_interval,
					 dunedaq::opmonlib::OpMonManager & opmgr)
{
  char* system = getenv("DUNEDAQ_SYSTEM");
  if (system) {
    m_system = std::string(system);
  } else {
    system = getenv("DUNEDAQ_PARTITION");
    if (system) {
      m_system = std::string(system);
    } else {
      throw(EnvNotFound(ERS_HERE, "DUNEDAQ_SYSTEM"));
    }
  }

  Queues_t qCfg = queues;
  Connections_t nwCfg;

  for (auto& connection : connections) {
    nwCfg.push_back(connection);
  }

  QueueRegistry::get().configure(qCfg, opmgr);
  NetworkManager::get().configure(nwCfg, use_config_client, config_client_interval, opmgr);
}

void
dunedaq::iomanager::IOManager::shutdown()
{
  QueueRegistry::get().shutdown();
  NetworkManager::get().shutdown();
  m_senders.clear();
  m_receivers.clear();
}

void
dunedaq::iomanager::IOManager::reset()
{
  QueueRegistry::get().reset();
  NetworkManager::get().reset();
  m_senders.clear();
  m_receivers.clear();
  s_instance = nullptr;
}

std::set<std::string>
dunedaq::iomanager::IOManager::get_datatypes(std::string const& uid)
{
  auto output = QueueRegistry::get().get_datatypes(uid);
  auto networks = NetworkManager::get().get_datatypes(uid);
  for (auto& dt : networks) {
    output.insert(dt);
  }
  return output;
}
