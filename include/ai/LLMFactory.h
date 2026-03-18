#ifndef AI_LLM_FACTORY_H
#define AI_LLM_FACTORY_H

#include "ai/LLM.h"

#include <memory>
#include <string>
#include <vector>

class LLMFactory {
public:
    struct Profile {
        struct Capabilities {
            bool vision{false};
            bool audio{false};
            bool files{false};
        };

        std::string key;
        std::string provider;
        std::string api_key;
        std::string base_url;
        std::string default_model;
        int timeout_seconds{30};
        Capabilities capabilities;
    };

    static std::unique_ptr<LLM> create();
    static std::shared_ptr<LLM> get();
    static std::shared_ptr<LLM> get(const std::string& key);
    static void reset();
    static void reset(const std::string& key);

    static bool upsertProfile(const Profile& profile);
    static std::vector<Profile> listProfiles();
    static bool loadProfilesFromFile(const std::string& path);
    static bool saveProfilesToFile(const std::string& path);
    static void setActiveKey(const std::string& key);
    static std::string getActiveKey();
    static Profile::Capabilities inferCapabilitiesFromModel(const std::string& model);
};

#endif
