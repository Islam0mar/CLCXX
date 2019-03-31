
#include <stack>
#include <string>

// #include "clcxx/array.hpp"
#include "clcxx/clcxx.hpp"
#include "clcxx/clcxx_config.hpp"

namespace clcxx {

namespace detail {

char *str_dup(const char *src) {
  try {
    size_t len = strlen(src) + 1;
    if (len > 1) {
      char *s = new char[len];
      memcpy(s, src, len);
      return s;
    }
    return nullptr;
  } catch (std::bad_alloc &err) {
    lisp_error(err.what());
  }
  return nullptr;
}

inline void Free(char *f) {
  if (!(f == nullptr)) {
    delete[] f;
  }
}
template <> void Remove(ClassInfo obj) {
  Free(obj.name);
  Free(obj.super_classes);
  Free(obj.slot_types);
  Free(obj.slot_names);
}
template <> void Remove(FunctionInfo obj) {
  Free(obj.name);
  Free(obj.class_obj);
  Free(obj.arg_types);
  Free(obj.return_type);
}
template <> void Remove(ConstantInfo obj) {
  Free(obj.name);
  Free(obj.value);
}
} // namespace detail

void PackageRegistry::remove_package(std::string lpack) {
  Package &pack = get_package(lpack);
  for (const auto &Class : pack.classes_meta_data()) {
    detail::Remove(Class);
  }
  for (const auto &Constant : pack.constants_meta_data()) {
    detail::Remove(Constant);
  }
  for (const auto &Func : pack.functions_meta_data()) {
    detail::Remove(Func);
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

} // namespace clcxx
