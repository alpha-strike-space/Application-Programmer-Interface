#include "Serializer.h"
// Build incident json
nlohmann::ordered_json build_incident_json(const pqxx::result& res) {
    nlohmann::ordered_json json_array = nlohmann::ordered_json::array();
    for (const auto& row : res) {
        nlohmann::ordered_json item;
        item["id"] = row["id"].as<long long>();
        //item["victim_tribe_name"] = row["victim_tribe_name"].as<std::string>(); // Empty if none presented
	item["victim_tribe_name"] = row["victim_tribe_name"].as<std::string>().empty() ? "NONE" : row["victim_tribe_name"].as<std::string>();
	item["victim_address"] = row["victim_address"].as<std::string>();
        item["victim_name"] = row["victim_name"].as<std::string>();
	// Hard write "ship" if loss_type is 0
	item["loss_type"] = (row["loss_type"].as<int>() == 0) ? "ship" : row["loss_type"].as<std::string>();
        //item["loss_type"] = row["loss_type"].as<std::string>();
        item["killer_tribe_name"] = row["killer_tribe_name"].as<std::string>().empty() ? "NONE" : row["killer_tribe_name"].as<std::string>();
	//item["killer_tribe_name"] = row["killer_tribe_name"].as<std::string>(); // Empty if none presented
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
// Format the name json
nlohmann::ordered_json format_top_names(const pqxx::result& resName) {
    nlohmann::ordered_json json_array = nlohmann::ordered_json::array();
    for (const auto& row : resName) {
        nlohmann::ordered_json item;
        item["name"] = row["person"].as<std::string>();
	item["tribe_name"] = row["tribe_name"].as<std::string>();
        item["total_kills"] = row["total_kills"].as<int>();
        item["total_losses"] = row["total_losses"].as<int>();
        json_array.push_back(item);
    }
    return json_array;
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
// Format top systems
nlohmann::ordered_json format_top_systems(const pqxx::result& resSystems) {
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
// Format top tribes
nlohmann::ordered_json format_top_tribes(const pqxx::result& resTribes) {
    nlohmann::ordered_json topTribes = nlohmann::ordered_json::array();
    for (const auto& row : resTribes) {
        nlohmann::ordered_json item;
        item["tribe_name"] = row["tribe_name"].as<std::string>();
        item["total_kills"] = row["kills"].as<long long>();
        item["total_losses"] = row["losses"].as<long long>();
        topTribes.push_back(item);
    }
    return topTribes;
}
// Format tribe characters
nlohmann::ordered_json format_tribe_membership(const pqxx::result& resTribes) {
    nlohmann::ordered_json tribe_json;
    std::vector<std::string> members;
    // Check if empty.
    if (resTribes.size() == 0) {
	nlohmann::json error_json;
	error_json["error"] = "Not found!";
        return error_json; // Just in case
    }
    // Use the first row for tribe_id, tribe_name, tribe_url
    const auto& first_row = resTribes[0];
    tribe_json["tribe_id"] = first_row["tribe_id"].as<long long>();
    tribe_json["tribe_name"] = first_row["tribe_name"].as<std::string>();
    tribe_json["tribe_url"] = first_row["tribe_url"].as<std::string>();
    // Run through all names that are members for display.
    for (const auto& row : resTribes) {
        members.push_back(row["member_name"].as<std::string>());
    }
    // Check to see if we are represented.
    if (!members.empty()) {
        tribe_json["members"] = members;
	tribe_json["member_count"] = first_row["member_count"].as<long long>(); 
    } else {
        tribe_json["members"] = "No members found";
    }
    //tribe_json["members"] = members;
    // Return the json
    return tribe_json;
}
// Format tribe information without membership listing
nlohmann::ordered_json format_tribes(const pqxx::result& resTribes) {
    nlohmann::ordered_json json_array = nlohmann::ordered_json::array();
    // Tribes without members display
    for (const auto& row : resTribes) {
	nlohmann::ordered_json item;
        item["tribe_id"] = row["tribe_id"].as<long long>();
        item["tribe_name"] = row["tribe_name"].as<std::string>();
        item["tribe_url"] = row["tribe_url"].is_null() ? "NONE" : row["tribe_url"].as<std::string>();
	item["member_count"] = row["member_count"].as<long long>(); 
	json_array.push_back(item);
    }
    // Return the json
    return json_array;
}
// Format character tribe history
nlohmann::ordered_json format_characters(const pqxx::result& resChars) {
    // Mapping characters
    std::map<std::string, nlohmann::ordered_json> characters;
    // Running through each character
    for (const auto& row : resChars) {
        std::string address = row["character_address"].as<std::string>();
        std::string name = row["name"].as<std::string>();
        std::string tribe = row["tribe_name"].as<std::string>();
        //long long left_date = row["left_at"].as<long long>();
        long long left_date = row["left_at"].is_null() ? -1 : row["left_at"].as<long long>(); // Checking for null, sentinel value
	// If first time seeing this character, set current tribe
        if (characters.find(address) == characters.end()) {
            nlohmann::ordered_json char_array;
            char_array["character_address"] = address;
            char_array["character_name"] = name;
            char_array["current_tribe"] = tribe;
            char_array["history"] = nlohmann::ordered_json::array();
            characters[address] = char_array;
        }
        // Add the tribe to history regardless
        nlohmann::ordered_json history_item;
        history_item["tribe_name"] = tribe;
        //history_item["left_date"] = left_date;
        // If left_at is null, show "NEVER LEFT", else show actual value
        if (row["left_at"].is_null()) {
            history_item["left_date"] = "NEVER LEFT";
        } else {
            history_item["left_date"] = row["left_at"].as<long long>();
        }
        characters[address]["history"].push_back(history_item);
    }
    // Put it all together
    nlohmann::ordered_json json_array = nlohmann::ordered_json::array();
    for (auto& [address, char_array] : characters) {
        json_array.push_back(char_array);
    }
    // Return the json
    return json_array;
}
