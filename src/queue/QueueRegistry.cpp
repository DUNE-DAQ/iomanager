/**
 * @file QueueRegistry.cpp
 *
 * The QueueRegistry class implementation
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/queue/QueueRegistry.hpp"

#include <map>
#include <memory>
#include <string>

namespace dunedaq::iomanager {

std::unique_ptr<QueueRegistry> QueueRegistry::s_instance = nullptr;

QueueRegistry&
QueueRegistry::get()
{
  if (!s_instance) {
    s_instance.reset(new QueueRegistry());
  }
  return *s_instance;
}

void
QueueRegistry::configure(const std::vector<QueueConfig>& configs)
{
  if (m_configured) {
    throw QueueRegistryConfigured(ERS_HERE);
  }

  m_queue_configs = configs;
  m_configured = true;
}

void
QueueRegistry::gather_stats(opmonlib::InfoCollector& ic, int level)
{

  for (const auto& [name, queue_entry] : m_queue_registry) {
    opmonlib::InfoCollector tmp_ci;
    queue_entry.m_instance->get_info(tmp_ci, level);
    if (!tmp_ci.is_empty()) {
      ic.add(name, tmp_ci);
    }
  }
}

bool
QueueRegistry::has_queue(const std::string& uid, const std::string& data_type) const
{
  for (auto& config : m_queue_configs) {
    if (config.id.uid == uid && config.id.data_type == data_type) {
      return true;
    }
  }

  return false;
}

std::set<std::string>
QueueRegistry::get_datatypes(const std::string& uid) const
{
  std::set<std::string> output;

  for (auto& config : m_queue_configs) {
    if (config.id.uid == uid) {
      output.insert(config.id.data_type);
    }
  }

  return output;
}

} // namespace dunedaq::iomanager
