#ifndef LOGIN_SERVICE_H
#define LOGIN_SERVICE_H

#include <string>

class LoginService {
public:
    std::string login(const std::string& username, const std::string& password);
};

#endif
