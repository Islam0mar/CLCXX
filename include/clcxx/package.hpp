#pragma once

#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

#include "type_conversion.hpp"

namespace clcxx {

extern "C" typedef struct {
  char *name;
  char *super_classes;
  char *slot_names;
  char *slot_types;
  void *constructor;
  void *destructor;
} ClassInfo;

extern "C" typedef struct {
  char *name;
  bool method_p;
  char *class_obj;
  const void *thunk_ptr;
  const void *func_ptr;
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

inline void lisp_error(const char *error);

namespace detail {

char *str_dup(const char *src);
char *str_append(char *old_str, const char *src);

template <typename T>
void remove_c_strings(T obj);

// Base class to specialize for constructor
template <typename CppT, typename... Args>
void *CppConstructor(Args... args) {
  CppT *obj_ptr = new CppT(args...);
  void *ptr = reinterpret_cast<void *>(obj_ptr);
  return ptr;
}

template <typename T, bool Constructor = true, typename... Args>
struct CreateClass {
  inline void *operator()() {
    return reinterpret_cast<void *>(CppConstructor<T, Args...>);
  }
};

template <typename T, typename... Args>
struct CreateClass<T, false, Args...> {
  inline void *operator()() { return nullptr; }
};

template <typename T>
void remove_class(void *ptr) {
  auto obj_ptr = reinterpret_cast<T *>(ptr);
  delete obj_ptr;
}

// Need to treat void specially
template <typename R, typename... Args>
struct ReturnTypeAdapter {
  using return_type = decltype(convert_to_lisp(std::declval<R>()));
  inline return_type operator()(const void *functor,
                                mapped_lisp_type<Args>... args) {
    auto std_func =
        reinterpret_cast<const std::function<R(Args...)> *>(functor);
    assert(std_func != nullptr);
    return convert_to_lisp((*std_func)(convert_to_cpp<Args>(args)...));
  }
};

template <typename... Args>
struct ReturnTypeAdapter<void, Args...> {
  inline void operator()(const void *functor, mapped_lisp_type<Args>... args) {
    auto std_func =
        reinterpret_cast<const std::function<void(Args...)> *>(functor);
    assert(std_func != nullptr);
    (*std_func)(convert_to_cpp<Args>(args)...);
  }
};

/// Make a string with the types in the variadic template parameter pack
template <typename... Args>
std::string arg_types_string() {
  std::vector<std::string> vec = {lisp_type<Args>()...};
  std::string s;
  for (auto arg_types : vec) {
    s.append(arg_types);
    s.append("+");
  }
  return s;
}

/// Make a string with the super classes in the variadic template parameter
/// pack
template <typename... Args>
std::string super_classes_string() {
  std::vector<std::string> vec = {class_name(std::type_index(typeid(Args)))...};
  std::string s;
  for (auto super_types : vec) {
    s.append(super_types);
    s.append("+");
  }
  return s;
}

}  // namespace detail

/// Abstract base class for storing any function
class CLCXX_API FunctionWrapperBase {};

/// Implementation of function storage, case of std::function
template <typename R, typename... Args>
class FunctionWrapper : public FunctionWrapperBase {
 public:
  typedef std::function<R(Args...)> functor_t;

  explicit FunctionWrapper(const functor_t &f) { p_function = f; }

  const void *ptr() { return reinterpret_cast<const void *>(&p_function); }

 private:
  functor_t p_function;
};

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
  auto functions();

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

  void send_data(MetaData *M, uint8_t n) { p_meta_data_handler_callback(M, n); }

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
template <typename R, typename... Args>
struct CallFunctor {
  using return_type = decltype(ReturnTypeAdapter<R, Args...>()(
      std::declval<const void *>(), std::declval<mapped_lisp_type<Args>>()...));

  static return_type apply(const void *functor,
                           mapped_lisp_type<Args>... args) {
    try {
      return ReturnTypeAdapter<R, Args...>()(functor, args...);
    } catch (const std::exception &err) {
      lisp_error(err.what());
    }
    return return_type();
  }
};
}  // namespace detail

template <typename T>
class ClassWrapper;

/// Store all exposed C++ functions associated with a package
class CLCXX_API Package {
 public:
  explicit Package(std::string cl_pack) : p_cl_pack(cl_pack) {}

  /// Define a new function
  template <typename R, typename... Args>
  void defun(const std::string &name, std::function<R(Args...)> f,
             bool is_method = false, const char *class_name = "") {
    FunctionInfo f_info;
    f_info.name = detail::str_dup(name.c_str());
    f_info.method_p = is_method;
    f_info.class_obj = detail::str_dup(class_name);
    f_info.thunk_ptr =
        reinterpret_cast<const void *>(detail::CallFunctor<R, Args...>::apply);
    f_info.arg_types =
        detail::str_dup(detail::arg_types_string<Args...>().c_str());
    f_info.return_type = detail::str_dup(lisp_type<R>().c_str());
    auto new_wrapper = new FunctionWrapper<R, Args...>(f);
    p_functions.push_back(std::shared_ptr<FunctionWrapperBase>(new_wrapper));
    f_info.func_ptr = new_wrapper->ptr();
    // store data
    p_functions_meta_data.push_back(f_info);
  }

  /// Define a new function. Overload for pointers
  template <typename R, typename... Args>
  void defun(const std::string &name, R (*f)(Args...), bool is_method = false,
             const char *class_name = "") {
    defun(name, std::function<R(Args...)>(f), is_method, class_name);
  }

  /// Define a new function. Overload for lambda
  template <typename LambdaT>
  void defun(const std::string &name, LambdaT &&lambda, bool is_method = false,
             const char *class_name = "") {
    add_lambda(name, std::forward<LambdaT>(lambda), &LambdaT::operator(),
               is_method, class_name);
  }

  /// Add a class type
  template <typename T, bool Constructor = true, typename... s_classes>
  ClassWrapper<T> defclass(const std::string &name, s_classes... super) {
    ClassInfo c_info;
    c_info.constructor = detail::CreateClass<T, Constructor>()();
    c_info.destructor = reinterpret_cast<void *>(detail::remove_class<T>);
    c_info.slot_types = nullptr;
    c_info.slot_names = nullptr;
    c_info.name = detail::str_dup(name.c_str());
    class_name[std::type_index(typeid(T))] = name;
    c_info.super_classes =
        detail::str_dup(detail::super_classes_string<s_classes...>().c_str());
    // Store data
    p_classes_meta_data.push_back(c_info);
    return ClassWrapper<T>(*this);
  }

  /// Set a global constant value at the package level
  template <typename T>
  void defconstant(const std::string &name, T &&value) {
    ConstantInfo const_info;
    const_info.name = detail::str_dup(name.c_str());
    const_info.value = detail::str_dup(std::to_string(value).c_str());
    p_constants.push_back(const_info);
  }

  std::string name() const { return p_cl_pack; }
  std::vector<std::shared_ptr<FunctionWrapperBase>> functions() {
    return p_functions;
  }
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
                  R (LambdaT::*)(ArgsT...) const, bool is_method = false,
                  const char *class_name = "") {
    return defun(name,
                 std::function<R(ArgsT...)>(std::forward<LambdaT>(lambda)),
                 is_method, class_name);
  }

  std::string p_cl_pack;
  std::vector<std::shared_ptr<FunctionWrapperBase>> p_functions;
  std::vector<ClassInfo> p_classes_meta_data;
  std::vector<FunctionInfo> p_functions_meta_data;
  std::vector<ConstantInfo> p_constants;
  std::unordered_map<std::type_index, std::string> class_name;
  template <class T>
  friend class ClassWrapper;
};

// Helper class to wrap type methods
template <typename T>
class ClassWrapper {
 public:
  typedef T type;

  explicit ClassWrapper(Package &pack) : p_package(pack) {}

  /// Add a constructor with the given argument types
  template <typename... Args>
  ClassWrapper<T> &constructor() {
    auto &curr_class = p_package.p_classes_meta_data.back();
    // Use name as a flag
    p_package.defun(std::string("create-" + std::string(curr_class.name) +
                                std::to_string(sizeof...(Args))),
                    detail::CppConstructor<T, Args...>, false, curr_class.name);
    return *this;
  }

  /// Define a member function
  template <typename R, typename CT, typename... ArgsT>
  ClassWrapper<T> &defmethod(const std::string &name, R (CT::*f)(ArgsT...)) {
    auto curr_class = p_package.p_classes_meta_data.back();
    p_package.defun(
        name, [f](T &obj, ArgsT... args) -> R { return (obj.*f)(args...); },
        true, curr_class.name);
    return *this;
  }

  /// Define a member function, const version
  template <typename R, typename CT, typename... ArgsT>
  ClassWrapper<T> &defmethod(const std::string &name,
                             R (CT::*f)(ArgsT...) const) {
    auto curr_class = p_package.p_classes_meta_data.back();
    p_package.defun(
        name,
        [f](const T &obj, ArgsT... args) -> R { return (obj.*f)(args...); },
        true, curr_class.name);
    return *this;
  }

  /// Define a "member" function using a lambda
  template <typename LambdaT>
  ClassWrapper<T> &defmethod(const std::string &name, LambdaT &&lambda) {
    return defmethod(name, std::forward<LambdaT>(lambda));
  }

  // Add public member >> readwrite
  template <typename CT, typename MemberT>
  ClassWrapper<T> &member(const std::string &name, MemberT CT::*pm) {
    static_assert(std::is_base_of<CT, T>::value,
                  "member() requires a class member (or base class member)");
    auto &curr_class = p_package.p_classes_meta_data.back();
    p_package.defun(std::string(name + ".get"),
                    [pm](const T &c) -> const MemberT & { return c.*pm; }, true,
                    curr_class.name);
    p_package.defun(std::string(name + ".set"),
                    [pm](T &c, const MemberT &value) { c.*pm = value; }, true,
                    curr_class.name);
    curr_class.slot_names = detail::str_append(curr_class.slot_names,
                                               std::string(name + "+").c_str());
    curr_class.slot_types = detail::str_append(
        curr_class.slot_types, std::string(lisp_type<MemberT>() + "+").c_str());
    return *this;
  }

  // Access to the module
  Package &package() { return p_package; }

 private:
  Package &p_package;
};

}  // namespace clcxx
extern "C" {
CLCXX_API bool clcxx_init(void (*error_handler)(char *),
                          void (*reg_data_callback)(clcxx::MetaData *,
                                                    uint8_t));
CLCXX_API bool remove_package(char *pack_name);
CLCXX_API bool register_package(const char *cl_pack,
                                void (*regfunc)(clcxx::Package &));
}

#define CLCXX_PACKAGE extern "C" CLCXX_ONLY_EXPORTS void
