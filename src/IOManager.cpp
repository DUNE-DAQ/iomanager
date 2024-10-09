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
dunedaq::iomanager::IOManager::configure(std::string session,
                                         std::vector<const confmodel::Queue*> queues,
                                         std::vector<const confmodel::NetworkConnection*> connections,
                                         const confmodel::ConnectivityService* connection_service,
                                         dunedaq::opmonlib::OpMonManager& opmgr)
{
  m_session = session;

  QueueRegistry::get().configure(queues, opmgr);
  NetworkManager::get().configure(session, connections, connection_service, opmgr);
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
