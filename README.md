# Application-Programmer-Interface

The alpha-strike.space Application Programmer Interface development file. Mostly written in C++ using the Crow framework, with a Python-based heatmap visualization tool.

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Dependencies](#dependencies)
3. [Installation](#installation)
    - [Debian/Ubuntu](#debianubuntu)
    - [Arch Linux](#arch-linux)
    - [macOS](#macos)
    - [Windows](#windows)
4. [Building the C++ API Server](#building-the-c-api-server)
5. [Running the API Server](#running-the-api-server)
6. [Heatmap Visualization Tool](#heatmap-visualization-tool)
7. [Usage Examples](#usage-examples)
8. [Troubleshooting](#troubleshooting)

---

## Project Overview

This repository contains two main components:

- **C++ API Server (`server.cpp`)**: A high-performance REST API and WebSocket server for querying and streaming incident and system data from a PostgreSQL database. Built with [Crow](https://crowcpp.org/master/), [libpqxx](https://github.com/jtv/libpqxx), and [nlohmann/json](https://github.com/nlohmann/json).
- **Heatmap Visualization (`heatmap/map.py`)**: A Python script that generates a 3D interactive heatmap of system activity (kill counts) using Plotly, based on data from the same PostgreSQL database.

---

## Dependencies

### C++ API Server

- **Crow** (web framework, C++)
- **libpqxx** (PostgreSQL C++ client)
- **nlohmann/json** (JSON for Modern C++)
- **cpp-dotenv** (for environment variable management)
- **PostgreSQL** (server and client libraries)
- **CMake** (>= 3.10)
- **A C++17 compiler** (e.g., g++ 9+, clang 10+)
- **Threads** (usually provided by your system)

### Python Heatmap Tool

- **Python 3.8+**
- **psycopg2** (PostgreSQL client)
- **pandas**
- **matplotlib**
- **plotly**
- **numpy**
- **python-dotenv**

---

## Installation

### Debian/Ubuntu

```sh
sudo apt update
sudo apt install -y build-essential cmake git libpqxx-dev libpq-dev python3 python3-pip
# For Crow and nlohmann/json, the build system will fetch them automatically.
# Python dependencies:
pip3 install -r requirements.txt
# Or, manually:
pip3 install psycopg2 pandas matplotlib plotly numpy python-dotenv
```

### Arch Linux

```sh
sudo pacman -Syu base-devel cmake git libpqxx postgresql python python-pip
pip install --user psycopg2 pandas matplotlib plotly numpy python-dotenv
```

### macOS

```sh
brew install cmake git libpqxx postgresql
# Ensure PostgreSQL is running and initialized
brew services start postgresql
pip3 install --user psycopg2 pandas matplotlib plotly numpy python-dotenv
```

### Windows

- Install [Visual Studio](https://visualstudio.microsoft.com/) with C++ support.
- Install [CMake](https://cmake.org/download/).
- Install [PostgreSQL](https://www.postgresql.org/download/windows/) and ensure `libpq` and `libpqxx` are available (may require building from source or using vcpkg).
- Install Python 3 from [python.org](https://www.python.org/downloads/).
- Install Python dependencies:
  ```
  pip install psycopg2 pandas matplotlib plotly numpy python-dotenv
  ```
- Use a terminal like PowerShell or Git Bash for building.

---

## Building the C++ API Server

1. **Clone the repository (with submodules):**
   ```sh
   git clone --recurse-submodules https://github.com/alpha-strike-space/Application-Programmer-Interface.git
   cd Application-Programmer-Interface
   ```

2. **Configure and build:**
   ```sh
   mkdir build && cd build
   cmake ..
   make -j$(nproc)
   ```

   The server executable will be named `server`.

---

## Running the API Server

1. **Set up your `.env` file** in the project root with your PostgreSQL credentials:
   ```
   DB_NAME=your_db
   DB_USER=your_user
   DB_PASSWORD=your_password
   DB_HOST=localhost
   DB_PORT=5432
   ```

2. **Start the server:**
   ```sh
   ./server
   ```

   The server listens on port 8080 by default.

### API Endpoints

- `GET /health` — Health check endpoint.
- `GET /location` — Query system locations. Optional query: `?system=...`
- `GET /totals` — Get top killers, victims, and systems. Filters: `?name=...`, `?system=...`, `?filter=day|week|month`
- `GET /incident` — Query incidents. Filters: `?name=...`, `?system=...`, `?mail_id=...`, `?filter=day|week|month`
- `WS /mails` — WebSocket endpoint for real-time notifications.

See `server.cpp` for full details on query parameters and response formats.

---

## Heatmap Visualization Tool

This tool visualizes system activity as a 3D heatmap.

### Usage

1. **Ensure your `.env` file is present** (see above).
2. **Run the script:**
   ```sh
   cd heatmap
   python3 map.py
   ```
   - This will generate a file called `scatter.html` in the `heatmap/` directory.
   - Open `scatter.html` in your browser to view the interactive heatmap.

#### What it does

- Connects to the PostgreSQL database using credentials from `.env`.
- Queries system coordinates and kill counts.
- Plots a 3D scatter plot with color intensity based on kill counts.
- Overlays a nebula background image for aesthetics.

#### Customization

- You can change the background image by replacing `B6901EF3-7FF5-424A-838C2A801A526C11_source.webp`.
- Adjust the SQL query or plotting parameters in `map.py` for different visualizations.

---

## Usage Examples

### Querying the API

```sh
curl http://localhost:8080/health
curl http://localhost:8080/location?system=Jita
curl http://localhost:8080/totals?filter=week
curl http://localhost:8080/incident?name=SomePlayer
```

### Using the WebSocket

Connect to `ws://localhost:8080/mails` using a WebSocket client to receive real-time notifications.

---

## Troubleshooting

- **Build errors:** Ensure all dependencies are installed and submodules are initialized.
- **PostgreSQL connection issues:** Check your `.env` file and that the database is running and accessible.
- **Python errors:** Ensure you are using Python 3.8+ and all required packages are installed.

---

## License

See LICENSE for details.


