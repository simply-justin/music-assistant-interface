#pragma once
// Persistent key/value settings. NVS on the device, in-memory on the host.
#include <map>
#include <string>

namespace app {

class Settings {
public:
    virtual ~Settings() = default;
    virtual std::string get(const std::string& key, const std::string& fallback = "") = 0;
    virtual void set(const std::string& key, const std::string& value) = 0;
    virtual void erase_all() = 0;  // "Reset and set up again"
};

class MemorySettings : public Settings {
public:
    std::string get(const std::string& key, const std::string& fallback = "") override {
        auto it = values_.find(key);
        return it == values_.end() ? fallback : it->second;
    }
    void set(const std::string& key, const std::string& value) override { values_[key] = value; }
    void erase_all() override { values_.clear(); }

private:
    std::map<std::string, std::string> values_;
};

}  // namespace app
