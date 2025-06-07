#pragma once
#include <atomic>

extern std::atomic<bool> shutdown_requested;
void listen_notifications();
