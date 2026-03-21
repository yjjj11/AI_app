#ifndef SERVICE_INTERVIEW_SERVICE_H
#define SERVICE_INTERVIEW_SERVICE_H

#include <functional>
#include <string>
#include <vector>

#include <hv/HttpService.h>

class InterviewService {
public:
    hv::Json parseJd(const std::string& jd_text, const std::string& direction);
    hv::Json generatePlan(const std::string& jd_text, const std::string& resume_text, const std::string& direction);
    hv::Json reviewSession(const std::string& username, const std::string& session_id, const std::string& direction);

    std::string streamTurn(const std::string& session_id,
                           const std::string& jd_text,
                           const std::string& resume_text,
                           const std::string& direction,
                           const std::string& user_answer,
                           const std::function<void(std::string_view)>& on_delta);
};

#endif
