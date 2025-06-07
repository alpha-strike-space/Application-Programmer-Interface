#pragma once
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>

nlohmann::ordered_json build_incident_json(const pqxx::result& res);
nlohmann::ordered_json build_system_json(const pqxx::result& res);
nlohmann::ordered_json formatName(const pqxx::result& resName);
nlohmann::ordered_json formatTopKillers(const pqxx::result& resKillers);
nlohmann::ordered_json formatTopVictims(const pqxx::result& resVictims);
nlohmann::ordered_json formatSystems(const pqxx::result& resSystems);
