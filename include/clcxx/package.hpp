#pragma once

#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

// #include "array.hpp"
#include "type_conversion.hpp"

namespace clcxx {

extern "C" typedef struct {
  char *name;
  char *super_classes;
  char *slot_names;
  char *slot_types;
  void *finalize;
} ClassInfo;

extern "C" typedef struct {
  char *name;
  bool method_p;
  char *class_obj;
  void *thunk_func;
  size_t index;
  char *arg_types;
  char *return_type;
} FunctionInfo;

extern "C" typedef struct {
  char *name;
  char *value;
} ConstantInfo;

extern "C" typedef union {
  FunctionInfo Func;
  ClassInfo Class;
  ConstantInfo Const;
} MetaData;

class CLCXX_API Package;

namespace detail {

char *str_dup(const char *src);

template <typename T> void Remove(T obj);

template <typename T> void finalize_class(void *ptr) {
  auto obj_ptr = reinterpret_cast<T *>(ptr);
  delete obj_ptr;
}
// Need to treat void specially
template <typename R, typename... Args> struct ReturnTypeAdapter {
  using return_type = decltype(convert_to_lisp(std::declval<R>()));
  inline return_type operator()(const void *functor,
                                mapped_lisp_type<Args>... args) {
    auto std_func =
        reinterpret_cast<const std::function<R(Args...)> *>(functor);
    assert(std_func != nullptr);
    return convert_to_lisp(
        (*std_func)(convert_to_cpp<mapped_reference_type<Args>>(args)...));
  }
};

template <typename... Args> struct ReturnTypeAdapter<void, Args...> {
  inline void operator()(const void *functor, mapped_lisp_type<Args>... args) {
    auto std_func =
        reinterpret_cast<const std::function<void(Args...)> *>(functor);
    assert(std_func != nullptr);
    (*std_func)(convert_to_cpp<mapped_reference_type<Args>>(args)...);
  }
};

/// Make a string with the types in the variadic template parameter pack
template <typename... Args> std::string arg_types_string() {
  std::vector<std::string> vec = {
      lisp_type<dereference_for_mapping<Args>>()...};
  std::string s;
  for (auto arg_types : vec) {
    s.append(arg_types);
    s.append("+");
  }
  return s;
}

template <typename... Args> struct NeedConvertHelper {
  bool operator()() {
    for (const bool b : {std::is_same<remove_const_ref<mapped_lisp_type<Args>>,
                                      remove_const_ref<Args>>::value...}) {
      if (!b)
        return true;
    }
    return false;
  }
};

template <> struct NeedConvertHelper<> {
  bool operator()() { return false; }
};

} // namespace detail

/// Registry containing different packages
class CLCXX_API PackageRegistry {
public:
  /// Create a package and register it
  Package &create_package(std::string lpack);

  Package &get_package(std::string pack) const {
    const auto iter = p_packages.find(pack);
    if (iter == p_packages.end()) {
      throw std::runtime_error("Pack with name " + pack +
                               " was not found in registry");
    }

    return *(iter->second);
  }

  bool has_package(std::string lpack) const {
    return p_packages.find(lpack) != p_packages.end();
  }

  void remove_package(std::string lpack);

  bool has_current_package() { return p_current_package != nullptr; }
  Package &current_package();
  void reset_current_package() { p_current_package = nullptr; }
  std::vector<std::shared_ptr<const void *>> functions();

  ~PackageRegistry() {
    for (auto const &p : p_packages) {
      remove_package(p.first);
    }
  }

  PackageRegistry()
      : p_error_handler_callback(nullptr),
        p_meta_data_handler_callback(nullptr) {}

  void set_error_handler(void (*callback)(char *)) {
    p_error_handler_callback = callback;
  }

  void handle_error(char *err_msg) { p_error_handler_callback(err_msg); }

  void set_meta_data_handler(void (*callback)(MetaData *, uint8_t)) {
    p_meta_data_handler_callback = callback;
  }

  void send_data(MetaData *M, uint8_t n) {
    p_meta_data_handler_callback(M,n);
  }

private:
  std::map<std::string, std::shared_ptr<Package>> p_packages;
  Package *p_current_package = nullptr;
  void (*p_error_handler_callback)(char *);
  void (*p_meta_data_handler_callback)(MetaData *, uint8_t);
};

CLCXX_API PackageRegistry &registry();

inline void lisp_error(const char *error) {
  clcxx::registry().handle_error(const_cast<char *>(error));
}

namespace detail {
template <typename R, typename... Args> struct CallFunctor {
  using return_type = decltype(ReturnTypeAdapter<R, Args...>()(
      std::declval<const void *>(), std::declval<mapped_lisp_type<Args>>()...));

  static return_type apply(size_t index, mapped_lisp_type<Args>... args) {
    try {
      const void *f = *registry().functions().at(index);
      return ReturnTypeAdapter<R, Args...>()(f, args...);
    } catch (const std::exception &err) {
      lisp_error(err.what());
    }
    return return_type();
  }
};
} // namespace detail

template <typename T> class ClassWrapper;

/// Store all exposed C++ functions associated with a package
class CLCXX_API Package {
public:
  explicit Package(std::string cl_pack) : p_cl_pack(cl_pack) {}

  /// Define a new function
  template <typename R, typename... Args>
  inline void defun(const std::string &name,
                    std::function<R(Args...)> functor) {
    FunctionInfo f_info;
    f_info.name = detail::str_dup(name.c_str());
    f_info.method_p = false;
    f_info.class_obj = nullptr;
    f_info.thunk_func =
        reinterpret_cast<void *>(detail::CallFunctor<R, Args...>::apply);
    f_info.index = p_functions.size();
    f_info.arg_types =
        detail::str_dup(detail::arg_types_string<Args...>().c_str());
    f_info.return_type =
        detail::str_dup(lisp_type<dereference_for_mapping<R>>().c_str());
    p_functions_meta_data.push_back(f_info);
    auto f_ptr = std::make_shared<const void *>(
        new const std::function<R(Args...)>(functor));
    p_functions.push_back(f_ptr);
  }

  /// Define a new function. Overload for pointers
  template <typename R, typename... Args>
  inline void defun(const std::string &name, R (*f)(Args...)) {
    defun(name, std::function<R(Args...)>(f));
  }

  /// Define a new function. Overload for lambda
  template <typename LambdaT>
  void defun(const std::string &name, LambdaT &&lambda) {
    add_lambda(name, std::forward<LambdaT>(lambda), &LambdaT::operator());
  }

  /// Add a composite type
  template <typename T, typename... s_classes>
  ClassWrapper<T> defclass(const std::string &name, s_classes... super) {
    ClassInfo c_info;
    c_info.finalize = reinterpret_cast<void *>(detail::finalize_class<T>);
    c_info.slot_types = nullptr;
    c_info.slot_names = nullptr;
    // store class name
    c_info.name = detail::str_dup(name.c_str());
    // store class type
    class_name[std::type_index(typeid(T))] = name;
    // get super classes names
    c_info.super_classes =
        detail::str_dup(super_classes_string<s_classes...>().c_str());
    p_classes_meta_data.push_back(c_info);
    return ClassWrapper<T>(*this);
  }

  /// Set a global constant value at the package level
  template <typename T> void defconstant(const std::string &name, T &&value) {
    ConstantInfo const_info;
    const_info.name = detail::str_dup(name.c_str());
    const_info.value = detail::str_dup(std::to_string(value).c_str());
    p_constants.push_back(const_info);
  }

  std::string name() const { return p_cl_pack; }
  std::vector<std::shared_ptr<const void *>> functions() { return p_functions; }
  std::unordered_map<std::type_index, std::string> classes() {
    return class_name;
  }
  std::vector<ClassInfo> &classes_meta_data() { return p_classes_meta_data; }
  std::vector<FunctionInfo> &functions_meta_data() {
    return p_functions_meta_data;
  }
  std::vector<ConstantInfo> &constants_meta_data() { return p_constants; }

private:
  template <typename R, typename LambdaT, typename... ArgsT>
  void add_lambda(const std::string &name, LambdaT &&lambda,
                  R (LambdaT::*)(ArgsT...) const) {
    return defun(name,
                 std::function<R(ArgsT...)>(std::forward<LambdaT>(lambda)));
  }

  /// Make a string with the super classes in the variadic template parameter
  /// pack
  template <typename... Args> std::string super_classes_string() {
    std::vector<std::string> vec = {
        class_name[std::type_index(typeid(Args))]...};
    std::string s;
    for (auto super_types : vec) {
      s.append(super_types);
      s.append("+");
    }
    return s;
  }

  std::string p_cl_pack;
  std::vector<std::shared_ptr<const void *>> p_functions;
  std::vector<ClassInfo> p_classes_meta_data;
  std::vector<FunctionInfo> p_functions_meta_data;
  std::vector<ConstantInfo> p_constants;
  std::unordered_map<std::type_index, std::string> class_name;
  template <class T> friend class ClassWrapper;
};

// Helper class to wrap type methods
template <typename T> class ClassWrapper {
public:
  typedef T type;
  // TODO(is): add constructor
  // /// Add a constructor with the given argument types
  // template <typename... ArgsT>
  // ClassWrapper<T> &constructor(bool finalize = true) {
  //   p_package.constructor<T, ArgsT...>(m_dt, finalize);
  //   return *this;
  // }
  explicit ClassWrapper(Package &pack) : p_package(pack) {}

  /// Define a member function
  template <typename R, typename CT, typename... ArgsT>
  ClassWrapper<T> &method(const std::string &name, R (CT::*f)(ArgsT...)) {
    auto curr_class = p_package.p_classes_meta_data.back();
    auto functor = [f](T &obj, ArgsT... args) -> R {
      return (obj.*f)(args...);
    };
    FunctionInfo f_info;
    f_info.name = detail::str_dup(name.c_str());
    f_info.method_p = true;
    f_info.class_obj = detail::str_dup(curr_class.name);
    f_info.thunk_func =
        reinterpret_cast<void *>(detail::CallFunctor<R, ArgsT...>::apply);
    f_info.index = p_package.p_functions.size();
    f_info.arg_types =
        detail::str_dup(detail::arg_types_string<ArgsT...>().c_str());
    f_info.return_type =
        detail::str_dup(lisp_type<dereference_for_mapping<R>>().c_str());
    p_package.p_functions_meta_data.push_back(f_info);
    auto f_ptr = std::make_shared<const void *>(
        new const std::function<R(T &, ArgsT...)>(functor));
    p_package.p_functions.push_back(f_ptr);
    return *this;
  }

  /// Define a member function, const version
  template <typename R, typename CT, typename... ArgsT>
  ClassWrapper<T> &method(const std::string &name, R (CT::*f)(ArgsT...) const) {
    auto functor = [f](const T &obj, ArgsT... args) -> R {
      return (obj.*f)(args...);
    };
    auto curr_class = p_package.p_classes_meta_data.back();
    FunctionInfo f_info;
    f_info.name = detail::str_dup(name.c_str());
    f_info.method_p = true;
    f_info.class_obj = detail::str_dup(curr_class.name);
    f_info.thunk_func =
        reinterpret_cast<void *>(detail::CallFunctor<R, ArgsT...>::apply);
    f_info.index = p_package.p_functions.size();
    f_info.arg_types =
        detail::str_dup(detail::arg_types_string<ArgsT...>().c_str());
    f_info.return_type =
        detail::str_dup(lisp_type<dereference_for_mapping<R>>().c_str());
    auto f_ptr = std::make_shared<const void *>(
        new const std::function<R(T &, ArgsT...)>(functor));
    p_package.p_functions.push_back(f_ptr);
    return *this;
  }

  /// Define a "member" function using a lambda
  template <typename LambdaT>
  ClassWrapper<T> &method(const std::string &name, LambdaT &&lambda) {
    return method(name, std::forward<LambdaT>(lambda));
  }

  // TODO(is): add member
  // template <typename LambdaT>
  // ClassWrapper<T> &member(const std::string &name, LambdaT &&lambda) {
  //   return method(name, std::forward<LambdaT>(lambda));
  // }

  // Access to the module
  Package &package() { return p_package; }

private:
  Package &p_package;
};

} // namespace clcxx

#define CLCXX_PACKAGE extern "C" CLCXX_ONLY_EXPORTS void
