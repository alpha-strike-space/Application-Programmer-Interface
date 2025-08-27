#pragma once
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>

nlohmann::ordered_json build_incident_json(const pqxx::result& res);
nlohmann::ordered_json build_system_json(const pqxx::result& res);
nlohmann::ordered_json format_name(const pqxx::result& resName);
nlohmann::ordered_json format_top_killers(const pqxx::result& resKillers);
nlohmann::ordered_json format_top_victims(const pqxx::result& resVictims);
nlohmann::ordered_json format_systems(const pqxx::result& resSystems);
