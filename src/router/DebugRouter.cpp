#include "router/DebugRouter.h"

#include <algorithm>
#include <chrono>
#include <thread>

static void sseSendDbg(const ::HttpResponseWriterPtr& writer, const char* event, const hv::Json& payload) {
    writer->SSEvent(payload.dump(), event);
}

static int parseSecondsFromFullPath(const std::string& fullpath) {
    const size_t q = fullpath.find('?');
    if (q == std::string::npos) return 60;
    const std::string qs = fullpath.substr(q + 1);
    size_t i = 0;
    while (i < qs.size()) {
        size_t amp = qs.find('&', i);
        if (amp == std::string::npos) amp = qs.size();
        size_t eq = qs.find('=', i);
        if (eq != std::string::npos && eq < amp) {
            const std::string key = qs.substr(i, eq - i);
            const std::string val = qs.substr(eq + 1, amp - eq - 1);
            if (key == "seconds") return std::atoi(val.c_str());
        }
        i = amp + 1;
    }
    return 60;
}

void DebugRouter::registerRoutes(hv::HttpService& service) {
    service.GET("/debug/sse/hold", [](const ::HttpRequestPtr& req, const ::HttpResponseWriterPtr& writer) {
        const int64_t raw = parseSecondsFromFullPath(req->FullPath());
        const int seconds = (int)std::max<int64_t>(1, std::min<int64_t>(3600, raw));
        writer->Begin();
        writer->WriteStatus(HTTP_STATUS_OK);
        writer->WriteHeader("Content-Type", "text/event-stream");
        writer->WriteHeader("Cache-Control", "no-cache");
        writer->WriteHeader("Connection", "keep-alive");
        writer->EndHeaders();

        for (int i = 0; i < seconds; i++) {
            sseSendDbg(writer, "delta", {{"delta", "ping"}, {"i", i}});
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        sseSendDbg(writer, "done", {{"done", true}, {"seconds", seconds}});
        writer->End();
    });
}
