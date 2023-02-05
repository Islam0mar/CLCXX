#pragma once

#include <exception>
#include <stdexcept>
#include <type_traits>

namespace scope {
namespace detail {
// for static asserion
template <typename T>
static constexpr bool always_false = false;

// ref: CppCon 2015: Andrei Alexandrescu “Declarative Control Flow"
class UncaughtExceptionCounter {
  const int count_;

 public:
  explicit UncaughtExceptionCounter() : count_(std::uncaught_exceptions()) {}
  bool HasNewUncaughtException() const noexcept {
    return std::uncaught_exceptions() > count_;
  }
};

template <typename FunctionType, bool execute_on_exception>
class ScopeGuardForExceptions {
  const UncaughtExceptionCounter ec_;
  const FunctionType fn_;

 public:
  explicit ScopeGuardForExceptions(const FunctionType& fn) : ec_(), fn_(fn) {}
  explicit ScopeGuardForExceptions(FunctionType&& fn)
      : ec_(), fn_(std::move(fn)) {}
  ~ScopeGuardForExceptions() {
    if constexpr (!std::is_nothrow_invocable_v<FunctionType>) {
      static_assert(always_false<FunctionType>, "function has to be no throw");
    }
    if (execute_on_exception == ec_.HasNewUncaughtException()) {
      fn_();
    }
  }
};

template <typename FunctionType>
class ScopeGuard {
  const FunctionType fn_;

 public:
  explicit ScopeGuard(const FunctionType& fn) : fn_(fn) {}
  explicit ScopeGuard(FunctionType&& fn) : fn_(std::move(fn)) {}
  ~ScopeGuard() {
    if constexpr (!std::is_nothrow_invocable_v<FunctionType>) {
      static_assert(always_false<FunctionType>, "function has to be no throw");
    }
    fn_();
  }
};

enum class ScopeExit;
enum class ScopeFail;
enum class ScopeSuccess;

template <typename FunctionType>
inline auto operator->*(ScopeExit, FunctionType&& fn) {
  return ScopeGuard<FunctionType>(std::forward<FunctionType>(fn));
}

template <typename FunctionType, bool execute_on_exception = true>
inline auto operator->*(ScopeFail, FunctionType&& fn) {
  return ScopeGuardForExceptions<FunctionType, execute_on_exception>(
      std::forward<FunctionType>(fn));
}

template <typename FunctionType, bool execute_on_exception = false>
inline auto operator->*(ScopeSuccess, FunctionType&& fn) {
  return ScopeGuardForExceptions<FunctionType, execute_on_exception>(
      std::forward<FunctionType>(fn));
}

}  // namespace detail
}  // namespace scope

#define SCOPE_COUNTER(x) __LINE__
#define SCOPE__CONCAT(x, y) x##y
#define SCOPE_CONCAT(x, y) SCOPE__CONCAT(x, y)
#define SCOPE_VAR(x) SCOPE_CONCAT(x, SCOPE_COUNTER(x))

#define SCOPE_EXIT                    \
  auto SCOPE_VAR(scope_exit_state_) = \
      scope::detail::ScopeExit()->*[&]() noexcept

#define SCOPE_SUCCESS                    \
  auto SCOPE_VAR(scope_success_state_) = \
      scope::detail::ScopeSuccess->*[&]() noexcept

#define SCOPE_FAIL                    \
  auto SCOPE_VAR(scope_fail_state_) = \
      scope::detail::ScopeFail()->*[&]() noexcept
