#pragma once
#include <memory>
#include <string>

#include "cJSON.h"

namespace backend {

struct JsonDeleter {
    void operator()(cJSON* p) const { cJSON_Delete(p); }
};
using Json = std::unique_ptr<cJSON, JsonDeleter>;

inline std::string json_str(const cJSON* obj, const char* key) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(v) ? v->valuestring : "";
}
inline double json_num(const cJSON* obj, const char* key, double def = 0) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(v) ? v->valuedouble : def;
}
inline bool json_bool(const cJSON* obj, const char* key, bool def = false) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsBool(v) ? cJSON_IsTrue(v) : def;
}

}  // namespace backend
