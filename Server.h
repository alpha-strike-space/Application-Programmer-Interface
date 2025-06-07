#pragma once
#include "crow.h"
#include <thread>
#include <atomic>

class Server {
public:
    Server();
    ~Server();
    void run();

private:
    crow::SimpleApp app;
    std::thread pg_listener;
    void setup();
    void startPgListener();
    void stop();
};
