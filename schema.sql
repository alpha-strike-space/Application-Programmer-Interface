-- schema.sql: Database schema for the CrowApp API

-- Table: systems
CREATE TABLE IF NOT EXISTS systems (
    solar_system_id   BIGINT PRIMARY KEY,
    solar_system_name TEXT NOT NULL,
    x                 TEXT,
    y                 TEXT,
    z                 TEXT
);

-- Table: incident
CREATE TABLE IF NOT EXISTS incident (
    id               BIGSERIAL PRIMARY KEY,
    victim_address   TEXT,
    victim_name      TEXT,
    loss_type        TEXT,
    killer_address   TEXT,
    killer_name      TEXT,
    time_stamp       BIGINT,
    solar_system_id  BIGINT,
    solar_system_name TEXT NOT NULL
    
); 