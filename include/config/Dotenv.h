#ifndef DOTENV_H
#define DOTENV_H

#include <string>

class Dotenv {
public:
    static bool loadFromFile(const std::string& filepath);
};

#endif
