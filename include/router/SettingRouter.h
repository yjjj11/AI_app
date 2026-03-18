#ifndef SETTING_ROUTER_H
#define SETTING_ROUTER_H

#include "router/IRouter.h"
#include "service/SettingService.h"

class SettingRouter : public IRouter {
public:
    void registerRoutes(hv::HttpService& service) override;

private:
    SettingService setting_service_;
};

#endif

