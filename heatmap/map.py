import os
from dotenv import load_dotenv
import psycopg2
import pandas as pd
import matplotlib.pyplot as plt
import plotly.express as px
import plotly.graph_objects as go
import numpy as np
from mpl_toolkits.mplot3d import Axes3D
import base64

# Load nebula image and encode as data URI
with open("B6901EF3-7FF5-424A-838C2A801A526C11_source.webp", "rb") as image_file:
    encoded_image = base64.b64encode(image_file.read()).decode()

image_source = f"data:image/webp;base64,{encoded_image}"

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

# Normalize coordinates if needed
df['x'] = df['x'] / 1e18
df['y'] = df['y'] / 1e18
df['z'] = df['z'] / 1e18

# Calculate distance from center (optional, for other colorizations)
df['distance'] = np.sqrt(df['x']**2 + df['y']**2 + df['z']**2)
df['opacity'] = np.where(df['kill_count'] == 0, 0.2, 1.0)

# Plot: color by kill_count (heatmap)
upper = df['kill_count'].quantile(0.90)  # 90th percentile
# Split the data
df_zero = df[df['kill_count'] == 0]
df_nonzero = df[df['kill_count'] > 0]

# Create traces
trace_zero = go.Scatter3d(
    x=df_zero['z'], y=df_zero['y'], z=df_zero['x'],
    mode='markers',
    marker=dict(
        size=2,
        color='gray',
        opacity=0.2
    ),
    customdata=df_zero['solar_system_name'],
    name='0 kills',
    showlegend=True,
    text=df_zero['solar_system_name'],
    hovertemplate='System ID: %{text}<br>Kills: 0<extra></extra>'
)
trace_nonzero = go.Scatter3d(
    x=df_nonzero['z'], y=df_nonzero['y'], z=df_nonzero['x'],
    mode='markers',
    marker=dict(
        size=2,
        color=df_nonzero['kill_count'],
        colorscale='reds',
        opacity=1.0,
        colorbar=dict(title='Kills'),
        cmin=df['kill_count'].min(),
        cmax=upper
    ),
    customdata=df_nonzero['solar_system_name'],
    name='>0 kills',
    text=df_nonzero['solar_system_name'],
    hovertemplate='System ID: %{text}<br>Kills: %{marker.color}<extra></extra>'
)

# Create the figure
fig = go.Figure(data=[trace_zero, trace_nonzero])

# Add layout (including nebula background)
fig.update_layout(
    scene=dict(
        xaxis=dict(
            backgroundcolor="black",
            gridcolor="gray",
            showbackground=True,
            zerolinecolor="gray",
            showticklabels=False,
            title='',
        ),
        yaxis=dict(
            backgroundcolor="black",
            gridcolor="gray",
            showbackground=True,
            zerolinecolor="gray",
            showticklabels=False,
            title='',
        ),
        zaxis=dict(
            backgroundcolor="black",
            gridcolor="gray",
            showbackground=True,
            zerolinecolor="gray",
            showticklabels=False,
            title='',
        ),
    ),
    images=[dict(
        source=image_source,
        xref="paper", yref="paper",
        x=0, y=1,
        sizex=1, sizey=1,
        sizing="stretch",
        opacity=1,
        layer="below"
    )],
    paper_bgcolor="black",
    plot_bgcolor="black",
    scene_camera=dict(
        eye=dict(x=1.5, y=1.5, z=1.5)
    )
)

fig.write_html(
    "scatter.html",
    include_plotlyjs='cdn',
    full_html=True,
    post_script="""
    var plot = document.getElementsByClassName('plotly-graph-div')[0];
    plot.on('plotly_click', function(data){
        for (var i = 0; i < data.points.length; i++) {
            var point = data.points[i];
            // Only redirect for nonzero kill systems (trace index 1)
            if (point.curveNumber === 1) {
                var system = point.customdata;
                if (system) {
                    var url = 'https://alpha-strike.space/pages/search.html?query=' + encodeURIComponent(system) + '&type=system';
                    window.open(url, '_blank');
                    break; // Only open one tab
                }
            }
        }
    });
    """
)