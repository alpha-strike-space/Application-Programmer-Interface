#pragma once
#include "crow.h"
// Functions for CROW_ROUTE and CROW_WEBSOCKET_ROUTE
void setupRoutes(crow::SimpleApp& app);
void setupWebSocket(crow::SimpleApp& app);
// Don't violate the one rule definition.
std::string get_pool_connection_string();
// Extern to make global variables for the websocket.
extern std::vector<crow::websocket::connection*> ws_connections;
extern std::mutex ws_mutex;
