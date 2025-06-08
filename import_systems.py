#!/usr/bin/env python3
import json
import subprocess
import psycopg2
from psycopg2.extras import execute_values

# Database connection parameters
DB_NAME = "eve_killmails"
DB_USER = "calaw"  # Replace with your username if different
DB_HOST = "localhost"
DB_PORT = "5432"

def fetch_systems():
    # Fetch data from API
    curl_cmd = ["curl", "-s", "https://api.alpha-strike.space/location"]
    result = subprocess.run(curl_cmd, capture_output=True, text=True)
    return json.loads(result.stdout)

def insert_systems(systems_data):
    # Connect to the database
    conn = psycopg2.connect(
        dbname=DB_NAME,
        user=DB_USER,
        host=DB_HOST,
        port=DB_PORT
    )
    cur = conn.cursor()

    # Prepare the data for insertion
    values = []
    for system in systems_data:
        values.append((
            system['solar_system_id'],
            system['solar_system_name'],
            system['coordinates']['x'],
            system['coordinates']['y'],
            system['coordinates']['z']
        ))

    # Insert the data
    insert_query = """
        INSERT INTO systems (solar_system_id, solar_system_name, x, y, z)
        VALUES %s
        ON CONFLICT (solar_system_id) DO UPDATE
        SET solar_system_name = EXCLUDED.solar_system_name,
            x = EXCLUDED.x,
            y = EXCLUDED.y,
            z = EXCLUDED.z;
    """
    
    execute_values(cur, insert_query, values)
    
    # Commit and close
    conn.commit()
    cur.close()
    conn.close()

def main():
    try:
        print("Fetching systems data...")
        systems_data = fetch_systems()
        print(f"Found {len(systems_data)} systems")
        
        print("Inserting data into database...")
        insert_systems(systems_data)
        print("Done!")
        
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main() 