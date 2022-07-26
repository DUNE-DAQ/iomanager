/**
 * @file Queue_test.cxx Queue class Unit Tests
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/Queue.hpp"

#define BOOST_TEST_MODULE Queue_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <memory>
#include <string>
#include <type_traits>

BOOST_AUTO_TEST_SUITE(Queue_test)

using namespace dunedaq::iomanager;

namespace queuetest {
template<class T>
class TestQueue : public Queue<T>
{
public:
  explicit TestQueue(const std::string& name)
    : Queue<T>(name)
  {}

  void push(T&& push, const std::chrono::milliseconds& tmo) override
  {
    if (elem_count == 0) {
      elem = push;
      elem_count = 1;
      return;
    }
    throw QueueTimeoutExpired(ERS_HERE, "TestQueue", "push", tmo.count());
  }
  void pop(T& pop, const std::chrono::milliseconds& tmo) override
  {
    if (elem_count == 1) {
      pop = elem;
      elem_count = 0;
      return;
    }
    throw QueueTimeoutExpired(ERS_HERE, "TestQueue", "pop", tmo.count());
  }
  bool try_push(T&& push, const std::chrono::milliseconds&) override
  {
    if (elem_count == 0) {
      elem = push;
      elem_count = 1;
      return true;
    }
    return false;
  }
  bool try_pop(T& pop, const std::chrono::milliseconds&) override
  {
    if (elem_count == 1) {
      pop = elem;
      elem_count = 0;
      return true;
    }
    return false;
  }
  size_t get_capacity() const override { return 1; }
  size_t get_num_elements() const override { return elem_count; }

private:
  T elem;
  size_t elem_count{ 0 };
};
} // namespace queuetest

BOOST_AUTO_TEST_CASE(QueueOperations)
{

  auto queue_ptr = std::shared_ptr<Queue<int>>(new queuetest::TestQueue<int>("test_queue"));
  BOOST_REQUIRE(queue_ptr != nullptr);

  BOOST_REQUIRE_EQUAL(queue_ptr->get_capacity(), 1);
  BOOST_REQUIRE_EQUAL(queue_ptr->get_num_elements(), 0);
  
  BOOST_REQUIRE(queue_ptr->can_push());
  BOOST_REQUIRE(!queue_ptr->can_pop());
  
  queue_ptr->push(15, std::chrono::milliseconds(1));
  
  BOOST_REQUIRE(!queue_ptr->can_push());
  BOOST_REQUIRE(queue_ptr->can_pop());
  BOOST_REQUIRE_EQUAL(queue_ptr->get_num_elements(), 1);
  
  int pop_value = 0;
  queue_ptr->pop(pop_value, std::chrono::milliseconds(1));
  BOOST_REQUIRE_EQUAL(pop_value, 15);

  BOOST_REQUIRE(queue_ptr->can_push());
  BOOST_REQUIRE(!queue_ptr->can_pop());
  BOOST_REQUIRE_EQUAL(queue_ptr->get_num_elements(), 0);

  BOOST_REQUIRE_EXCEPTION(queue_ptr->pop(pop_value, std::chrono::milliseconds(1)),
                          QueueTimeoutExpired,
                          [](QueueTimeoutExpired const&) { return true; });
  queue_ptr->push(16, std::chrono::milliseconds(1));

  BOOST_REQUIRE(!queue_ptr->can_push());
  BOOST_REQUIRE(queue_ptr->can_pop());

  BOOST_REQUIRE_EXCEPTION(queue_ptr->push(17, std::chrono::milliseconds(1)),
                          QueueTimeoutExpired,
                          [](QueueTimeoutExpired const&) { return true; });
  queue_ptr->pop(pop_value, std::chrono::milliseconds(1));
  BOOST_REQUIRE_EQUAL(pop_value, 16);

  BOOST_REQUIRE(queue_ptr->can_push());
  BOOST_REQUIRE(!queue_ptr->can_pop());
  
  auto ret = queue_ptr->try_pop(pop_value, std::chrono::milliseconds(1));
  BOOST_REQUIRE(ret == false);
  ret = queue_ptr->try_push(18, std::chrono::milliseconds(1));
  
  BOOST_REQUIRE(ret);
  BOOST_REQUIRE(!queue_ptr->can_push());
  BOOST_REQUIRE(queue_ptr->can_pop());
  
  ret = queue_ptr->try_push(19, std::chrono::milliseconds(1));
  BOOST_REQUIRE(ret == false);
  ret = queue_ptr->try_pop(pop_value, std::chrono::milliseconds(1));
  BOOST_REQUIRE_EQUAL(pop_value, 18);
  BOOST_REQUIRE(ret);
  BOOST_REQUIRE(queue_ptr->can_push());
  BOOST_REQUIRE(!queue_ptr->can_pop());


}

BOOST_AUTO_TEST_SUITE_END()
