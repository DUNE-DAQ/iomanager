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
                                         std::chrono::milliseconds config_client_interval)
{
  char* session = getenv("DUNEDAQ_SESSION");
  if (session) {
    m_session = std::string(session);
  } else {
    session = getenv("DUNEDAQ_PARTITION");
    if (session) {
      m_session = std::string(session);
    } else {
      throw(EnvNotFound(ERS_HERE, "DUNEDAQ_SESSION"));
    }
  }

  Queues_t qCfg = queues;
  Connections_t nwCfg;

  for (auto& connection : connections) {
    nwCfg.push_back(connection);
  }

  QueueRegistry::get().configure(qCfg);
  NetworkManager::get().configure(nwCfg, use_config_client, config_client_interval);
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
