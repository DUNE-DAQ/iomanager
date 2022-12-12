/**
 * @file QueueBase.hpp
 *
 * This is the base class for Queue objects which connect DAQModules. Queues
 * are exposed to DAQModules via the DAQSource and DAQSink classes, and should
 * not be handled directly. Queues are registered with QueueRegistry for
 * retrieval by DAQSink and DAQSource instances. QueueBase allows to expose common
 * behavior of queues irrespective of their type.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_QUEUEBASE_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_QUEUEBASE_HPP_

#include "iomanager/queueinfo/InfoNljs.hpp"
#include "utilities/NamedObject.hpp"

#include "opmonlib/InfoCollector.hpp"

#include "ers/Issue.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace dunedaq::iomanager {

/**
 * @brief The QueueBase class allows to address generic behavior of any Queue implementation
 *
 */
class QueueBase : public utilities::NamedObject
{
public:
  /**
   * @brief QueueBase Constructor
   * @param name Name of the Queue instance
   */
  explicit QueueBase(const std::string& name)
    : utilities::NamedObject(name)
  {
  }

  /**
   * @brief Method to retrieve information (occupancy) from
   * queues.
   */
  void get_info(opmonlib::InfoCollector& ci, int /*level*/)
  {
    queueinfo::Info info;
    info.capacity = this->get_capacity();
    info.number_of_elements = this->get_num_elements();
    ci.add(info);
  }

  /**
   * @brief Get the capacity (max size) of the queue
   * @return size_t capacity
   */
  virtual size_t get_capacity() const = 0;

  virtual size_t get_num_elements() const = 0;

private:
  QueueBase(const QueueBase&) = delete;
  QueueBase& operator=(const QueueBase&) = delete;
  QueueBase(QueueBase&&) = default;
  QueueBase& operator=(QueueBase&&) = default;
};

} // namespace dunedaq::iomanager

#endif // IOMANAGER_INCLUDE_IOMANAGER_QUEUEBASE_HPP_
