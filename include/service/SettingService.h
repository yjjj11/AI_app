#ifndef SETTING_SERVICE_H
#define SETTING_SERVICE_H

#include <hv/HttpService.h>

class SettingService {
public:
    hv::Json listLlmProfiles();
    bool applyLlmConfig(const hv::Json& body, hv::Json& out, int& status_code);
    hv::Json setActiveProfile(const hv::Json& body);
};

#endif
