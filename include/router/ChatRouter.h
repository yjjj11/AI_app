#ifndef CHAT_ROUTER_H
#define CHAT_ROUTER_H

#include "router/IRouter.h"
#include "service/ChatService.h"
#include "service/ChatDbModels.h"
class ChatRouter : public IRouter {
public:
    void registerRoutes(hv::HttpService& service) override;

private:
    ChatService chat_service_;
};

#endif
