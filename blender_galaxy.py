import bpy
import math
import json
import os
from dotenv import load_dotenv
import psycopg2
import pandas as pd

# Load environment variables from .env file
load_dotenv()

# Get DB credentials from environment
DB_NAME = os.getenv("DB_NAME")
DB_USER = os.getenv("DB_USER")
DB_PASSWORD = os.getenv("DB_PASSWORD")
DB_HOST = os.getenv("DB_HOST")
DB_PORT = os.getenv("DB_PORT", 5432)

# Connect to the database
conn = psycopg2.connect(
    dbname=DB_NAME,
    user=DB_USER,
    password=DB_PASSWORD,
    host=DB_HOST,
    port=DB_PORT
)

# Query systems and kill counts
query = """
SELECT
    s.solar_system_name,
    s.x,
    s.y,
    s.z,
    COALESCE(i.kill_count, 0) AS kill_count
FROM
    systems s
LEFT JOIN (
    SELECT
        solar_system_name,
        COUNT(*) AS kill_count
    FROM
        incident
    GROUP BY
        solar_system_name
) i ON s.solar_system_name = i.solar_system_name;
"""

df = pd.read_sql_query(query, conn)
conn.close()

# Normalize coordinates
df['x'] = df['x'] / 1e18
df['y'] = df['y'] / 1e18
df['z'] = df['z'] / 1e18

# Clear existing objects
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete()

# Create a new collection for our galaxy
galaxy_collection = bpy.data.collections.new("Galaxy")
bpy.context.scene.collection.children.link(galaxy_collection)

# Create a material for the points
def create_point_material(name, color, emission_strength):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    
    # Clear default nodes
    nodes.clear()
    
    # Create emission shader
    emission = nodes.new('ShaderNodeEmission')
    emission.inputs[0].default_value = color
    emission.inputs[1].default_value = emission_strength
    
    # Create output node
    output = nodes.new('ShaderNodeOutputMaterial')
    
    # Link nodes
    mat.node_tree.links.new(emission.outputs[0], output.inputs[0])
    
    return mat

# Create materials for different kill counts
zero_kills_mat = create_point_material("ZeroKills", (0.2, 0.2, 0.2, 1), 0.5)
nonzero_kills_mat = create_point_material("NonZeroKills", (1, 0, 0, 1), 1.0)

# Create points
for _, row in df.iterrows():
    # Create a circle mesh
    bpy.ops.mesh.primitive_circle_add(
        radius=0.01,
        vertices=32,
        location=(row['x'], row['y'], row['z'])
    )
    
    # Get the created object
    point = bpy.context.active_object
    
    # Add billboard constraint to always face camera
    constraint = point.constraints.new(type='TRACK_TO')
    constraint.target = bpy.context.scene.camera
    constraint.track_axis = 'TRACK_NEGATIVE_Z'
    constraint.up_axis = 'UP_Y'
    
    # Assign material based on kill count
    if row['kill_count'] == 0:
        point.data.materials.append(zero_kills_mat)
    else:
        point.data.materials.append(nonzero_kills_mat)
    
    # Move to galaxy collection
    bpy.context.scene.collection.objects.unlink(point)
    galaxy_collection.objects.link(point)

# Set up camera
bpy.ops.object.camera_add(location=(2, 2, 2))
camera = bpy.context.active_object
camera.rotation_euler = (math.radians(45), 0, math.radians(45))
bpy.context.scene.camera = camera

# Set up world background
world = bpy.context.scene.world
world.use_nodes = True
bg = world.node_tree.nodes['Background']
bg.inputs[0].default_value = (0, 0, 0, 1)  # Black background
bg.inputs[1].default_value = 1.0  # Strength

# Set render settings
bpy.context.scene.render.engine = 'CYCLES'
bpy.context.scene.cycles.samples = 128
bpy.context.scene.render.film_transparent = True 