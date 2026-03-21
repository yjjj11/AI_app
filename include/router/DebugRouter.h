#ifndef ROUTER_DEBUG_ROUTER_H
#define ROUTER_DEBUG_ROUTER_H

#include "router/IRouter.h"

class DebugRouter : public IRouter {
public:
    void registerRoutes(hv::HttpService& service) override;
};

#endif

