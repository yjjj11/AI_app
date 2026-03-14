#include "HttpServer.h"
#include "config/Dotenv.h"

int main() {
    Dotenv::loadFromFile("/root/my_ai_app/config/mysql.env");
    Dotenv::loadFromFile("/root/my_ai_app/config/ai.env");
    MyHttpServer server(8888, 4);
    server.run();
    return 0;
}
