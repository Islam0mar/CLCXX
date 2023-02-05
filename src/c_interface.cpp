
#include <cstdint>
#include <cstring>
#include <string>

#include "clcxx/clcxx.hpp"
#include "clcxx/clcxx_config.hpp"
#include "clcxx/type_conversion.hpp"

extern "C" {

CLCXX_API bool clcxx_init(void (*error_handler)(char *),
                          void (*reg_data_callback)(clcxx::MetaData *,
                                                    uint8_t)) {
  try {
    clcxx::registry().set_error_handler(error_handler);
    clcxx::registry().set_meta_data_handler(reg_data_callback);
    return true;
  } catch (const std::runtime_error &err) {
    clcxx::LispError(const_cast<char *>(err.what()));
  }
  return false;
}

CLCXX_API bool remove_package(const char *pack_name) {
  try {
    clcxx::registry().remove_package(pack_name);
    return true;
  } catch (const std::runtime_error &err) {
    clcxx::LispError(const_cast<char *>(err.what()));
  }
  return false;
}

CLCXX_API bool register_package(const char *cl_pack,
                                void (*regfunc)(clcxx::Package &)) {
  try {
    clcxx::Package &pack = clcxx::registry().create_package(cl_pack);
    regfunc(pack);
    for (auto Class : pack.classes_meta_data()) {
      clcxx::MetaData m;
      m.Class = Class;
      clcxx::registry().send_data(&m, 0);
      clcxx::detail::remove_c_strings(Class);
    }
    pack.classes_meta_data().clear();
    for (auto Constant : pack.constants_meta_data()) {
      clcxx::MetaData m;
      m.Const = Constant;
      clcxx::registry().send_data(&m, 1);
      clcxx::detail::remove_c_strings(Constant);
    }
    pack.constants_meta_data().clear();
    for (auto Func : pack.functions_meta_data()) {
      clcxx::MetaData m;
      m.Func = Func;
      clcxx::registry().send_data(&m, 2);
      clcxx::detail::remove_c_strings(Func);
    }
    pack.functions_meta_data().clear();
    clcxx::registry().reset_current_package();
    return true;
  } catch (const std::runtime_error &err) {
    clcxx::LispError(const_cast<char *>(err.what()));
  }
  return false;
}

CLCXX_API size_t used_bytes_size() {
  return clcxx::MemPool().get_num_of_bytes_allocated();
}

CLCXX_API size_t max_stack_bytes_size() { return clcxx::BUF_SIZE; }

CLCXX_API bool delete_string(char *str) {
  try {
    const auto n = std::strlen(str);
    clcxx::MemPool().deallocate(static_cast<void *>(str),
                                (n + 1) * sizeof(char),
                                std::alignment_of_v<char>);
    return true;
  } catch (const std::runtime_error &err) {
    clcxx::LispError(const_cast<char *>(err.what()));
  }
  return false;
}
}
