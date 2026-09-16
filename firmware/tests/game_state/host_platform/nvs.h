#pragma once
#include "Preferences.h"
using nvs_handle_t = uint32_t;
using esp_err_t = int;
constexpr int NVS_READONLY = 0;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_FAIL = -1;
constexpr esp_err_t ESP_ERR_NVS_NOT_FOUND = 0x1102;
inline esp_err_t nvs_open(const char* name, int, nvs_handle_t* handle) {
  if (FakeNvs::failOpen || std::strcmp(name, "pokemon_g1") != 0) return ESP_FAIL;
  *handle = 1;
  return ESP_OK;
}
inline esp_err_t nvs_get_blob(nvs_handle_t, const char* key, void* output, size_t* length) {
  if (FakeNvs::failRead) return ESP_FAIL;
  const auto found = FakeNvs::data.find(std::string("pokemon_g1/") + key);
  if (found == FakeNvs::data.end()) return ESP_ERR_NVS_NOT_FOUND;
  if (output) {
    if (*length < found->second.size()) return ESP_FAIL;
    std::memcpy(output, found->second.data(), found->second.size());
  }
  *length = found->second.size();
  return ESP_OK;
}
inline void nvs_close(nvs_handle_t) {}
