#include "pgListener.h"
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>
#include <vector>
#include <mutex>
#include <thread>
#include <iostream>
#include "Routes.h" // for ws_connections and ws_mutex
#include <cstdlib> // For getenv
#include <string>

// Direct Connection
std::string get_direct_connection_string() {
    const char* dbname = std::getenv("PGDIRECT_DB");
    const char* user = std::getenv("PGDIRECT_USER");
    const char* password = std::getenv("PGDIRECT_PASSWORD");
    const char* host = std::getenv("PGDIRECT_HOST");
    const char* port = std::getenv("PGDIRECT_PORT");

    if (!dbname || !user || !password || !host || !port) {
        throw std::runtime_error("Database environment variables are not set for listener. Please check your .env file.");
    }

    return "dbname=" + std::string(dbname) +
           " user=" + std::string(user) +
           " password=" + std::string(password) +
           " host=" + std::string(host) +
           " port=" + std::string(port);
}

class NotifyListener : public pqxx::notification_receiver {
public:
    NotifyListener(pqxx::connection_base &conn, const std::string &channel)
        : pqxx::notification_receiver(conn, channel) {}

    void operator()(const std::string &payload, int) override {
        std::lock_guard<std::mutex> lock(ws_mutex);
        nlohmann::ordered_json filtered_json;
        nlohmann::ordered_json parsed_json;
        try {
            parsed_json = nlohmann::json::parse(payload);
        } catch (const nlohmann::json::parse_error& e) {
            throw std::runtime_error("Failed to parse JSON: " + std::string(e.what()));
        }
        filtered_json["id"] = parsed_json["id"];
        filtered_json["victim_name"] = parsed_json["victim_name"];
        filtered_json["loss_type"] = parsed_json["loss_type"];
        filtered_json["killer_name"] = parsed_json["killer_name"];
        filtered_json["time_stamp"] = parsed_json["time_stamp"];
        filtered_json["solar_system_id"] = parsed_json["solar_system_id"];
        pqxx::connection conn(get_pool_connection_string());
        pqxx::work txn(conn);
        std::string query = "SELECT solar_system_name FROM systems WHERE solar_system_id::text ILIKE $1;";
        std::string solar_system_id_str = filtered_json["solar_system_id"].is_number_integer() 
            ? std::to_string(filtered_json["solar_system_id"].get<long long>())
            : filtered_json["solar_system_id"].get<std::string>();
        pqxx::result result = txn.exec_params(query, solar_system_id_str);
        txn.commit();
        if (!result.empty()) filtered_json["solar_system_name"] = result[0][0].as<std::string>();
        std::string json_string = filtered_json.dump(4);
        for (auto* ws : ws_connections) ws->send_text(json_string);
    }
};

void listen_notifications() {
    while (!shutdown_requested) {
        try {
            pqxx::connection conn(get_direct_connection_string());
            NotifyListener listener(conn, "incident_trigger");
            {
                pqxx::work txn(conn);
                txn.exec("LISTEN incident_trigger;");
                txn.commit();
                std::cout << "Listening on channel 'incident_trigger'..." << std::endl;
            }
            while (conn.is_open() && !shutdown_requested) {
                bool notification_received = conn.await_notification(1,0);
                if (!notification_received) {
                    try {
                        pqxx::nontransaction nt(conn);
                        nt.exec("SELECT 1;");
                    } catch (const std::exception& e) {
                        std::cerr << "Heartbeat failure: " << e.what() << std::endl;
                        throw;
                    }
                }
            }
        } catch (const std::exception &e) {
            std::cerr << "Error in notification listener: " << e.what() << "\nReconnecting in 5s.\n";
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}
