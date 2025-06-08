#pragma once
#include "crow.h"
// Functions for CROW_ROUTE and CROW_WEBSOCKET_ROUTE
void setupRoutes(crow::SimpleApp& app);
void setupWebSocket(crow::SimpleApp& app);

// Helper function to build the connection string
std::string get_connection_string();

// Extern to make global variables for the websocket.
extern std::vector<crow::websocket::connection*> ws_connections;
extern std::mutex ws_mutex;
