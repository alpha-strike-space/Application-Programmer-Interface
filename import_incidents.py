#!/usr/bin/env python3
import json
import subprocess
import psycopg2
from psycopg2.extras import execute_values
from datetime import datetime

# Database connection parameters
DB_NAME = "eve_killmails"
DB_USER = "calaw"  # Replace with your username if different
DB_HOST = "localhost"
DB_PORT = "5432"

def fetch_incidents():
    # Fetch data from API
    curl_cmd = ["curl", "-s", "https://api.alpha-strike.space/incident"]
    result = subprocess.run(curl_cmd, capture_output=True, text=True)
    return json.loads(result.stdout)

def insert_incidents(incidents_data):
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
    for incident in incidents_data:
        values.append((
            incident['id'],
            incident.get('victim_address', ''),
            incident.get('victim_name', ''),
            incident.get('loss_type', ''),
            incident.get('killer_address', ''),
            incident.get('killer_name', ''),
            incident.get('time_stamp', 0),
            incident.get('solar_system_id'),
            incident.get('solar_system_name', '')
        ))

    # Insert the data
    insert_query = """
        INSERT INTO incident (
            id, victim_address, victim_name, loss_type, 
            killer_address, killer_name, time_stamp, 
            solar_system_id, solar_system_name
        )
        VALUES %s
        ON CONFLICT (id) DO UPDATE
        SET victim_address = EXCLUDED.victim_address,
            victim_name = EXCLUDED.victim_name,
            loss_type = EXCLUDED.loss_type,
            killer_address = EXCLUDED.killer_address,
            killer_name = EXCLUDED.killer_name,
            time_stamp = EXCLUDED.time_stamp,
            solar_system_id = EXCLUDED.solar_system_id,
            solar_system_name = EXCLUDED.solar_system_name;
    """
    
    execute_values(cur, insert_query, values)
    
    # Commit and close
    conn.commit()
    cur.close()
    conn.close()

def main():
    try:
        print("Fetching incidents data...")
        incidents_data = fetch_incidents()
        print(f"Found {len(incidents_data)} incidents")
        
        print("Inserting data into database...")
        insert_incidents(incidents_data)
        print("Done!")
        
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main() 