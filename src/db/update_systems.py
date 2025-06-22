import psycopg2
import requests
import time
from typing import Dict, Any, List

# Database connection parameters
DB_PARAMS = {
    "dbname": "crowapp",
    "user": "postgres",
    "password": "postgres",
    "host": "localhost",
    "port": "5432"
}

def get_db_connection():
    return psycopg2.connect(**DB_PARAMS)

def create_tables():
    with get_db_connection() as conn:
        with conn.cursor() as cur:
            # Create citext extension if it doesn't exist
            cur.execute("CREATE EXTENSION IF NOT EXISTS citext;")
            
            # Create new tables
            cur.execute("""
                CREATE TABLE IF NOT EXISTS systems (
                    solar_system_id   BIGINT PRIMARY KEY,
                    solar_system_name CITEXT NOT NULL,
                    region_id         BIGINT NOT NULL
                );
                
                CREATE TABLE IF NOT EXISTS incident (
                    id               BIGSERIAL PRIMARY KEY,
                    victim_address   TEXT,
                    victim_name      TEXT,
                    loss_type        TEXT,
                    killer_address   TEXT,
                    killer_name      TEXT,
                    time_stamp       BIGINT,
                    solar_system_id  BIGINT REFERENCES systems(solar_system_id),
                    solar_system_name TEXT NOT NULL
                );
            """)
            conn.commit()

def fetch_all_systems() -> List[Dict[str, Any]]:
    """Fetch all systems using the bulk endpoint with pagination"""
    all_systems = []
    offset = 0
    limit = 100
    
    while True:
        url = f"https://world-api-stillness.live.tech.evefrontier.com/v2/solarsystems?limit={limit}&offset={offset}"
        response = requests.get(url)
        if response.status_code == 200:
            data = response.json()
            systems = data['data']
            if not systems:  # Empty array means we've reached the end
                break
            all_systems.extend(systems)
            offset += limit
            print(f"Fetched {len(all_systems)} systems so far...")
        else:
            print(f"Failed to fetch systems at offset {offset}")
            break
    
    return all_systems

def fetch_system_details(system_id: int) -> Dict[str, Any]:
    """Fetch detailed information for a specific system"""
    url = f"https://world-api-stillness.live.tech.evefrontier.com/v2/solarsystems/{system_id}"
    response = requests.get(url)
    if response.status_code == 200:
        return response.json()
    return None

def insert_system_data(cur, system_data: Dict[str, Any]):
    """Insert or update system data"""
    cur.execute("""
        INSERT INTO systems (solar_system_id, solar_system_name, region_id)
        VALUES (%s, %s, %s)
        ON CONFLICT (solar_system_id) DO UPDATE
        SET solar_system_name = EXCLUDED.solar_system_name,
            region_id = EXCLUDED.region_id;
    """, (
        system_data['id'],
        system_data['name'],
        system_data.get('regionId', 0)  # Default to 0 if regionId is not present
    ))

def main():
    # Create tables
    create_tables()
    
    # First, fetch all systems in bulk
    print("Fetching all systems...")
    all_systems = fetch_all_systems()
    if not all_systems:
        print("Failed to fetch systems")
        return

    # Insert initial data
    with get_db_connection() as conn:
        with conn.cursor() as cur:
            for system in all_systems:
                insert_system_data(cur, system)
            conn.commit()
            print(f"Inserted {len(all_systems)} systems")

    # Now update each system with detailed information
    print("\nUpdating systems with detailed information...")
    with get_db_connection() as conn:
        with conn.cursor() as cur:
            for system in all_systems:
                system_id = system['id']
                detailed_data = fetch_system_details(system_id)
                if detailed_data:
                    insert_system_data(cur, detailed_data)
                    conn.commit()
                    print(f"Updated system {system_id}: {detailed_data['name']}")
                else:
                    print(f"Failed to fetch details for system {system_id}")
                
                # Add a small delay to avoid overwhelming the API
                time.sleep(0.1)

if __name__ == "__main__":
    main() 