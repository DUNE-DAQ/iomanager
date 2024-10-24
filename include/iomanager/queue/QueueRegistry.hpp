/**
 * @file QueueRegistry.hpp
 *
 * The QueueRegistry class declarations
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_QUEUEREGISTRY_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_QUEUEREGISTRY_HPP_

#include "iomanager/SchemaUtils.hpp"
#include "iomanager/queue/Queue.hpp"
#include "iomanager/queue/QueueIssues.hpp"

#include "confmodel/Queue.hpp"
#include "opmonlib/OpMonManager.hpp"

#include "ers/Issue.hpp"

#include <map>
#include <memory>
#include <string>

namespace dunedaq {
namespace iomanager {

/**
 * @brief The QueueRegistry class manages all Queue instances and gives out
 * handles to the Queues upon request
 */
class QueueRegistry
{
public:
  /**
   * @brief QueueRegistry destructor
   */
  ~QueueRegistry() = default;

  /**
   * @brief Get a handle to the QueueRegistry
   * @return QueueRegistry handle
   */
  static QueueRegistry& get();

  /**
   * @brief Get a handle to a Queue
   * @tparam T Type of the data stored in the Queue
   * @param name Name of the Queue
   * @return std::shared_ptr to generic queue pointer
   */
  template<typename T>
  std::shared_ptr<Queue<T>> get_queue(const std::string& name);

  /**
   * @brief Configure the QueueRegistry
   * @param configs Queue configurations
   */
  void configure(const std::vector<const confmodel::Queue*>& configs, opmonlib::OpMonManager &);

  // ONLY TO BE USED FOR TESTING!
  static void reset() { s_instance.reset(nullptr); }
  void shutdown() { m_queue_registry.clear(); }

  bool has_queue(std::string const& uid, std::string const& data_type) const;

  std::set<std::string> get_datatypes(std::string const& uid) const;

private:
  struct QueueEntry
  {
    const confmodel::Queue* m_config;
    const std::type_info* m_type;
    std::shared_ptr<QueueBase> m_instance;
  };

  QueueRegistry() = default;

  template<typename T>
  std::shared_ptr<QueueBase> create_queue(const confmodel::Queue* config);

  std::map<std::string, QueueEntry> m_queue_registry;
  std::vector<const confmodel::Queue*> m_queue_configs;
  std::shared_ptr<opmonlib::OpMonLink> m_opmon_link{ std::make_shared<opmonlib::OpMonLink>() };
  
  bool m_configured{ false };

  static std::unique_ptr<QueueRegistry> s_instance;

  QueueRegistry(const QueueRegistry&) = delete;
  QueueRegistry& operator=(const QueueRegistry&) = delete;
  QueueRegistry(QueueRegistry&&) = delete;
  QueueRegistry& operator=(QueueRegistry&&) = delete;
};

} // namespace iomanager

} // namespace dunedaq

#include "detail/QueueRegistry.hxx"

#endif // IOMANAGER_INCLUDE_IOMANAGER_QUEUEREGISTRY_HPP_
