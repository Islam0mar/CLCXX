
#include <stack>
#include <string>

// #include "clcxx/array.hpp"
#include "clcxx/clcxx.hpp"
#include "clcxx/clcxx_config.hpp"

namespace clcxx {

namespace detail {

char *str_dup(const char *src) {
  try {
    if (src == nullptr) {
      return nullptr;
    }
    size_t len = strlen(src) + 1;
    if (len > 1) {
      char *new_str = new char[len];
      memcpy(new_str, src, len);
      return new_str;
    }
    return nullptr;
  } catch (std::bad_alloc &err) {
    lisp_error(err.what());
  }
  return nullptr; 
}

char *str_append(char *old_str, const char *src) {
  try {
    if (old_str == nullptr) {
      return str_dup(src);
    }
    if (src == nullptr) {
      return old_str;
    }
    size_t len = strlen(src) + 1;
    if (len > 1) {
      size_t len_old = strlen(old_str);
      char *new_str = new char[len + len_old];
      memcpy(new_str, old_str, len_old);
      delete[] old_str;
      strcat(new_str, src);
      return new_str;
    }
    return old_str;
  } catch (std::bad_alloc &err) {
    lisp_error(err.what());
  }
  return nullptr;
}

inline void delete_char_array(char *f) {
  if (!(f == nullptr)) {
    delete[] f;
  }
}
template <>
void remove_c_strings(ClassInfo obj) {
  delete_char_array(obj.name);
  delete_char_array(obj.super_classes);
  delete_char_array(obj.slot_types);
  delete_char_array(obj.slot_names);
}
template <>
void remove_c_strings(FunctionInfo obj) {
  delete_char_array(obj.name);
  delete_char_array(obj.class_obj);
  delete_char_array(obj.arg_types);
  delete_char_array(obj.return_type);
}
template <>
void remove_c_strings(ConstantInfo obj) {
  delete_char_array(obj.name);
  delete_char_array(obj.value);
}
}  // namespace detail

void PackageRegistry::remove_package(std::string lpack) {
  Package &pack = get_package(lpack);
  for (const auto &Class : pack.classes_meta_data()) {
    detail::remove_c_strings(Class);
  }
  for (const auto &Constant : pack.constants_meta_data()) {
    detail::remove_c_strings(Constant);
  }
  for (const auto &Func : pack.functions_meta_data()) {
    detail::remove_c_strings(Func);
  }
  p_packages.erase(lpack);
}

std::string class_name(std::type_index i) {
  return registry().current_package().classes()[i];
}

std::vector<std::shared_ptr<const void *>> PackageRegistry::functions() {
  return PackageRegistry::current_package().functions();
}

Package &PackageRegistry::create_package(std::string pack_name) {
  if (has_package(pack_name)) {
    throw std::runtime_error("Error registering module: " + pack_name +
                             " was already registered");
  }
  p_current_package = new Package(pack_name);
  p_packages[pack_name].reset(p_current_package);
  return *p_current_package;
}

Package &PackageRegistry::current_package() {
  assert(p_current_package != nullptr);
  return *p_current_package;
}

CLCXX_API PackageRegistry &registry() {
  static PackageRegistry p_registry;
  return p_registry;
}

}  // namespace clcxx
