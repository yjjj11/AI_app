#ifndef LOGIN_ROUTER_H
#define LOGIN_ROUTER_H

#include "IRouter.h"
#include "../service/LoginService.h"

class LoginRouter : public IRouter {
public:
    void registerRoutes(hv::HttpService& service) override;

private:
    LoginService login_service_;
};

#endif
