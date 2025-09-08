#pragma once
#include <atomic>
// Listen for postgresql
extern std::atomic<bool> shutdown_requested;
void listen_notifications();
