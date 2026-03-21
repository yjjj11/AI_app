#include "ai/LLMFactory.h"

#include "ai/BailianLLM.h"

#include <fstream>
#include <cctype>
#include <mutex>
#include <unordered_map>

#include <nlohmann/json.hpp>

static std::mutex g_llm_mutex;
static std::unordered_map<std::string, std::shared_ptr<LLM>> g_llm_instances;
static std::unordered_map<std::string, LLMFactory::Profile> g_profiles;
static std::string g_active_key;
static std::string g_default_key;

static std::string toLower(std::string s) {
    for (auto& ch : s) ch = (char)std::tolower((unsigned char)ch);
    return s;
}

LLMFactory::Profile::Capabilities LLMFactory::inferCapabilitiesFromModel(const std::string& model) {
    Profile::Capabilities caps;
    const std::string m = toLower(model);
    if (m.find("vl") != std::string::npos) caps.vision = true;
    if (m.find("vision") != std::string::npos) caps.vision = true;
    if (m.find("gpt-4o") != std::string::npos) caps.vision = true;
    if (m.find("gpt-4.1") != std::string::npos) caps.vision = true;

    if (m.find("audio") != std::string::npos) caps.audio = true;
    if (m.find("omni") != std::string::npos) caps.audio = true;
    return caps;
}

static LLMFactory::Profile::Capabilities inferCapabilities(const LLMFactory::Profile& p) {
    return LLMFactory::inferCapabilitiesFromModel(p.default_model);
}

static std::string resolveKeyLocked(const std::string& key) {
    if (key == "active") {
        std::string k = g_active_key.empty() ? g_default_key : g_active_key;
        auto it = g_profiles.find(k);
        if (it != g_profiles.end()) {
            const std::string& u = it->second.base_url;
            const bool is_local = (u.find("127.0.0.1") != std::string::npos) || (u.find("localhost") != std::string::npos);
            if (is_local) {
                for (const auto& kv : g_profiles) {
                    if (kv.second.base_url.find("dashscope.aliyuncs.com") != std::string::npos) return kv.first;
                }
                for (const auto& kv : g_profiles) {
                    const std::string& uu = kv.second.base_url;
                    const bool local2 = (uu.find("127.0.0.1") != std::string::npos) || (uu.find("localhost") != std::string::npos);
                    if (!local2) return kv.first;
                }
            }
        }
        return k;
    }
    if (key.empty() || key == "default") {
        return g_default_key;
    }
    return key;
}

static std::unique_ptr<LLM> createByKeyLocked(const std::string& key) {
    const std::string k = resolveKeyLocked(key);
    if (k.empty()) return nullptr;

    auto it = g_profiles.find(k);
    if (it == g_profiles.end()) return nullptr;
    const auto& p = it->second;

    std::string url = p.base_url;
    if (url.empty()) return nullptr;
    const int timeout = p.timeout_seconds > 0 ? p.timeout_seconds : 30;
    return std::make_unique<BailianLLM>(p.api_key, url, p.default_model, timeout);
}

std::unique_ptr<LLM> LLMFactory::create() {
    std::lock_guard<std::mutex> lock(g_llm_mutex);
    return createByKeyLocked("default");
}

std::shared_ptr<LLM> LLMFactory::get() {
    return get("active");
}

std::shared_ptr<LLM> LLMFactory::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(g_llm_mutex);
    const std::string k = key.empty() ? "default" : key;
    auto it = g_llm_instances.find(k);
    if (it != g_llm_instances.end()) return it->second;

    {
        auto u = createByKeyLocked(k);
        if (!u) return nullptr;
        auto sp = std::shared_ptr<LLM>(std::move(u));
        g_llm_instances.emplace(k, sp);
        return sp;
    }
}

void LLMFactory::reset() {
    std::lock_guard<std::mutex> lock(g_llm_mutex);
    g_llm_instances.clear();
}

void LLMFactory::reset(const std::string& key) {
    std::lock_guard<std::mutex> lock(g_llm_mutex);
    if (key.empty() || key == "default") {
        g_llm_instances.erase("default");
        return;
    }
    g_llm_instances.erase(key);
}

bool LLMFactory::upsertProfile(const Profile& profile) {
    if (profile.key.empty()) return false;
    std::lock_guard<std::mutex> lock(g_llm_mutex);
    g_profiles[profile.key] = profile;
    g_llm_instances.erase(profile.key);
    if (g_default_key.empty() || profile.key == "default") {
        g_default_key = profile.key;
    }
    if (g_active_key.empty()) {
        g_active_key = g_default_key;
    }
    return true;
}

std::vector<LLMFactory::Profile> LLMFactory::listProfiles() {
    std::lock_guard<std::mutex> lock(g_llm_mutex);
    std::vector<Profile> out;
    out.reserve(g_profiles.size());
    auto it_default = g_profiles.find("default");
    if (it_default != g_profiles.end()) out.push_back(it_default->second);
    for (const auto& kv : g_profiles) {
        if (kv.first == "default") continue;
        out.push_back(kv.second);
    }
    return out;
}

bool LLMFactory::loadProfilesFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;
    nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
    if (j.is_discarded()) return false;

    std::lock_guard<std::mutex> lock(g_llm_mutex);
    g_llm_instances.clear();
    g_profiles.clear();
    g_active_key.clear();
    g_default_key.clear();

    if (j.contains("active_key") && j["active_key"].is_string()) {
        g_active_key = j["active_key"].get<std::string>();
    }
    if (j.contains("profiles") && j["profiles"].is_array()) {
        std::string first_key;
        for (const auto& p : j["profiles"]) {
            if (!p.is_object()) continue;
            Profile prof;
            if (p.contains("key") && p["key"].is_string()) prof.key = p["key"].get<std::string>();
            if (prof.key.empty()) continue;
            if (p.contains("provider") && p["provider"].is_string()) prof.provider = p["provider"].get<std::string>();
            if (p.contains("api_key") && p["api_key"].is_string()) prof.api_key = p["api_key"].get<std::string>();
            if (p.contains("base_url") && p["base_url"].is_string()) prof.base_url = p["base_url"].get<std::string>();
            if (p.contains("default_model") && p["default_model"].is_string()) prof.default_model = p["default_model"].get<std::string>();
            if (p.contains("timeout_seconds") && p["timeout_seconds"].is_number_integer()) prof.timeout_seconds = p["timeout_seconds"].get<int>();

            if (p.contains("capabilities") && p["capabilities"].is_object()) {
                const auto& c = p["capabilities"];
                if (c.contains("vision") && c["vision"].is_boolean()) prof.capabilities.vision = c["vision"].get<bool>();
                if (c.contains("audio") && c["audio"].is_boolean()) prof.capabilities.audio = c["audio"].get<bool>();
                if (c.contains("files") && c["files"].is_boolean()) prof.capabilities.files = c["files"].get<bool>();
            } else {
                prof.capabilities = inferCapabilities(prof);
            }

            g_profiles[prof.key] = prof;
            if (first_key.empty()) first_key = prof.key;
        }
        if (g_profiles.find("default") != g_profiles.end()) {
            g_default_key = "default";
        } else {
            g_default_key = first_key;
        }
    }
    if (!g_active_key.empty() && g_profiles.find(g_active_key) == g_profiles.end()) {
        g_active_key.clear();
    }
    if (g_active_key.empty()) {
        g_active_key = g_default_key;
    }
    return true;
}

bool LLMFactory::saveProfilesToFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_llm_mutex);
    nlohmann::json j;
    j["active_key"] = g_active_key;
    j["profiles"] = nlohmann::json::array();
    auto dumpProfile = [&](const Profile& p) {
        nlohmann::json pj;
        pj["key"] = p.key;
        pj["provider"] = p.provider;
        pj["api_key"] = p.api_key;
        pj["base_url"] = p.base_url;
        pj["default_model"] = p.default_model;
        pj["timeout_seconds"] = p.timeout_seconds;
        pj["capabilities"] = {
            {"vision", p.capabilities.vision},
            {"audio", p.capabilities.audio},
            {"files", p.capabilities.files}
        };
        j["profiles"].push_back(std::move(pj));
    };

    auto it_default = g_profiles.find("default");
    if (it_default != g_profiles.end()) dumpProfile(it_default->second);
    for (const auto& kv : g_profiles) {
        if (kv.first == "default") continue;
        dumpProfile(kv.second);
    }
    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << j.dump(2);
    return true;
}

void LLMFactory::setActiveKey(const std::string& key) {
    std::lock_guard<std::mutex> lock(g_llm_mutex);
    g_active_key = key;
}

std::string LLMFactory::getActiveKey() {
    std::lock_guard<std::mutex> lock(g_llm_mutex);
    return g_active_key;
}
