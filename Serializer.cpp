#include "Serializer.h"
// Build incident Json
nlohmann::ordered_json build_incident_json(const pqxx::result& res) {
    nlohmann::ordered_json json_array = nlohmann::ordered_json::array();
    for (const auto& row : res) {
        nlohmann::ordered_json item;
        item["id"] = row["id"].as<long long>();
        item["victim_address"] = row["victim_address"].as<std::string>();
        item["victim_name"] = row["victim_name"].as<std::string>();
        item["loss_type"] = row["loss_type"].as<std::string>();
        item["killer_address"] = row["killer_address"].as<std::string>();
        item["killer_name"] = row["killer_name"].as<std::string>();
        item["time_stamp"] = row["time_stamp"].as<long long>();
        item["solar_system_id"] = row["solar_system_id"].as<long long>();
        item["solar_system_name"] = row["solar_system_name"].as<std::string>();
        json_array.push_back(std::move(item));
    }
    return json_array;
}
// Build system json
nlohmann::ordered_json build_system_json(const pqxx::result& res) {
    nlohmann::ordered_json json_array = nlohmann::ordered_json::array();
    for (const auto& row : res) {
        nlohmann::ordered_json item;
        item["solar_system_id"] = row["solar_system_id"].as<long long>();
        item["solar_system_name"] = row["solar_system_name"].as<std::string>();
        item["coordinates"] = {
            {"x", row["x"].as<std::string>()},
            {"y", row["y"].as<std::string>()},
            {"z", row["z"].as<std::string>()}
        };
        json_array.push_back(std::move(item));
    }
    return json_array;
}
// Format the json
nlohmann::ordered_json format_name(const pqxx::result& resName) {
    nlohmann::ordered_json jsonResponse = nlohmann::ordered_json::array();
    for (const auto& row : resName) {
        nlohmann::ordered_json item;
        item["name"] = row["person"].as<std::string>();
        item["total_kills"] = row["total_kills"].as<int>();
        item["total_losses"] = row["total_losses"].as<int>();
        jsonResponse.push_back(item);
    }
    return jsonResponse;
}
// Format top killers
nlohmann::ordered_json format_top_killers(const pqxx::result& resKillers) {
    nlohmann::ordered_json topKillers = nlohmann::ordered_json::array();
    for (const auto& row : resKillers) {
        nlohmann::ordered_json item;
        item["name"] = row["name"].as<std::string>();
        item["kills"] = row["incident_count"].as<int>();
        topKillers.push_back(item);
    }
    return topKillers;
}
// Format top victims
nlohmann::ordered_json format_top_victims(const pqxx::result& resVictims) {
    nlohmann::ordered_json topVictims = nlohmann::ordered_json::array();
    for (const auto& row : resVictims) {
        nlohmann::ordered_json item;
        item["name"] = row["name"].as<std::string>();
        item["losses"] = row["incident_count"].as<int>();
        topVictims.push_back(item);
    }
    return topVictims;
}
// Format systems
nlohmann::ordered_json format_systems(const pqxx::result& resSystems) {
    nlohmann::ordered_json topSystems = nlohmann::ordered_json::array();
    for (const auto& row : resSystems) {
        nlohmann::ordered_json item;
        item["solar_system_id"] = row["solar_system_id"].as<std::string>();
        item["solar_system_name"] = row["solar_system_name"].as<std::string>();
        item["incident_count"] = row["incident_count"].as<int>();
        topSystems.push_back(item);
    }
    return topSystems;
}
