#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <hv/HttpServer.h>
#include <hv/HttpService.h>

#include <memory>
#include <vector>

#include "router/IRouter.h"

class MyHttpServer {
public:
    MyHttpServer(int port, int thread_num);
    void run();

private:
    void setupMiddleware();
    void registerRoutes();

    int port_{0};
    int thread_num_{0};
    hv::HttpService service_;
    hv::HttpServer server_;
    std::vector<std::unique_ptr<IRouter>> routers_;
};

#endif
