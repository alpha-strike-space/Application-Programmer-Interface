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
// Notification Object is public
class NotifyListener : public pqxx::notification_receiver {
	// Public members
	public:
		NotifyListener(pqxx::connection_base &conn, const std::string &channel)
        		: pqxx::notification_receiver(conn, channel) {}
	// Operations method overriden
	void operator()(const std::string &payload, int) override {
        	std::lock_guard<std::mutex> lock(ws_mutex);
        	nlohmann::ordered_json filtered_json;
        	nlohmann::ordered_json parsed_json;
		// Try loading json to serialize
		try {
			parsed_json = nlohmann::json::parse(payload);
		} catch (const nlohmann::json::parse_error& e) {
			throw std::runtime_error("Failed to parse JSON: " + std::string(e.what()));
		}
		// Check json string from incident trigger channel in postgresql
		//std::cout << parsed_json.dump(4) << std::endl;
		// Time to initialize the postgresql connection
		pqxx::connection conn(get_pool_connection_string());
		pqxx::work txn(conn);
		// Get victim information
		std::string victim_name, victim_tribe_name, victim_address;
		try {
			pqxx::result v_res = txn.exec_params("SELECT c.name, t.name, encode(c.address, 'hex') "
						"FROM characters c "
						"LEFT JOIN character_tribe_membership ctm "
						"ON c.id = ctm.character_id "
						"AND ctm.joined_at <= $2 "
						"AND (ctm.left_at IS NULL OR ctm.left_at > $2) "
						"LEFT JOIN tribes t ON ctm.tribe_id = t.id "
						"WHERE c.id = $1 LIMIT 1;",
						parsed_json["victim_id"].get<std::string>(),
						parsed_json["time_stamp"].get<long long>());
			// Check query information and store.
			if (!v_res.empty()) {
				victim_name = v_res[0][0].as<std::string>();
				victim_tribe_name = v_res[0][1].is_null() ? "" : v_res[0][1].as<std::string>();
				victim_address = v_res[0][2].is_null() ? "" : v_res[0][2].as<std::string>();
			}
		} catch (const std::exception& e) {
			// Default in case there is an issue with database.
			victim_name = "";
			victim_tribe_name = "";
			victim_address = "";
			std::cerr << "Error: " << e.what() << std::endl;
		}
		// Get killer information
		std::string killer_name, killer_tribe_name, killer_address;
		try {
			pqxx::result k_res = txn.exec_params("SELECT c.name, t.name, encode(c.address, 'hex') "
						"FROM characters c "
						"LEFT JOIN character_tribe_membership ctm "
						"ON c.id = ctm.character_id "
						"AND ctm.joined_at <= $2 "
						"AND (ctm.left_at IS NULL OR ctm.left_at > $2) "
						"LEFT JOIN tribes t ON ctm.tribe_id = t.id "
						"WHERE c.id = $1 LIMIT 1;",
						parsed_json["killer_id"].get<std::string>(),
						parsed_json["time_stamp"].get<long long>());
			// Check query information and store.
			if (!k_res.empty()) {
				killer_name = k_res[0][0].as<std::string>();
				killer_tribe_name = k_res[0][1].is_null() ? "" : k_res[0][1].as<std::string>();
				killer_address = k_res[0][2].is_null() ? "" : k_res[0][2].as<std::string>();
			}
		} catch (const std::exception& e) {
			// Default in case there is an issue with database.
			killer_name = "";
			killer_tribe_name = "";
			killer_address = "";
			std::cerr << "Error: " << e.what() << std::endl;
		}
		// Get system information
		std::string solar_system_name;
		try {
			pqxx::result s_res = txn.exec_params("SELECT solar_system_name FROM systems WHERE solar_system_id::text ILIKE $1;",
						parsed_json["solar_system_id"].is_number_integer() 
						? std::to_string(parsed_json["solar_system_id"].get<long long>())
						: parsed_json["solar_system_id"].get<std::string>());
			// Check and set string
			if (!s_res.empty()) {
				solar_system_name = s_res[0][0].as<std::string>();
			}
		} catch (const std::exception& e) {
			// Default in case there is an issue with database.
			solar_system_name = "";
			std::cerr << "Error: " << e.what() << std::endl;
		}
		// Order json before stringify
		filtered_json["id"] = parsed_json["id"];
		filtered_json["victim_tribe_name"] = victim_tribe_name;
		filtered_json["victim_name"] = victim_name;
		filtered_json["victim_address"] = victim_address;
		// Check loss type
		std::string loss_type;
		if (parsed_json["loss_type"].is_string()) {
			std::string loss_type_value = parsed_json["loss_type"].get<std::string>();
			loss_type = (loss_type_value == "0") ? "ship/structure" : loss_type_value; // Make sure we are not "0"
		} else if (parsed_json["loss_type"].is_number_integer()) {
			int loss_type_val = parsed_json["loss_type"].get<int>();
			loss_type = (loss_type_val == 0) ? "ship/structure" : std::to_string(loss_type_val);
		} else {
			loss_type = "";
		}
		filtered_json["loss_type"] = loss_type;
		filtered_json["killer_tribe_name"] = killer_tribe_name;
		filtered_json["killer_name"] = killer_name;
		filtered_json["killer_address"] = killer_address;
		filtered_json["time_stamp"] = parsed_json["time_stamp"];
		filtered_json["solar_system_id"] = parsed_json["solar_system_id"];
		filtered_json["solar_system_name"] = solar_system_name;
		// Dump that json back as a string for send off
		std::string json_string = filtered_json.dump(4);
		// Run through all json string packages
		for (auto* ws : ws_connections) {
			ws->send_text(json_string); // Send out string across websocket mutex.
		}
    	}
};
// Notifications loop to stay on the database trigger.
void listen_notifications() {
	// Make sure we are still on and using those threads.
	while (!shutdown_requested) {
		// Get on our postgresql trigger channel
		try {
			pqxx::connection conn(get_direct_connection_string());
			NotifyListener listener(conn, "incident_trigger");
			{
				pqxx::work txn(conn);
				txn.exec("LISTEN incident_trigger;");
				txn.commit();
				std::cout << "Listening on channel 'incident_trigger'..." << std::endl;
			}
			// Check we are open for business and not wasting threads
			while (conn.is_open() && !shutdown_requested) {
				// Flag
				bool notification_received = conn.await_notification(1,0);
				// Not received
				if (!notification_received) {
					// Heartbeat
					try {
						pqxx::nontransaction nt(conn);
						nt.exec("SELECT 1;");
					} catch (const std::exception& e) {
						std::cerr << "Heartbeat failure: " << e.what() << std::endl;
						throw; // Heart attack
					}
				}
			}
		} catch (const std::exception &e) {
			std::cerr << "Error in notification listener: " << e.what() << std::endl << "Reconnecting in 5s." << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(5));
		}
	}
}
