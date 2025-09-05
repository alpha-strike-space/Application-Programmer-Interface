#pragma once
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>
// All serializer functions
//nlohmann::ordered_json build_health_json(const pqxx::result& res);
nlohmann::ordered_json build_incident_json(const pqxx::result& res);
nlohmann::ordered_json build_system_json(const pqxx::result& res);
nlohmann::ordered_json format_top_names(const pqxx::result& resName);
nlohmann::ordered_json format_top_killers(const pqxx::result& resKillers);
nlohmann::ordered_json format_top_victims(const pqxx::result& resVictims);
nlohmann::ordered_json format_top_systems(const pqxx::result& resSystems);
nlohmann::ordered_json format_top_tribes(const pqxx::result& resTribes);
nlohmann::ordered_json format_tribe_membership(const pqxx::result& resTribes);
nlohmann::ordered_json format_tribes(const pqxx::result& resTribes);
nlohmann::ordered_json format_characters(const pqxx::result& resChars);
