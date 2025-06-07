#include "Server.h"
#include "Routes.h"
#include "pgListener.h"
#include <signal.h>
#include <chrono>
// Graceful shutdown procedures, bool value set.
std::atomic<bool> shutdown_requested(false);
// Handle system level status signal
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) shutdown_requested = true;
}
// System level signals setup.
Server::Server() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    setup();
}
// Send signal when server is fully stopped, i.e. no running threads.
Server::~Server() {
    stop();
}
// Step up server app for routes.
void Server::setup() {
    setupRoutes(app);
    setupWebSocket(app);
}
// Set up postgresql thread.
void Server::startPgListener() {
    pg_listener = std::thread(listen_notifications);
}
// Start server and run asynchronously.
void Server::run() {
    startPgListener();
    app.bindaddr("0.0.0.0").port(8080).multithreaded().run_async();
    while (!shutdown_requested) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    app.stop();
    if (pg_listener.joinable()) pg_listener.join();
}
// Server stop
void Server::stop() {
    shutdown_requested = true;
}
// Where we tie everyting together.
int main() {
    Server server;
    server.run();
    return 0;
}
