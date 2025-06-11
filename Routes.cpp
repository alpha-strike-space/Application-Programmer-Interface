#include "Routes.h"
#include "Serializer.h"
#include <pqxx/pqxx>
#include <iostream>
#include <nlohmann/json.hpp>
#include <cstdlib> // For getenv
#include <string>
// Pooled Connection
std::string get_pool_connection_string() {
    const char* dbname = std::getenv("PGBOUNCER_DB");
    const char* user = std::getenv("PGBOUNCER_USER");
    const char* password = std::getenv("PGBOUNCER_PASSWORD");
    const char* host = std::getenv("PGBOUNCER_HOST");
    const char* port = std::getenv("PGBOUNCER_PORT");

    if (!dbname || !user || !password || !host || !port) {
        throw std::runtime_error("Database environment variables are not set. Please check your .env file.");
    }

    return "dbname=" + std::string(dbname) +
           " user=" + std::string(user) +
           " password=" + std::string(password) +
           " host=" + std::string(host) +
           " port=" + std::string(port);
}
// Health route and all HTTP API routes here
void setupRoutes(crow::SimpleApp& app) {
        // Get server health
        CROW_ROUTE(app, "/health").methods("GET"_method)([]() {
                // Create response value.
		crow::json::wvalue response;
		response["Health"] = "I'm alive!";
                return crow::response(200, response);
        });
        // get locations
        CROW_ROUTE(app, "/location").methods("GET"_method)([](const crow::request &req) -> crow::response {
                // Get Method
                if(req.method == crow::HTTPMethod::Get) {
                        try {
                                pqxx::connection conn{get_connection_string()};
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
				// Create response value.
				crow::json::wvalue error_response;
				error_response["error"] = "Internal Server Error!";
                                return crow::response(500, error_response);
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
                                pqxx::connection conn{get_connection_string()};
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
				// Create response value.
				crow::json::wvalue error_response;
				error_response["error"] = "Internal Server Error!";
                                return crow::response(500, error_response);
		}
	});
        // Get incidents
        CROW_ROUTE(app, "/incident").methods("GET"_method)([](const crow::request &req) -> crow::response {
                // Get Method
                if(req.method == crow::HTTPMethod::Get) {
                        try {
                                // Get your PostgreSQL connection
                                pqxx::connection conn{get_connection_string()};
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
				// Create response value.
				crow::json::wvalue error_response;
				error_response["error"] = "Internal Server Error!";
                                return crow::response(500, error_response);
                        }
                } else {
                        // Send error for method issues. This is where our post function will go when the Eve Frontier API is updated.
                        return crow::response(405);
                }
            });
}
// Websocket
#include <mutex>
#include <vector>
// Make sure these are declared
std::vector<crow::websocket::connection*> ws_connections;
std::mutex ws_mutex;

void setupWebSocket(crow::SimpleApp& app) {
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
}
