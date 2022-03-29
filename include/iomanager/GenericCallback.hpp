/**
 * @file GenericCallback.hpp
 *
 * From: https://stackoverflow.com/questions/66329804/c-generic-callback-implementation
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 *
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_GENERICCALLBACK_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_GENERICCALLBACK_HPP_

#include <any>
#include <vector>
#include <tuple>
#include <utility>
#include <memory>

namespace dunedaq {
namespace iomanager {

struct Caller
{
  virtual ~Caller() = default;
  virtual std::any call(const std::vector<std::any>& args) = 0;
};

template<typename R, typename... A>
struct Caller_ : Caller
{

  template<size_t... Is>
  auto make_tuple_impl(const std::vector<std::any>& anyArgs, std::index_sequence<Is...>)
  {
    return std::make_tuple(std::any_cast<std::decay_t<decltype(std::get<Is>(args))>>(anyArgs.at(Is))...);
  }

  template<size_t N>
  auto make_tuple(const std::vector<std::any>& anyArgs)
  {
    return make_tuple_impl(anyArgs, std::make_index_sequence<N>{});
  }

  std::any call(const std::vector<std::any>& anyArgs) override
  {
    args = make_tuple<sizeof...(A)>(anyArgs);
    ret = std::apply(func, args);
    return { ret };
  };

  explicit Caller_(std::function<R(A...)>& func_)
    : func(func_)
  {}

  std::function<R(A...)>& func;
  std::tuple<A...> args;
  R ret;
};

struct GenericCallback
{

  template<class R, class... A>
  GenericCallback& operator=(std::function<R(A...)>&& func_)
  {
    func = std::move(func_);
    caller = std::make_unique<Caller_<R, A...>>(std::any_cast<std::function<R(A...)>&>(func));
    return *this;
  }

  template<class Func>
  GenericCallback& operator=(Func&& func_)
  {
    return *this = std::function(std::forward<Func>(func_));
  }

  std::any callAny(const std::vector<std::any>& args) { return caller->call(args); }

  template<class R, class... Args>
  R call(Args&&... args)
  {
    auto& f = std::any_cast<std::function<R(Args...)>&>(func);
    return f(std::forward<Args>(args)...);
  }

  std::any func;
  std::unique_ptr<Caller> caller;
};

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_GENERICCALLBACK_HPP_
