#pragma once
#include "crow.h"
#include <thread>
#include <atomic>
// Server Object
class Server {
	// Public Members
	public:
		Server();
		~Server();
		void run();
	// Private Members 
	private:
		crow::SimpleApp app;
		std::thread pg_listener;
		void setup();
		void startPgListener();
		void stop();
};
