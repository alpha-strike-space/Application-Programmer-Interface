#include "crow.h"
#include "pqxx/pqxx"
#include <iostream>
#include <vector>
#include <set>
#include <mutex>
#include <thread>
#include <chrono>
#include <algorithm>
#include <locale>
#include <stdexcept>
#include <nlohmann/json.hpp>
// Store WebSocket connections
// Global container to hold active WebSocket connections
std::vector<crow::websocket::connection*> ws_connections;
std::mutex ws_mutex;
// Minimal notification receiver for PostgreSQL triggers,
// now matching the abstract base's expected signature.
class NotifyListener : public pqxx::notification_receiver {
public:
    // Construct by passing a reference to the open connection and the channel to listen on.
    NotifyListener(pqxx::connection_base &conn, const std::string &channel)
        : pqxx::notification_receiver(conn, channel) {}

    // This method is called automatically when a notification is received.
    // It receives the payload and the sender's backend process ID.
    void operator()(const std::string &payload, int backend_pid) override {
        // Lock down our users
        std::lock_guard<std::mutex> lock(ws_mutex);
        // Build our json response using nlohmann
        nlohmann::ordered_json filtered_json;
	nlohmann::ordered_json parsed_json;
    	// Make sure we have json from the postgresql trigger.
	try {
    	    	// Attempt to parse the JSON string.
    		parsed_json = nlohmann::json::parse(payload);
    	} catch (const nlohmann::json::parse_error& e) {
        // If parsing fails, throw a runtime error.
    	    throw std::runtime_error("Failed to parse JSON: " + std::string(e.what()));
    	}
        // Finally, copy the rest of the fields from parsed_json.
	//filtered_json["id"] = parsed_json["id"];
        filtered_json["victim_name"] = parsed_json["victim_name"];
        filtered_json["loss_type"] = parsed_json["loss_type"];
	filtered_json["killer_name"] = parsed_json["killer_name"];
       	filtered_json["time_stamp"] = parsed_json["time_stamp"];
        filtered_json["solar_system_id"] = parsed_json["solar_system_id"];
        // Create the PostgreSQL connection.
        pqxx::connection conn(/* "dbname= user= password= host= port=" */);
        pqxx::work txn(conn);
        // Prepare the query. We use ILIKE for case-insensitive matching.
        std::string query = "SELECT solar_system_name FROM systems WHERE solar_system_id::text ILIKE $1;";
        // Convert the solar_system_id to a string.
        // If it's a number, you may convert it using std::to_string.
        std::string solar_system_id_str;
        if (filtered_json["solar_system_id"].is_number_integer()) {
            solar_system_id_str = std::to_string(filtered_json["solar_system_id"].get<long long>());
        } else {
            // If it is already a string, retrieve it directly.
            solar_system_id_str = filtered_json["solar_system_id"].get<std::string>();
        }
        // Execute the query with parameter binding.
        pqxx::result result = txn.exec_params(query, solar_system_id_str);
        txn.commit();
        // If a valid result was found, add the solar_system_name into filtered_json.
        if (!result.empty()) {
            std::string fetched_value = result[0][0].as<std::string>();
            filtered_json["solar_system_name"] = fetched_value;
        }
	std::string json_string = filtered_json.dump(4); // Pretty printing with indent of 4 spaces
	// Logging
	//std::cout << json_string << std::endl;
        // Send the notification to every connected WebSocket client.
        for (auto* ws : ws_connections) {
            //ws->send_text(message);
            ws->send_text(json_string);
        }
    }
};
// This function connects to PostgreSQL, issues a LISTEN command,
// and continuously polls for notifications. When a notification is
// received, its payload is forwarded to all active WebSocket clients.
// Function to listen for PostgreSQL notifications
void listen_notifications() {
    // The outer loop allows us to automatically attempt reconnection if something goes wrong.
    while (true) {
        try {
                // Connection
                pqxx::connection conn(/* "dbname= user= password= host= port=" */);
                // Instantiate our notification receiver on channel "incident_trigger"
                NotifyListener listener(conn, "incident_trigger");

                // Begin a transaction to issue the LISTEN command on the desired channel.
                {
                   pqxx::work txn(conn);
                   txn.exec("LISTEN incident_trigger;");
                   txn.commit();
                   std::cout << "Listening on channel 'incident_trigger' for notifications..." << std::endl;
                }
                // Main listening loop.
                while (conn.is_open()) {
                        // Wait up to 1000 milliseconds for a notification.
                        // (Note: Some versions of libpqxx support a timeout parameter for await_notification.)
                        bool notification_received = conn.await_notification(1000);
                        // Check notification
                        if (!notification_received) {
                                // A timeout occurred without any notifications. This is a good moment
                                // to perform a lightweight heartbeat query to ensure the connection is still he>
                             try {
                                        pqxx::nontransaction nt(conn);
                                        nt.exec("SELECT 1;");
                                        // Optionally, log heartbeat messages for debugging:
                                        // std::cout << "Heartbeat OK." << std::endl;
                                }
                                catch (const std::exception &e) {
                                        std::cerr << "Heartbeat failure: " << e.what() << std::endl;
                                        throw; // Re-throw to enter the outer catch block and reconnect.
                                }
                        }
                }
        } catch (const std::exception &e) {
                std::cerr << "Error in notification listener: " << e.what() << "\nAttempting to reconnect in 5 seconds" << std::endl;
                // Sleep before attempting to reconnect.
                std::this_thread::sleep_for(std::chrono::seconds(5));
                // The outer while loop then causes reattempt of connection and registration.
        }
    }
}
// Parsing json function for incident endpoint.
nlohmann::ordered_json build_incident_json(const pqxx::result& res) {
    // Create an empty JSON array.
    nlohmann::ordered_json json_array = nlohmann::ordered_json::array();
    // Loop through each row in the pqxx::result.
    for (const auto& row : res) {
	// Create json to iterate over.
        nlohmann::ordered_json item;
	// All the mail information
	item["id"] = row["id"].as<long long>();
        item["victim_address"] = row["victim_address"].as<std::string>();
        item["victim_name"] = row["victim_name"].as<std::string>();
        item["loss_type"] = row["loss_type"].as<std::string>();
        item["killer_address"] = row["killer_address"].as<std::string>();
        item["killer_name"] = row["killer_name"].as<std::string>();
	item["time_stamp"] = row["time_stamp"].as<long long>();
        item["solar_system_id"] = row["solar_system_id"].as<long long>();
	item["solar_system_name"] = row["solar_system_name"].as<std::string>();
        // Add to the array.
        json_array.push_back(std::move(item));
    }
    return json_array;
}
// Parsing json function for the location endpoint.
nlohmann::ordered_json build_system_json(const pqxx::result& res) {
    nlohmann::ordered_json json_array = nlohmann::ordered_json::array();
    for (const auto& row : res) {
        // Create json to iterate over.
	nlohmann::ordered_json item;
        // List name and identifier
        item["solar_system_id"] = row["solar_system_id"].as<long long>();
        item["solar_system_name"] = row["solar_system_name"].as<std::string>();
	// Group the coordinate values under a single "coordinates" object.
        item["coordinates"] = {
            {"x", row["x"].as<std::string>()},
            {"y", row["y"].as<std::string>()},
            {"z", row["z"].as<std::string>()}
        };
	// Group the coordinate values under a single "coordinates" object.
	// Add to the array.
        json_array.push_back(std::move(item));
    }
    return json_array;
}
// Format query results for totals endpoint based on name.
nlohmann::ordered_json formatName(const pqxx::result& resName) {
	nlohmann::ordered_json jsonResponse = nlohmann::ordered_json::array();
	for (const auto& row : resName) {
            	// Create json to iterate over
		nlohmann::ordered_json item;
            	item["name"] = row["person"].as<std::string>();
            	item["total_kills"] = row["total_kills"].as<int>();
            	item["total_losses"] = row["total_losses"].as<int>();
            	jsonResponse.push_back(item);
	}
	return jsonResponse;
}
// Format query results for top killers.
nlohmann::ordered_json formatTopKillers(const pqxx::result& resKillers) {
   	nlohmann::ordered_json topKillers = nlohmann::ordered_json::array();
    	for (const auto& row : resKillers) {
		// Create json to iterate over
        	nlohmann::ordered_json item;
		// List name and incident count.
        	item["name"] = row["name"].as<std::string>();
        	item["kills"] = row["incident_count"].as<int>();
        	topKillers.push_back(item);
    	}
    	return topKillers;
}
// Format query results for top victims.
nlohmann::ordered_json formatTopVictims(const pqxx::result& resVictims) {
    	nlohmann::ordered_json topVictims = nlohmann::ordered_json::array();
    	for (const auto& row : resVictims) {
        	// Create json to iterate over
		nlohmann::ordered_json item;
		// List name and incident count
        	item["name"] = row["name"].as<std::string>();
        	item["losses"] = row["incident_count"].as<int>();
        	topVictims.push_back(item);
    	}
    	return topVictims;
}
// Format query results for top systems.
nlohmann::ordered_json formatSystems(const pqxx::result& resSystems) {
   	nlohmann::ordered_json topSystems = nlohmann::ordered_json::array();
    	for (const auto& row : resSystems) {
		// Create json to iterate over
        	nlohmann::ordered_json item;
        	item["solar_system_id"] = row["solar_system_id"].as<std::string>();
        	item["solar_system_name"] = row["solar_system_name"].as<std::string>();
        	item["incident_count"] = row["incident_count"].as<int>();
        	topSystems.push_back(item);
    	}
    	return topSystems;
}
// Main API function end-point architecture.
int main() {
        // Log everything
        crow::logger::setLogLevel(crow::LogLevel::Debug);
        // Create the crow application
        crow::SimpleApp app;
        // Get server health
        CROW_ROUTE(app, "/health").methods("GET"_method)([]() {
                // Create response value.
                return crow::response(200, std::string("I'm alive!"));
        });
        // get locations
        CROW_ROUTE(app, "/location").methods("GET"_method)([](const crow::request &req) -> crow::response {
                // Get Method
                if(req.method == crow::HTTPMethod::Get) {
                        try {
                                pqxx::connection conn(/* "dbname= user= password= host= port=" */);
                                pqxx::work txn(conn);
                                pqxx::result res;
                                // Check for the parameters by initializing a pointer for the url sent.
                                const char* system_parameter = req.url_params.get("system");
				// Check the parameters every time we are called up.
                                if(system_parameter) {
                                        // Parse our search value system_parameter
                                        std::string searchPattern = "%" + std::string(system_parameter) + "%";
                                        // Prepare SQL call.
                                        res = txn.exec_params("SELECT solar_system_name, solar_system_id, x, y, z FROM systems"
                                                " WHERE solar_system_name ILIKE $1 or solar_system_id::text ILIKE $1;",
                                                searchPattern
                                        );
                                        // Check if query returned any rows.
                                        if (res.size() == 0) {
                                                crow::json::wvalue error_response;
                                                error_response["error"] = "Bad Request! No system records found";
                                                return crow::response(400, error_response);
                                        }
                                } else {
                                        res = txn.exec_params("SELECT solar_system_name, solar_system_id, x, y, z FROM systems");
                                }
                                // Transact
                                txn.commit();
        			// Build the JSON using nlohmann
        			auto system_string = build_system_json(res);
        			// Convert JSON object to string
        			std::string json_string = system_string.dump(4); // Pretty printing with indent of 4 spaces
       		 		// Create a Crow response with the correct Content-Type header
        			crow::response resp(json_string);
        			resp.set_header("Content-Type", "application/json");
        			return resp;
                        // Error catching
                        } catch (const std::exception& e){
				// Log the error and return an error message.
        			std::cerr << "Error: " << e.what() << std::endl;
                                return crow::response(500, std::string("Internal Server Error!"));
                        }
                } else {
                       // Send error for method issues.
                       return crow::response(405);
                }
        });
	// Get totals
	CROW_ROUTE(app, "/totals").methods("GET"_method)([](const crow::request &req) -> crow::response {
		// Get Method
		if(req.method != crow::HTTPMethod::Get) {
			// Wrong method sent.
                   	return crow::response(405);
		}
		// Try user input.
		try {
                                // Get your PostgreSQL connection
                                pqxx::connection conn(/* "dbname= user= password= host= port=" */);
                                pqxx::work txn(conn);
                                pqxx::result res;
				pqxx::result resKillers;
				pqxx::result resVictims;
				pqxx::result resSystems;
                                // Check for the parameters by initializing a pointer for the url sent.
                                const char* name_parameter = req.url_params.get("name"); // name
                                const char* system_parameter = req.url_params.get("system"); // system by id or name
				// Extract the "filter" parameter (e.g., "24h", "week", or "month")
    				const char* filter_parameter = req.url_params.get("filter");
				// Build parameters
                                // Helper lambda to build search pattern.
                                auto buildSearchPattern = [](const char* value) -> std::string {
                                        return std::string("%") + std::string(value) + "%";
                                };
				// Create a time-based helper lambda to build the time-based clause.
    				auto getTimeClause = [](const char* filter_param) -> std::string {
        				// Check for filter value passed.
					if (!filter_param)
						return "";
					// Create string to hold passed value.
        				std::string filterStr = filter_param;
					// Check our filter value.
        				if (filterStr == "day") {
            					return "to_timestamp((i.time_stamp - 116444736000000000) / 10000000.0) >= now() - interval '24 hours'";
        				} else if (filterStr == "week") {
            					return "to_timestamp((i.time_stamp - 116444736000000000) / 10000000.0) >= now() - interval '7 days'";
        				} else if (filterStr == "month") {
            					return "to_timestamp((i.time_stamp - 116444736000000000) / 10000000.0) >= now() - interval '1 month'";
        				} else {
						return "";
					}
    				};
				// Check the parameters every time we are called up.
                                if(name_parameter) {
                                        // Parse our search value name parameter.
                                        std::string searchPattern = buildSearchPattern(name_parameter);
					// Work with the filter.
					std::string timeClause = getTimeClause(filter_parameter);
					// Base query
					std::string baseQuery = "WITH combined AS ("
    								"  SELECT killer_name AS person, 1 AS kill_count, 0 AS loss_count, "
    								"         to_timestamp((time_stamp - 116444736000000000) / 10000000.0) AS event_time "
    								"  FROM incident i "
    								"  UNION ALL "
    								"  SELECT victim_name AS person, 0 AS kill_count, 1 AS loss_count, "
    								"         to_timestamp((time_stamp - 116444736000000000) / 10000000.0) AS event_time "
    								"  FROM incident i "
    								") "
    								"SELECT person, "
    								"       SUM(kill_count) AS total_kills, "
    								"       SUM(loss_count) AS total_losses "
    								"FROM combined "
    								"WHERE person LIKE $1 ";  //The parameter placeholder for our name search.;
					// Empty query string.
					std::string query;
					// Append filters
					if (!timeClause.empty()) {
    						baseQuery += "AND " + timeClause + " ";
					}
					query = baseQuery + "GROUP BY person;";
					std::cout << query << std::endl;
                                        // Prepare SQL call.
                                        res = txn.exec_params(query, searchPattern);
                                        // Check if query returned any rows.
                                        if (res.size() == 0) {
                                                crow::json::wvalue error_response;
                                                error_response["error"] = "Bad Request! No incident records found!";
                                                return crow::response(400, error_response);
                                        }
					// Build the JSON using nlohmann
                                	auto name_string = formatName(res);
                                	// Convert JSON object to string
                                	std::string json_string = name_string.dump(4); // Pretty printing with indent of 4 spaces
                                	// Create a Crow response with the correct Content-Type header
                                	crow::response resp(json_string);
                                	resp.set_header("Content-Type", "application/json");
                                	return resp;
                                } else if(system_parameter) {
                                        // Convert the string to a 64-bit integer using std::stoll.
                                        std::string searchPattern = buildSearchPattern(system_parameter);
                                        // Work with the filter.
                                        std::string timeClause = getTimeClause(filter_parameter);
					// Base query that aggregates kill/loss counts per "person"
    					// and joins with the systems table to include solar_system_name.
   	 				std::string baseQuery = "SELECT s.solar_system_id, "
        							"       s.solar_system_name, "
        							"       COUNT(*) AS incident_count "
        							"FROM incident i "
        							"JOIN systems s ON i.solar_system_id = s.solar_system_id "
        							"WHERE (s.solar_system_id::text ILIKE $1 OR s.solar_system_name ILIKE $1) ";
					// Empty query string.
					std::string query;
					// Append filters
					if (!timeClause.empty()) {
        					baseQuery += "AND " + timeClause + " ";
    					}
    					query = baseQuery + "GROUP BY s.solar_system_id, s.solar_system_name "
                 				"ORDER BY s.solar_system_id DESC;";
					std::cout << query << std::endl;
                                        // Prepare SQL call.
                                        res = txn.exec_params(query, searchPattern);
                                        // Check if query returned any rows.
                                        if (res.size() == 0) {
                                                crow::json::wvalue error_response;
                                                error_response["error"] = "Bad Request! No incident records found";
                                                return crow::response(400, error_response);
                                        }
					// Reuse formatTopSystems since returns are the same with nlohmann.
					auto systems_string = formatSystems(res);
                                	// Convert JSON object to string
                                	std::string json_string = systems_string.dump(4); // Pretty printing with indent of 4 spaces
                                	// Create a Crow response with the correct Content-Type header
                                	crow::response resp(json_string);
                                	resp.set_header("Content-Type", "application/json");
                                	return resp;
                                } else {
					// Work with the filter.
                                        std::string timeClause = getTimeClause(filter_parameter);
                                        // Base query for the Top Ten Killers
    					// Count rows per killer_name using the provided time filter.
    					std::string qKillers = "SELECT killer_name AS name, COUNT(*) AS incident_count "
        						"FROM incident i "
							"WHERE killer_name <> '' ";  // exclude blank names if any
    					// Append filters
					if (!timeClause.empty()) {
        					qKillers += " AND " + timeClause;
    					}
    					qKillers += " GROUP BY killer_name "
                				" ORDER BY incident_count DESC LIMIT 10;";
					// Base query for the Top Ten Victims ---
    					std::string qVictims = "SELECT victim_name AS name, COUNT(*) AS incident_count "
							"FROM incident i "
							"WHERE victim_name <> '' ";  // ensure a valid name
    					// Append filters
					if (!timeClause.empty()) {
        					qVictims += " AND " + timeClause;
    					}
    					qVictims += " GROUP BY victim_name "
                				" ORDER BY incident_count DESC LIMIT 10;";
    					// base query for the Top Ten Systems
    					// Retrieve solar_system info by joining incident and systems tables.
    					std::string qSystems = "SELECT s.solar_system_id, s.solar_system_name, COUNT(*) AS incident_count "
        						"FROM incident i "
        						"JOIN systems s ON i.solar_system_id = s.solar_system_id ";
					// Append filters
    					if (!timeClause.empty()) {
        					qSystems += " WHERE " + timeClause;
    					}
    					qSystems += " GROUP BY s.solar_system_id, s.solar_system_name "
                				" ORDER BY incident_count DESC LIMIT 10;";
					// Optionally, log or print the queries.
    					std::cout << "Killers Query: " << qKillers << std::endl;
    					std::cout << "Victims Query: " << qVictims << std::endl;
    					std::cout << "Systems Query: " << qSystems << std::endl;
					// Execute the queries.
    					resKillers = txn.exec(qKillers);
   	 				resVictims = txn.exec(qVictims);
    					resSystems  = txn.exec(qSystems);
					// Format each result via its dedicated function.
        				auto topKillers = formatTopKillers(resKillers);
        				auto topVictims = formatTopVictims(resVictims);
        				auto topSystems = formatSystems(resSystems);
				        // Assemble the final response.
        				nlohmann::ordered_json response;
        				response["top_killers"] = topKillers;
        				response["top_victims"] = topVictims;
        				response["top_systems"] = topSystems;
                                	// Convert JSON object to string
                                	std::string json_string = response.dump(4); // Pretty printing with indent of 4 spaces
                                	// Create a Crow response with the correct Content-Type header
                                	crow::response resp(json_string);
                                	resp.set_header("Content-Type", "application/json");
                                	return resp;
				}
		} catch(const std::exception& e) {
                                std::cerr << "Notification error: " << e.what() << "\n";
                                return crow::response(500, std::string("Internal Server Error!"));
		}
	});
        // Get incidents
        CROW_ROUTE(app, "/incident").methods("GET"_method)([](const crow::request &req) -> crow::response {
                // Get Method
                if(req.method == crow::HTTPMethod::Get) {
                        try {
                                // Get your PostgreSQL connection
                                pqxx::connection conn(/* "dbname= user= password= host= port=" */);
                                pqxx::work txn(conn);
                                pqxx::result res;
                                // Check for the parameters by initializing a pointer for the url sent.
                                const char* name_parameter = req.url_params.get("name"); // name
                                const char* system_parameter = req.url_params.get("system"); // system by id or name
				// Extract the "filter" parameter (e.g., "24h", "week", or "month")
    				const char* filter_parameter = req.url_params.get("filter");
				const char* mail_parameter = req.url_params.get("mail_id");
				// Build parameters
                                // Helper lambda to build search pattern.
                                auto buildSearchPattern = [](const char* value) -> std::string {
                                        return std::string("%") + std::string(value) + "%";
                                };
				// Create a time-based helper lambda to build the time-based clause.
    				auto getTimeClause = [](const char* filter_param) -> std::string {
        				// Check for filter value passed.
					if (!filter_param)
						return "";
					// Create string to hold passed value.
        				std::string filterStr = filter_param;
					// Check our filter value.
        				if (filterStr == "day") {
            					return "to_timestamp((i.time_stamp - 116444736000000000) / 10000000.0) >= now() - interval '24 hours'";
        				} else if (filterStr == "week") {
            					return "to_timestamp((i.time_stamp - 116444736000000000) / 10000000.0) >= now() - interval '7 days'";
        				} else if (filterStr == "month") {
            					return "to_timestamp((i.time_stamp - 116444736000000000) / 10000000.0) >= now() - interval '1 month'";
        				} else {
						return "";
					}
    				};
                                // Check the parameters every time we are called up.
                                if(name_parameter) {
                                        // Parse our search value name parameter.
                                        std::string searchPattern = buildSearchPattern(name_parameter);
					// Work with the filter.
					std::string timeClause = getTimeClause(filter_parameter);
					// Base query
					std::string baseQuery = "SELECT i.id, COALESCE(i.victim_address, '') AS victim_address, "
								"COALESCE(i.victim_name, '') AS victim_name, "
								"COALESCE(i.killer_address, '') AS killer_address, "
								"COALESCE(i.killer_name, '') AS killer_name, "
								"i.solar_system_id, "
								"s.solar_system_name, "
        							"i.loss_type, "
        							"i.time_stamp "
        							"FROM incident AS i "
        							"JOIN systems AS s ON i.solar_system_id = s.solar_system_id "
        							"WHERE (i.killer_name::text LIKE $1 OR i.victim_name LIKE $1)";
					// Empty query string.
					std::string query;
					// Append filters
					if(!timeClause.empty()) {
   						query = baseQuery + " AND " + timeClause + " ORDER BY i.time_stamp DESC;";
					} else {
						query = baseQuery + " ORDER BY i.time_stamp DESC;";
					}
					//std::cout << query << std::endl;
                                        // Prepare SQL call.
                                        res = txn.exec_params(query, searchPattern);
                                        // Check if query returned any rows.
                                        if (res.size() == 0) {
                                                crow::json::wvalue error_response;
                                                error_response["error"] = "Bad Request! No incident records found!";
                                                return crow::response(400, error_response);
                                        }
                                } else if(system_parameter) {
                                        // Convert the string to a 64-bit integer using std::stoll.
                                        std::string searchPattern = buildSearchPattern(system_parameter);
                                        // Work with the filter.
                                        std::string timeClause = getTimeClause(filter_parameter);
					// Base query
                                        std::string baseQuery = "SELECT i.id, COALESCE(i.victim_address, '') AS victim_address, "
                                                                "COALESCE(i.victim_name, '') AS victim_name, "
                                                                "COALESCE(i.killer_address, '') AS killer_address, "
                                                                "COALESCE(i.killer_name, '') AS killer_name, "
                                                                "i.solar_system_id, "
                                                                "s.solar_system_name, "
                                                                "i.loss_type, "
                                                                "i.time_stamp "
                                                                "FROM incident AS i "
                                                                "JOIN systems AS s ON i.solar_system_id = s.solar_system_id "
                                                                "WHERE (i.solar_system_id::text ILIKE $1 OR s.solar_system_name ILIKE $1)";
                                        // Empty query string.
					std::string query;
					// Append filters
                                        if(!timeClause.empty()) {
                                                query = baseQuery + " AND " + timeClause + " ORDER BY i.time_stamp DESC;";
                                        } else {
                                                query = baseQuery + " ORDER BY i.time_stamp DESC;";
                                        }
					//std::cout << query << std::endl;
                                        // Prepare SQL call.
                                        res = txn.exec_params(query, searchPattern);
                                        // Check if query returned any rows.
                                        if (res.size() == 0) {
                                                crow::json::wvalue error_response;
                                                error_response["error"] = "Bad Request! No incident records found";
                                                return crow::response(400, error_response);
                                        }
				} else if(mail_parameter) {
                                        // Convert the string to a 64-bit integer using std::stoi.
                                        int searchPattern;
    					try {
       	 					searchPattern = std::stoi(std::string(mail_parameter));
    					} catch (const std::exception& e) {
        					return crow::response(400, "Invalid 'id' parameter");
    					}
					// Base query.
					std::string query = "SELECT i.id, COALESCE(i.victim_address, '') AS victim_address, "
                        					"COALESCE(i.victim_name, '') AS victim_name, "
                        					"COALESCE(i.killer_address, '') AS killer_address, "
                        					"COALESCE(i.killer_name, '') AS killer_name, "
                        					"i.solar_system_id, "
                        					"s.solar_system_name, "
                        					"i.loss_type, "
                        					"i.time_stamp "
                        					"FROM incident AS i "
                        					"JOIN systems AS s ON i.solar_system_id = s.solar_system_id "
			                        		"WHERE i.id = $1;";
                                        //std::cout << query << std::endl;
                                        // Prepare SQL call.
                                        res = txn.exec_params(query, searchPattern);
                                        // Check if query returned any rows.
                                        if (res.size() == 0) {
                                                crow::json::wvalue error_response;
                                                error_response["error"] = "Bad Request! No incident records found";
                                                return crow::response(400, error_response);
                                        }
                                } else if(filter_parameter) {
					// Work with the filter.
                                        std::string timeClause = getTimeClause(filter_parameter);
                                        // Base query
					std::string baseQuery = "SELECT i.id, COALESCE(i.victim_address, '') AS victim_address, "
    								"COALESCE(i.victim_name, '') AS victim_name, "
    								"COALESCE(i.killer_address, '') AS killer_address, "
    								"COALESCE(i.killer_name, '') AS killer_name, "
   								"i.solar_system_id, "
    								"s.solar_system_name, "
    								"i.loss_type, "
    								"i.time_stamp "
								"FROM incident "
								"AS i JOIN systems AS s ON i.solar_system_id = s.solar_system_id";
					// Empty query string.
					std::string query;
					// Append filters
                                        if(!timeClause.empty()) {
                                                query = baseQuery + " AND " + timeClause + " ORDER BY i.time_stamp DESC;";
                                        } else {
                                                query = baseQuery + " ORDER BY i.time_stamp DESC;";
					}
					//std::cout << query << std::endl;
					// Prepare SQL call.
                                        res = txn.exec(query);
					// Check if query returned any rows.
                                        if (res.size() == 0) {
                                                crow::json::wvalue error_response;
                                                error_response["error"] = "Bad Request! No incident records found";
                                                return crow::response(400, error_response);
                                        }
                                } else {
                                        // Base query
                                        std::string baseQuery = "SELECT i.id, COALESCE(i.victim_address, '') AS victim_address, "
                                                                "COALESCE(i.victim_name, '') AS victim_name, "
                                                                "COALESCE(i.killer_address, '') AS killer_address, "
                                                                "COALESCE(i.killer_name, '') AS killer_name, "
                                                                "i.solar_system_id, "
                                                                "s.solar_system_name, "
                                                                "i.loss_type, "
                                                                "i.time_stamp "
                                                                "FROM incident "
                                                                "AS i JOIN systems AS s ON i.solar_system_id = s.solar_system_id "
								"ORDER BY i.time_stamp DESC;";
                                        // Prepare SQL call.
                                        res = txn.exec(baseQuery);
                                        // Check if query returned any rows.
                                        if (res.size() == 0) {
                                               crow::json::wvalue error_response;
                                               error_response["error"] = "Bad Request! No incident records found";
                                               return crow::response(400, error_response);
                                        }
				}
                                // Transact
                                txn.commit();
                                // Build the JSON using nlohmann
                                auto incident_string = build_incident_json(res);
                                // Convert JSON object to string
                                std::string json_string = incident_string.dump(4); // Pretty printing with indent of 4 spaces
                                // Create a Crow response with the correct Content-Type header
                                crow::response resp(json_string);
                                resp.set_header("Content-Type", "application/json");
                                return resp;
                        // Error catching
                        } catch(const std::exception& e) {
                                std::cerr << "Notification error: " << e.what() << "\n";
                                return crow::response(500, std::string("Internal Server Error!"));
                        }
                } else {
                        // Send error for method issues.
                        return crow::response(405);
                }
            });
	// Websocket
        CROW_WEBSOCKET_ROUTE(app, "/mails").onopen([](crow::websocket::connection& ws) {
                std::cout << "WebSocket connection established." << std::endl;
                {
                    // Add the new connection to our global list (thread-safe)
                    std::lock_guard<std::mutex> lock(ws_mutex);
                    ws_connections.push_back(&ws);
                }
                // Inform the client that they've connected to the notification service.
		nlohmann::json msg;
		msg["message"] = "Connected to alpha-strikes notification service.";
		// Now send the JSON string (using .dump() to serialize it)
		ws.send_text(msg.dump());
        }).onclose([](crow::websocket::connection& ws, const std::string& reason, uint16_t close_code) {
                std::cout << "WebSocket connection closed!" << std::endl;
                // Remove the closed connection from the global list
                std::lock_guard<std::mutex> lock(ws_mutex);
                ws_connections.erase(std::remove(ws_connections.begin(), ws_connections.end(), &ws), ws_connections.end());
        }).onmessage([](crow::websocket::connection& ws, const std::string& msg, bool is_binary) {
                // Simple echo for demonstration
		nlohmann::json response;
		response["message"] = "Knocking on my door? Join the discord listed on the documentation page!";
		response["echo"] = msg;
		// Convert the JSON object to a string and send it.
		ws.send_text(response.dump());
        }).onerror([](crow::websocket::connection& ws, const std::string& error) {
                std::cerr << "WebSocket error!" << std::endl;
		nlohmann::json msg;
                msg["message"] = "Error! Contact owner of alpha-strike services.";
                // Now send the JSON string (using .dump() to serialize it)
                ws.send_text(msg.dump());
        });
        // Start the PostgreSQL listener in a separate thread.
        std::thread pg_listener(listen_notifications);
        // Force address and port
        app.bindaddr("0.0.0.0").port(8080).multithreaded().run();
        // In practice, you would handle shutdown more gracefully.
        pg_listener.join();
}
