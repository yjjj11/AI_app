#ifndef ROUTER_INTERVIEW_ROUTER_H
#define ROUTER_INTERVIEW_ROUTER_H

#include "router/IRouter.h"
#include "service/InterviewService.h"

class InterviewRouter : public IRouter {
public:
    void registerRoutes(hv::HttpService& service) override;

private:
    InterviewService interview_service_;
};

#endif

