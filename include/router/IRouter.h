#ifndef IROUTER_H
#define IROUTER_H

#include <hv/HttpService.h>

class IRouter {
public:
    virtual ~IRouter() = default;
    virtual void registerRoutes(hv::HttpService& service) = 0;
};

#endif
