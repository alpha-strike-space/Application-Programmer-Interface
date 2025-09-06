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
	// Check for environment variables.
	if (!dbname || !user || !password || !host || !port) {
		throw std::runtime_error("Database environment variables are not set. Please check your .env file.");
	}
	// Return the name
	return "dbname=" + std::string(dbname) +
		" user=" + std::string(user) +
		" password=" + std::string(password) +
		" host=" + std::string(host) +
		" port=" + std::string(port);
}
// Health route and all HTTP API routes here
void setupRoutes(crow::SimpleApp& app) {
	// Get server health, character count, and kill count
	CROW_ROUTE(app, "/health").methods("GET"_method)([]() {
		crow::json::wvalue response;
		response["health"] = "I'm alive!";
		// Try to get everything else for health
		try {
			pqxx::connection conn{get_pool_connection_string()};
			pqxx::work txn(conn);
			// Character count
			pqxx::result char_res = txn.exec("SELECT COUNT(*) FROM characters");
			int character_count = char_res[0][0].as<int>();
			response["player_count"] = character_count;
			// Incident count
			pqxx::result kill_res = txn.exec("SELECT COUNT(*) FROM incident");
			int kill_count = kill_res[0][0].as<int>();
			response["incident_count"] = kill_count;
			// Commit
			txn.commit();
		} catch (const std::exception &e) {
			// Log the error and return an error message.
			std::cerr << "Error: " << e.what() << std::endl;
			// Create response value.
			crow::json::wvalue error_response;
			error_response["error"] = "Internal Server Error!";
			return crow::response(500, error_response);
		}
		// Return response
		return crow::response(200, response);
	});
	// get characters
	CROW_ROUTE(app, "/characters").methods("GET"_method)([](const crow::request &req) -> crow::response {
		// Check methods applied.
		if (req.method != crow::HTTPMethod::Get) {
			// Wrong method sent.
			return crow::response(405);
		}
		// Try user input.
		try {
			pqxx::connection conn{get_pool_connection_string()};
			pqxx::work txn(conn);
			pqxx::result res;
			// Check for parameters by initializin a pointer for the url sent.
			const char* name_parameter = req.url_params.get("name");
			// Check the parameters every time we are called up.
			if (name_parameter) {
                // Parse our search value name_parameter
                std::string searchPattern = "%" + std::string(name_parameter) + "%";
				// The query
				std::string query = "SELECT "
					"    c.name, "
					"    encode(c.address, 'hex') AS character_address, "
					"    t.name AS tribe_name, "
					"    m.joined_at, "
					"    m.left_at "
					"FROM characters c "
					"LEFT JOIN character_tribe_membership m ON c.id = m.character_id "
					"LEFT JOIN tribes t ON m.tribe_id = t.id "
					"WHERE LOWER(c.name) LIKE LOWER($1) "
					"ORDER BY (m.left_at IS NULL) DESC, m.joined_at DESC";
				res = txn.exec_params(query, searchPattern);
                                // Check if query returned any rows.
                                if (res.size() == 0) {
                                        crow::json::wvalue error_response;
                                        error_response["error"] = "Bad Request! No character records found";
                                        return crow::response(400, error_response);
                                }
			} else {
				// Nothing found
				crow::json::wvalue error_response;
				error_response["error"] = "Missing parameter!";
				return crow::response(400, error_response);
			}
			// Transact
			txn.commit();
			// Build the JSON using nlohmann
			auto character_string = format_characters(res);
			// Convert JSON object to string
			std::string json_string = character_string.dump(4); // Pretty printing with indent of 4 spaces
			// Create a Crow response with the correct Content-Type header
			crow::response resp(json_string);
			resp.set_header("Content-Type", "application/json");
			return resp;
		} catch (const std::exception &e) {
			// Log the error and return an error message.
			std::cerr << "Erorr: " << e.what() << std::endl;
			// Create response value.
			crow::json::wvalue error_response;
			error_response["error"] = "Interal Server Error!";
			return crow::response(500, error_response);
		}
	});
	// get tribes
	CROW_ROUTE(app, "/tribes").methods("GET"_method)([](const crow::request &req) -> crow::response {
		// Check methods applied.
		if(req.method != crow::HTTPMethod::Get) {
			// Wrong method sent.
			return crow::response(405);
		}
		// Try user input.
		try {
			// Set up connections
			pqxx::connection conn{get_pool_connection_string()};
			pqxx::work txn(conn);
			pqxx::result res;
			// Check for parameters by initializing a pointer for the url sent.
			const char* name_parameter = req.url_params.get("name");
			// Check the parameters every time we are called up.
			if (name_parameter) {
				// Parse our search value name_parameter
				std::string searchPattern = "%" + std::string(name_parameter) + "%";
				// Prepare SQL Call.
				std::string query = "SELECT "
					"    t.id AS tribe_id, "
					"    t.name AS tribe_name, "
					"    t.url AS tribe_url, "
					"    c.name AS member_name, "
					"    (SELECT COUNT(*) "
					"    	 FROM character_tribe_membership m2 "
					"        WHERE m2.tribe_id = t.id AND m2.left_at IS NULL) AS member_count "
					"FROM tribes t "
					"LEFT JOIN character_tribe_membership m ON t.id = m.tribe_id AND m.left_at IS NULL "
					"LEFT JOIN characters c ON m.character_id = c.id "
					"WHERE LOWER(t.name) LIKE LOWER($1) "
					"ORDER BY t.id, c.name";
				res = txn.exec_params(query, searchPattern);
                                // Check if query returned any rows.
				if (res.size() == 0) {
					crow::json::wvalue error_response;
					error_response["error"] = "Bad Request! No tribe records found";
					return crow::response(400, error_response);
				}
				// Transact
				txn.commit();
				// Build the JSON using nlohmann
				auto tribe_string = format_tribe_membership(res);
				// Convert JSON object to string
				std::string json_string = tribe_string.dump(4); // Pretty printing with indent of 4 spaces.
				// Create a crow response with the correct Content-Type header.
				crow::response resp(json_string);
				resp.set_header("Content-Type", "application/json");
				return resp;
			} else {
				// Qeury
				std::string query = "SELECT "
					"    t.id AS tribe_id, "
					"    t.name AS tribe_name, "
					"    t.url AS tribe_url, "
					"    (SELECT COUNT(*) "
					"        FROM character_tribe_membership m "
					"        WHERE m.tribe_id = t.id AND m.left_at IS NULL) AS member_count "
					"FROM tribes t "
					"ORDER BY t.id";
				res = txn.exec(query);
                                // Check if query returned any rows.
                                if (res.size() == 0) {
                                        crow::json::wvalue error_response;
                                        error_response["error"] = "Bad Request! No tribe records found";
                                        return crow::response(400, error_response);
                                }
				// Transact
				txn.commit();
				// Build the JSON using nlohmann
				auto tribe_string = format_tribes(res);
				// Convert JSON object to string
				std::string json_string = tribe_string.dump(4); // Pretty printing with indent of 4 spaces
				// Create a Crow response with the correct Content-Type header
				crow::response resp(json_string);
				resp.set_header("Content-Type", "application/json");
				return resp;
			}
		} catch (const std::exception &e) {
			// Log the error and return an error message.
			std::cerr << "Erorr: " << e.what() << std::endl;
			// Create response value.
			crow::json::wvalue error_response;
			error_response["error"] = "Interal Server Error!";
			return crow::response(500, error_response);
		}
	});
	// get locations
	CROW_ROUTE(app, "/location").methods("GET"_method)([](const crow::request &req) -> crow::response {
		// Get Method
		if(req.method == crow::HTTPMethod::Get) {
			try {
				// Set up connections
				pqxx::connection conn{get_pool_connection_string()};
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
			pqxx::connection conn{get_pool_connection_string()};
			pqxx::work txn(conn);
			pqxx::result res;
			pqxx::result resKillers;
			pqxx::result resVictims;
			pqxx::result resSystems;
			pqxx::result resTribes;
			// Check for the parameters by initializing a pointer for the url sent.
			const char* name_parameter = req.url_params.get("name"); // name
			const char* system_parameter = req.url_params.get("system"); // system by id or name
			// Extract the "filter" parameter (e.g., "24h", "week", or "month")
			const char* filter_parameter = req.url_params.get("filter");
			const char* tribe_parameter = req.url_params.get("tribe"); // tribe
			// Build parameters
			// Helper lambda to build search pattern.
			auto build_search_pattern = [](const char* value) -> std::string {
				return std::string("%") + std::string(value) + "%";
			};
			// Create a time-based helper lambda to build the time-based clause.
			auto get_time_clause = [](const char* filter_param) -> std::string {
				// Check for filter value passed.
				if (!filter_param)
					return "";
				// Create string to hold passed value.
				std::string filterStr = filter_param;
				// Check our filter value.
				if (filterStr == "day") {
					return "i.time_stamp >= extract(epoch from now() - interval '24 hours')";
				} else if (filterStr == "week") {
					return "i.time_stamp >= extract(epoch from now() - interval '7 days')";
				} else if (filterStr == "month") {
					return "i.time_stamp >= extract(epoch from now() - interval '1 month')";
				} else {
					return "";
				}
			};
			// Check the parameters every time we are called up.
			if(name_parameter) {
				if(name_parameter && std::string(name_parameter).size() > 0) {
					// Parse our search value name parameter.
					std::string searchPattern = build_search_pattern(name_parameter);
					// Work with the filter.
					std::string timeClause = get_time_clause(filter_parameter);
					// Base query
					std::string baseQuery = "WITH combined AS ("
						"  SELECT killer.id AS char_id, killer.name AS person, "
						"         1 AS kill_count, 0 AS loss_count, "
						"         i.time_stamp "
						"  FROM incident i "
						"  JOIN characters killer ON i.killer_id = killer.id "
						"  UNION ALL "
						"  SELECT victim.id AS char_id, victim.name AS person, "
						"         0 AS kill_count, 1 AS loss_count, "
						"         i.time_stamp "
						"  FROM incident i "
						"  JOIN characters victim ON i.victim_id = victim.id "
						") "
						"SELECT c.person, "
						"       SUM(c.kill_count) AS total_kills, "
						"       SUM(c.loss_count) AS total_losses, "
						"       COALESCE(t.name, '') AS tribe_name "
						"FROM combined c "
						"LEFT JOIN character_tribe_membership m ON c.char_id = m.character_id AND m.left_at IS NULL "
						"LEFT JOIN tribes t ON m.tribe_id = t.id "
						"WHERE c.person ILIKE $1 ";  //The parameter placeholder for our name search.;
					// Empty query string.
					std::string query;
					// Append filters
					if (!timeClause.empty()) {
						baseQuery += "AND " + timeClause + " ";
					}
					query = baseQuery + "GROUP BY c.person, t.name;";
					// Prepare SQL call.
					res = txn.exec_params(query, searchPattern);
				} else {
					// Work with the filter.
					std::string timeClause = get_time_clause(filter_parameter);
					// Base query
					std::string query = "WITH combined AS ("
						"  SELECT killer.id AS char_id, killer.name AS person, 1 AS kill_count, 0 AS loss_count, i.time_stamp "
						"  FROM incident i "
						"  JOIN characters killer ON i.killer_id = killer.id "
						+ (timeClause.empty() ? "" : "WHERE " + timeClause + " ") +
						"  UNION ALL "
						"  SELECT victim.id AS char_id, victim.name AS person, 0 AS kill_count, 1 AS loss_count, i.time_stamp "
						"  FROM incident i "
						"  JOIN characters victim ON i.victim_id = victim.id "
						+ (timeClause.empty() ? "" : "WHERE " + timeClause + " ") +
						") "
						"SELECT agg.person, "
						"       SUM(agg.kill_count) AS total_kills, "
						"       SUM(agg.loss_count) AS total_losses, "
						"       COALESCE(t.name, '') AS tribe_name "
						"FROM combined agg "
						"LEFT JOIN character_tribe_membership m ON agg.char_id = m.character_id AND m.left_at IS NULL "
						"LEFT JOIN tribes t ON m.tribe_id = t.id "
						"GROUP BY agg.person, t.name "
						"ORDER BY total_kills DESC;";
					// Run the query (no exec_params needed since no placeholders)
					res = txn.exec(query);
				}
				// Check if query returned any rows.
				if (res.size() == 0) {
					crow::json::wvalue error_response;
					error_response["error"] = "Bad Request! No name records found!";
					return crow::response(400, error_response);
				}
				// Build the JSON using nlohmann
				auto name_string = format_top_names(res);
				// Convert JSON object to string
				std::string json_string = name_string.dump(4); // Pretty printing with indent of 4 spaces
				// Create a Crow response with the correct Content-Type header
				crow::response resp(json_string);
				resp.set_header("Content-Type", "application/json");
				return resp;
			} else if(system_parameter) {
				if(system_parameter && std::string(system_parameter).size() > 0) {
					// Convert the string to a 64-bit integer using std::stoll.
					std::string searchPattern = build_search_pattern(system_parameter);
					// Work with the filter.
					std::string timeClause = get_time_clause(filter_parameter);
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
					// Prepare SQL call.
					res = txn.exec_params(query, searchPattern);
				} else {
					// Work with the filter.
					std::string timeClause = get_time_clause(filter_parameter);
					// Empty query string, with a filter for systems.
					std::string baseQuery = "SELECT s.solar_system_id, s.solar_system_name, COUNT(*) AS incident_count "
						"FROM incident i "
						"JOIN systems s ON i.solar_system_id = s.solar_system_id";
					// Empty query string.
					std::string query;
					// If get_time_clause returns a non-empty string, add WHERE before it.
					if (!timeClause.empty()) {
						baseQuery += " WHERE " + timeClause + " ";
					}
					query = baseQuery + " GROUP BY s.solar_system_id, s.solar_system_name "
						"ORDER BY incident_count DESC;";
					// Prepare SQL call.
					res = txn.exec(query);
				}
				// Check if query returned any rows.
				if (res.size() == 0) {
					crow::json::wvalue error_response;
					error_response["error"] = "Bad Request! No system records found";
					return crow::response(400, error_response);
				}
				// Reuse formatTopSystems since returns are the same with nlohmann.
				auto systems_string = format_top_systems(res);
				// Convert JSON object to string
				std::string json_string = systems_string.dump(4); // Pretty printing with indent of 4 spaces
				// Create a Crow response with the correct Content-Type header
				crow::response resp(json_string);
				resp.set_header("Content-Type", "application/json");
				return resp;
			} else if(tribe_parameter) {
				// Check the parameters every time we are called up.
				if(tribe_parameter && std::string(tribe_parameter).size() > 0) {
					// Parse our search value tribe parameter.
					std::string searchPattern = build_search_pattern(tribe_parameter);
					// Work with the filter.
					std::string timeClause = get_time_clause(filter_parameter);
					// Base query for tribe stats with name filter
					std::string baseQuery = "SELECT t.name AS tribe_name, "
						"COALESCE(kills_table.kills, 0) AS kills, "
						"COALESCE(losses_table.losses, 0) AS losses "
						"FROM tribes t "
						"LEFT JOIN ("
						"  SELECT ctm.tribe_id, COUNT(*) AS kills "
						"  FROM incident i "
						"  JOIN characters c ON i.killer_id = c.id "
						"  JOIN character_tribe_membership ctm ON ctm.character_id = c.id AND ctm.joined_at <= i.time_stamp AND (ctm.left_at IS NULL OR ctm.left_at > i.time_stamp) "
						+ (timeClause.empty() ? "" : "WHERE " + timeClause + " ") +
						"  GROUP BY ctm.tribe_id "
						") kills_table ON t.id = kills_table.tribe_id "
						"LEFT JOIN ("
						"  SELECT ctm.tribe_id, COUNT(*) AS losses "
						"  FROM incident i "
						"  JOIN characters c ON i.victim_id = c.id "
						"  JOIN character_tribe_membership ctm ON ctm.character_id = c.id AND ctm.joined_at <= i.time_stamp AND (ctm.left_at IS NULL OR ctm.left_at > i.time_stamp) "
						+ (timeClause.empty() ? "" : "WHERE " + timeClause + " ") +
						"  GROUP BY ctm.tribe_id "
						") losses_table ON t.id = losses_table.tribe_id "
						"WHERE t.name ILIKE $1 "
						"ORDER BY kills DESC";
					// Prepare SQL call.
					res = txn.exec_params(baseQuery, searchPattern);
				} else {
					std::string timeClause = get_time_clause(filter_parameter);
					std::string baseQuery = "SELECT t.name AS tribe_name, "
						"COALESCE(kills_table.kills, 0) AS kills, "
						"COALESCE(losses_table.losses, 0) AS losses "
						"FROM tribes t "
						"LEFT JOIN ("
						"  SELECT ctm.tribe_id, COUNT(*) AS kills "
						"  FROM incident i "
						"  JOIN characters c ON i.killer_id = c.id "
						"  JOIN character_tribe_membership ctm ON ctm.character_id = c.id AND ctm.joined_at <= i.time_stamp AND (ctm.left_at IS NULL OR ctm.left_at > i.time_stamp) "
						+ (timeClause.empty() ? "" : "WHERE " + timeClause + " ") +
						"  GROUP BY ctm.tribe_id "
						") kills_table ON t.id = kills_table.tribe_id "
						"LEFT JOIN ("
						"  SELECT ctm.tribe_id, COUNT(*) AS losses "
						"  FROM incident i "
						"  JOIN characters c ON i.victim_id = c.id "
						"  JOIN character_tribe_membership ctm ON ctm.character_id = c.id AND ctm.joined_at <= i.time_stamp AND (ctm.left_at IS NULL OR ctm.left_at > i.time_stamp) "
						+ (timeClause.empty() ? "" : "WHERE " + timeClause + " ") +
						"  GROUP BY ctm.tribe_id "
						") losses_table ON t.id = losses_table.tribe_id "
						"ORDER BY kills DESC";
					res = txn.exec(baseQuery);
				}
				if (res.size() == 0) {
					crow::json::wvalue error_response;
					error_response["error"] = "Bad Request! No tribe records found!";
					return crow::response(400, error_response);
				}
				auto name_string = format_top_tribes(res);
				std::string json_string = name_string.dump(4);
				crow::response resp(json_string);
				resp.set_header("Content-Type", "application/json");
				return resp;
			} else {
				std::string timeClause = get_time_clause(filter_parameter);
				std::string qKillers = "SELECT killer.name AS name, COUNT(*) AS incident_count "
					"FROM incident i "
					"JOIN characters killer ON i.killer_id = killer.id "
					"WHERE killer.name <> '' ";
				if (!timeClause.empty()) {
					qKillers += "AND " + timeClause + " ";
				}
				qKillers += "GROUP BY killer.name ORDER BY incident_count DESC LIMIT 10;";
				std::string qVictims = "SELECT victim.name AS name, COUNT(*) AS incident_count "
					"FROM incident i "
					"JOIN characters victim ON i.victim_id = victim.id "
					"WHERE victim.name <> '' ";
				if (!timeClause.empty()) {
					qVictims += "AND " + timeClause + " ";
				}
				qVictims += "GROUP BY victim.name ORDER BY incident_count DESC LIMIT 10;";
				std::string qSystems = "SELECT s.solar_system_id, s.solar_system_name, COUNT(*) AS incident_count "
					"FROM incident i "
					"JOIN systems s ON i.solar_system_id = s.solar_system_id ";
				if (!timeClause.empty()) {
					qSystems += "WHERE " + timeClause + " ";
				}
				qSystems += "GROUP BY s.solar_system_id, s.solar_system_name ORDER BY incident_count DESC LIMIT 10;";
				std::string qTribes = "SELECT t.name AS tribe_name, "
					"COALESCE(kills_table.kills, 0) AS kills, "
					"COALESCE(losses_table.losses, 0) AS losses "
					"FROM tribes t "
					"LEFT JOIN ("
					"  SELECT ctm.tribe_id, COUNT(*) AS kills "
					"  FROM incident i "
					"  JOIN characters c ON i.killer_id = c.id "
					"  JOIN character_tribe_membership ctm ON ctm.character_id = c.id AND ctm.joined_at <= i.time_stamp AND (ctm.left_at IS NULL OR ctm.left_at > i.time_stamp) "
					+ (timeClause.empty() ? "" : "WHERE " + timeClause + " ") +
					"  GROUP BY ctm.tribe_id "
					") kills_table ON t.id = kills_table.tribe_id "
					"LEFT JOIN ("
					"  SELECT ctm.tribe_id, COUNT(*) AS losses "
					"  FROM incident i "
					"  JOIN characters c ON i.victim_id = c.id "
					"  JOIN character_tribe_membership ctm ON ctm.character_id = c.id AND ctm.joined_at <= i.time_stamp AND (ctm.left_at IS NULL OR ctm.left_at > i.time_stamp) "
					+ (timeClause.empty() ? "" : "WHERE " + timeClause + " ") +
					"  GROUP BY ctm.tribe_id "
					") losses_table ON t.id = losses_table.tribe_id "
					"ORDER BY kills DESC, losses DESC LIMIT 10;";
				resKillers = txn.exec(qKillers);
				resVictims = txn.exec(qVictims);
				resSystems  = txn.exec(qSystems);
				resTribes = txn.exec(qTribes);
				auto topKillers = format_top_killers(resKillers);
				auto topVictims = format_top_victims(resVictims);
				auto topSystems = format_top_systems(resSystems);
				auto topTribes = format_top_tribes(resTribes);
				nlohmann::ordered_json response;
				response["top_killers"] = topKillers;
				response["top_victims"] = topVictims;
				response["top_systems"] = topSystems;
				response["top_tribes"] = topTribes;
				std::string json_string = response.dump(4);
				crow::response resp(json_string);
				resp.set_header("Content-Type", "application/json");
				return resp;
			}
		} catch(const std::exception& e) {
			std::cerr << "Notification error: " << e.what() << "\n";
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
				pqxx::connection conn{get_pool_connection_string()};
				pqxx::work txn(conn);
				pqxx::result res;
				// Check for the parameters by initializing a pointer for the url sent.
				const char* name_parameter = req.url_params.get("name"); // name
				const char* system_parameter = req.url_params.get("system"); // system by id or name
				const char* filter_parameter = req.url_params.get("filter");
				const char* mail_parameter = req.url_params.get("mail_id");
				const char* tribe_parameter = req.url_params.get("tribe");
				int limit = req.url_params.get("limit") ? std::stoi(req.url_params.get("limit")) : 100;
				int offset = req.url_params.get("offset") ? std::stoi(req.url_params.get("offset")) : 0;
				auto build_search_pattern = [](const char* value) -> std::string {
					return std::string("%") + std::string(value) + "%";
				};
				auto get_time_clause = [](const char* filter_param) -> std::string {
					if (!filter_param)
						return "";
					std::string filterStr = filter_param;
					if (filterStr == "day") {
						return "i.time_stamp >= extract(epoch from now() - interval '24 hours')";
					} else if (filterStr == "week") {
						return "i.time_stamp >= extract(epoch from now() - interval '7 days')";
					} else if (filterStr == "month") {
						return "i.time_stamp >= extract(epoch from now() - interval '1 month')";
					} else {
						return "";
					}
				};
				if(name_parameter) {
					std::string searchPattern = build_search_pattern(name_parameter);
					std::string timeClause = get_time_clause(filter_parameter);
					std::string baseQuery = "SELECT i.id, "
						"COALESCE(victim.name, '') AS victim_name, "
						"COALESCE(encode(victim.address, 'hex'), '') AS victim_address, "
						"COALESCE(victim_tribe.name, '') AS victim_tribe_name, "
						"COALESCE(killer.name, '') AS killer_name, "
						"COALESCE(encode(killer.address, 'hex'), '') AS killer_address, "
						"COALESCE(killer_tribe.name, '') AS killer_tribe_name, "
						"i.solar_system_id, "
						"s.solar_system_name, "
						"i.loss_type, "
						"i.time_stamp "
						"FROM incident AS i "
						"JOIN characters AS victim ON i.victim_id = victim.id "
						"LEFT JOIN character_tribe_membership victim_ctm ON victim_ctm.character_id = victim.id AND victim_ctm.joined_at <= i.time_stamp AND (victim_ctm.left_at IS NULL OR victim_ctm.left_at > i.time_stamp) "
						"LEFT JOIN tribes AS victim_tribe ON victim_ctm.tribe_id = victim_tribe.id "
						"JOIN characters AS killer ON i.killer_id = killer.id "
						"LEFT JOIN character_tribe_membership killer_ctm ON killer_ctm.character_id = killer.id AND killer_ctm.joined_at <= i.time_stamp AND (killer_ctm.left_at IS NULL OR killer_ctm.left_at > i.time_stamp) "
						"LEFT JOIN tribes AS killer_tribe ON killer_ctm.tribe_id = killer_tribe.id "
						"JOIN systems AS s ON i.solar_system_id = s.solar_system_id "
						"WHERE (victim.name ILIKE $1 OR killer.name ILIKE $1)";
					std::string query;
					if(!timeClause.empty()) {
						query = baseQuery + " AND " + timeClause + " ORDER BY i.time_stamp DESC, i.id DESC LIMIT $2 OFFSET $3;";
					} else {
						query = baseQuery + " ORDER BY i.time_stamp DESC, i.id DESC LIMIT $2 OFFSET $3;";
					}
					res = txn.exec_params(query, searchPattern, limit, offset);
					if (res.size() == 0) {
						crow::json::wvalue error_response;
						error_response["error"] = "Bad Request! No incident records found!";
						return crow::response(400, error_response);
					}
				} else if(system_parameter) {
					std::string searchPattern = build_search_pattern(system_parameter);
					std::string timeClause = get_time_clause(filter_parameter);
					std::string baseQuery = "SELECT i.id, "
						"COALESCE(victim.name, '') AS victim_name, "
						"COALESCE(encode(victim.address, 'hex'), '') AS victim_address, "
						"COALESCE(victim_tribe.name, '') AS victim_tribe_name, "
						"COALESCE(killer.name, '') AS killer_name, "
						"COALESCE(encode(killer.address, 'hex'), '') AS killer_address, "
						"COALESCE(killer_tribe.name, '') AS killer_tribe_name, "
						"i.solar_system_id, "
						"s.solar_system_name, "
						"i.loss_type, "
						"i.time_stamp "
						"FROM incident AS i "
						"JOIN characters AS victim ON i.victim_id = victim.id "
						"LEFT JOIN character_tribe_membership victim_ctm ON victim_ctm.character_id = victim.id AND victim_ctm.joined_at <= i.time_stamp AND (victim_ctm.left_at IS NULL OR victim_ctm.left_at > i.time_stamp) "
						"LEFT JOIN tribes AS victim_tribe ON victim_ctm.tribe_id = victim_tribe.id "
						"JOIN characters AS killer ON i.killer_id = killer.id "
						"LEFT JOIN character_tribe_membership killer_ctm ON killer_ctm.character_id = killer.id AND killer_ctm.joined_at <= i.time_stamp AND (killer_ctm.left_at IS NULL OR killer_ctm.left_at > i.time_stamp) "
						"LEFT JOIN tribes AS killer_tribe ON killer_ctm.tribe_id = killer_tribe.id "
						"JOIN systems AS s ON i.solar_system_id = s.solar_system_id "
						"WHERE (i.solar_system_id::text ILIKE $1 OR s.solar_system_name ILIKE $1)";
					std::string query;
					if(!timeClause.empty()) {
						query = baseQuery + " AND " + timeClause + " ORDER BY i.time_stamp DESC, i.id DESC LIMIT $2 OFFSET $3;";
					} else {
						query = baseQuery + " ORDER BY i.time_stamp DESC, i.id DESC LIMIT $2 OFFSET $3;";
					}
					res = txn.exec_params(query, searchPattern, limit, offset);
					if (res.size() == 0) {
						crow::json::wvalue error_response;
						error_response["error"] = "Bad Request! No incident records found";
						return crow::response(400, error_response);
					}
				} else if(mail_parameter) {
					int searchPattern;
					try {
						searchPattern = std::stoi(std::string(mail_parameter));
					} catch (const std::exception& e) {
						return crow::response(400, "Invalid 'id' parameter");
					}
					std::string query = "SELECT i.id, "
						"COALESCE(victim_tribe.name, '') AS victim_tribe_name, "
						"COALESCE(encode(victim.address, 'hex'), '') AS victim_address, "
						"COALESCE(victim.name, '') AS victim_name, "
						"COALESCE(killer_tribe.name, '') AS killer_tribe_name, "
						"COALESCE(encode(killer.address, 'hex'), '') AS killer_address, "
						"COALESCE(killer.name, '') AS killer_name, "
						"i.solar_system_id, "
						"s.solar_system_name, "
						"i.loss_type, "
						"i.time_stamp "
						"FROM incident AS i "
						"JOIN systems AS s ON i.solar_system_id = s.solar_system_id "
						"LEFT JOIN characters victim ON i.victim_id = victim.id "
						"LEFT JOIN character_tribe_membership victim_ctm ON victim_ctm.character_id = victim.id AND victim_ctm.joined_at <= i.time_stamp AND (victim_ctm.left_at IS NULL OR victim_ctm.left_at > i.time_stamp) "
						"LEFT JOIN tribes victim_tribe ON victim_ctm.tribe_id = victim_tribe.id "
						"LEFT JOIN characters killer ON i.killer_id = killer.id "
						"LEFT JOIN character_tribe_membership killer_ctm ON killer_ctm.character_id = killer.id AND killer_ctm.joined_at <= i.time_stamp AND (killer_ctm.left_at IS NULL OR killer_ctm.left_at > i.time_stamp) "
						"LEFT JOIN tribes killer_tribe ON killer_ctm.tribe_id = killer_tribe.id "
						"WHERE i.id = $1;";
					res = txn.exec_params(query, searchPattern);
					if (res.size() == 0) {
						crow::json::wvalue error_response;
						error_response["error"] = "Bad Request! No incident records found";
						return crow::response(400, error_response);
					}
				} else if(tribe_parameter) {
					std::string searchPattern = build_search_pattern(tribe_parameter);
					std::string timeClause = get_time_clause(filter_parameter);
					std::string baseQuery =	"SELECT i.id, "
						"COALESCE(victim_tribe.name, '') AS victim_tribe_name, "
						"COALESCE(encode(victim.address, 'hex'), '') AS victim_address, "
						"COALESCE(victim.name, '') AS victim_name, "
						"COALESCE(killer_tribe.name, '') AS killer_tribe_name, "
						"COALESCE(encode(killer.address, 'hex'), '') AS killer_address, "
						"COALESCE(killer.name, '') AS killer_name, "
						"i.solar_system_id, "
						"s.solar_system_name, "
						"i.loss_type, "
						"i.time_stamp "
						"FROM incident AS i "
						"JOIN systems AS s ON i.solar_system_id = s.solar_system_id "
						"LEFT JOIN characters victim ON i.victim_id = victim.id "
						"LEFT JOIN tribes victim_tribe ON victim.tribe_id = victim_tribe.id "
						"LEFT JOIN characters killer ON i.killer_id = killer.id "
						"LEFT JOIN tribes killer_tribe ON killer.tribe_id = killer_tribe.id "
						"WHERE (killer_tribe.name ILIKE $1 OR victim_tribe.name ILIKE $1)";
					std::string query;
					if(!timeClause.empty()) {
						query = baseQuery + " AND " + timeClause + " ORDER BY i.time_stamp DESC, i.id DESC LIMIT $2 OFFSET $3;";
					} else {
						query = baseQuery + " ORDER BY i.time_stamp DESC, i.id DESC LIMIT $2 OFFSET $3;";
					}
					res = txn.exec_params(query, searchPattern, limit, offset);
					if (res.size() == 0) {
						crow::json::wvalue error_response;
						error_response["error"] = "Bad Request! No incident records found";
						return crow::response(400, error_response);
					}
				} else if(filter_parameter) {
					std::string timeClause = get_time_clause(filter_parameter);
					std::string baseQuery = "SELECT i.id, "
						"COALESCE(victim.name, '') AS victim_name, "
						"COALESCE(encode(victim.address, 'hex'), '') AS victim_address, "
						"COALESCE(victim_tribe.name, '') AS victim_tribe_name, "
						"COALESCE(killer.name, '') AS killer_name, "
						"COALESCE(encode(killer.address, 'hex'), '') AS killer_address, "
						"COALESCE(killer_tribe.name, '') AS killer_tribe_name, "
						"i.solar_system_id, "
						"s.solar_system_name, "
						"i.loss_type, "
						"i.time_stamp "
						"FROM incident AS i "
						"JOIN characters AS victim ON i.victim_id = victim.id "
						"LEFT JOIN character_tribe_membership victim_ctm ON victim_ctm.character_id = victim.id AND victim_ctm.joined_at <= i.time_stamp AND (victim_ctm.left_at IS NULL OR victim_ctm.left_at > i.time_stamp) "
						"LEFT JOIN tribes AS victim_tribe ON victim_ctm.tribe_id = victim_tribe.id "
						"JOIN characters AS killer ON i.killer_id = killer.id "
						"LEFT JOIN character_tribe_membership killer_ctm ON killer_ctm.character_id = killer.id AND killer_ctm.joined_at <= i.time_stamp AND (killer_ctm.left_at IS NULL OR killer_ctm.left_at > i.time_stamp) "
						"LEFT JOIN tribes AS killer_tribe ON killer_ctm.tribe_id = killer_tribe.id "
						"JOIN systems AS s ON i.solar_system_id = s.solar_system_id";
					std::string query;
					if(!timeClause.empty()) {
						query = baseQuery + " AND " + timeClause + " ORDER BY i.time_stamp DESC, i.id DESC LIMIT $1 OFFSET $2;";
					} else {
						query = baseQuery + " ORDER BY i.time_stamp DESC, i.id DESC LIMIT $1 OFFSET $2;";
					}
					res = txn.exec_params(query, limit, offset);
					if (res.size() == 0) {
						crow::json::wvalue error_response;
						error_response["error"] = "Bad Request! No incident records found";
						return crow::response(400, error_response);
					}
				} else {
					std::string query = "SELECT i.id, "
						"COALESCE(victim.name, '') AS victim_name, "
						"COALESCE(encode(victim.address, 'hex'), '') AS victim_address, "
						"COALESCE(victim_tribe.name, '') AS victim_tribe_name, "
						"COALESCE(killer.name, '') AS killer_name, "
						"COALESCE(encode(killer.address, 'hex'), '') AS killer_address, "
						"COALESCE(killer_tribe.name, '') AS killer_tribe_name, "
						"i.solar_system_id, "
						"s.solar_system_name, "
						"i.loss_type, "
						"i.time_stamp "
						"FROM incident AS i "
						"JOIN characters AS victim ON i.victim_id = victim.id "
						"LEFT JOIN character_tribe_membership victim_ctm ON victim_ctm.character_id = victim.id AND victim_ctm.joined_at <= i.time_stamp AND (victim_ctm.left_at IS NULL OR victim_ctm.left_at > i.time_stamp) "
						"LEFT JOIN tribes AS victim_tribe ON victim_ctm.tribe_id = victim_tribe.id "
						"JOIN characters AS killer ON i.killer_id = killer.id "
						"LEFT JOIN character_tribe_membership killer_ctm ON killer_ctm.character_id = killer.id AND killer_ctm.joined_at <= i.time_stamp AND (killer_ctm.left_at IS NULL OR killer_ctm.left_at > i.time_stamp) "
						"LEFT JOIN tribes AS killer_tribe ON killer_ctm.tribe_id = killer_tribe.id "
						"JOIN systems AS s ON i.solar_system_id = s.solar_system_id "
						"ORDER BY i.time_stamp DESC, i.id DESC LIMIT $1 OFFSET $2;";
					res = txn.exec_params(query, limit, offset);
					if (res.size() == 0) {
						crow::json::wvalue error_response;
						error_response["error"] = "Bad Request! No incident records found";
						return crow::response(400, error_response);
					}
				}
				txn.commit();
				auto incident_string = build_incident_json(res);
				std::string json_string = incident_string.dump(4);
				crow::response resp(json_string);
				resp.set_header("Content-Type", "application/json");
				return resp;
			} catch(const std::exception& e) {
				std::cerr << "Notification error: " << e.what() << "\n";
				crow::json::wvalue error_response;
				error_response["error"] = "Internal Server Error!";
				return crow::response(500, error_response);
			} catch (const std::out_of_range& e) {
				crow::json::wvalue error_response;
				error_response["error"] = "Bad Request! Parameter value out of range for limit, or offset!";
				return crow::response(400, error_response);
			}
		} else {
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
		ws.send_text(msg.dump());
	}).onclose([](crow::websocket::connection& ws, const std::string& reason, uint16_t close_code) {
		std::cout << "WebSocket connection closed!" << std::endl;
		std::lock_guard<std::mutex> lock(ws_mutex);
		ws_connections.erase(std::remove(ws_connections.begin(), ws_connections.end(), &ws), ws_connections.end());
	}).onmessage([](crow::websocket::connection& ws, const std::string& msg, bool is_binary) {
		nlohmann::json response;
		response["message"] = "Knocking on my door? Join the discord listed on the documentation page!";
		response["echo"] = msg;
		ws.send_text(response.dump());
	}).onerror([](crow::websocket::connection& ws, const std::string& error) {
		std::cerr << "WebSocket error!" << std::endl;
		nlohmann::json msg;
		msg["message"] = "Error! Contact owner of alpha-strike services.";
		ws.send_text(msg.dump());
	});
}
