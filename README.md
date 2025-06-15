# Application-Programmer-Interface
The https://frontier.alpha-strike.space/ Application Programmer Interface development file. Mostly, written in the C++ language utilizing the Crow framework. You may find more information about the amazing Crow framework at https://crowcpp.org/master/ that which makes this service possible. 

# Local API Testing Guide (BSD/Linux)

## Prerequisites

- **Compiler:** GCC (>= 10) or Clang (>= 11)
- **CMake:** >= 3.14
- **Crow framework** (ensure you use the maintained fork, e.g. [CrowCpp/Crow](https://github.com/CrowCpp/Crow))
- **Dependencies:** Any dependencies are in the CMakeLists.txt file.

## Setup

```sh
# Clone the repository
git clone https://github.com/alpha-strike-space/Application-Programmer-Interface.git
cd Application-Programmer-Interface

# Create and enter a build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build the project
make
```

> **macOS Users:** The standard build process may require extra steps. Please see the detailed **[macOS Build Guide](BUILD_MACOS.md)** for specific instructions.

## Running the API

Please make a visit to https://github.com/alpha-strike-space/PostgreSQL-Configuration if you have any questions regarding how the database tables are set up. Now, to run this server in development or production, you must provide the following environment variables so it can connect to the PostgreSQL database:

### PgBouncer (for Routes.cpp and pooled queries in pgListener.cpp)
- `PGBOUNCER_HOST`: Name of Bouncer Service
- `PGBOUNCER_PORT`: Typically, port 6432
- `PGBOUNCER_DB`: DB name
- `PGBOUNCER_USER`: DB user
- `PGBOUNCER_PASSWORD`: DB user password
  
### PostgreSQL Direct (for LISTEN/NOTIFY in pgListener.cpp)
- `PGDIRECT_HOST`: Name of PG Service
- `PGDIRECT_PORT`: Typically, port 5432
- `PGDIRECT_DB`: DB name
- `PGDIRECT_USER`: DB user
- `PGDIRECT_PASSWORD`: DB user password

You can set these variables in-line when you execute the binary from the project root. Many different ways these variables may be declared. Best practice in run-time is to load them not from a .env file. Although, this is perfectly fine for development.

- The default application port is usually `8080` (check your `Server.cpp` or config).
- On BSD, replace `make` with `gmake` if necessary.

## Testing Endpoints Locally

You can test endpoints using `curl` or tools like [HTTPie](https://httpie.io/):

Note: Cross reference an endpoint and try it out.

```sh
curl http://localhost:8080/endpoint
```

Note: This is for future reference. We do not have a post function yet.

For POST requests with JSON:
```sh
curl -X POST -H "Content-Type: application/json" -d '{"key":"value"}' http://localhost:8080/endpoint
```

For Websocket connections:
```sh
wscat -c ws://localhost/endpoint
```

## Example Endpoints

| Method | Path                | Description        |
|--------|---------------------|--------------------|
| GET    | /health             | Health check       |
| POST   | /endpoint           | Example resource   |

> Replace with actual endpoints.

## Example Websocket

| Path                | Description        |
|---------------------|--------------------|
| /mails              | Incidents          |

> Replace with actual websocket.

## Troubleshooting

- **Port in Use:** If 8080 is in use, change the port in your Server.cpp.
- **Missing Libraries:** Install dependencies using your OS package manager (e.g., `apt`, `pkg`, or `brew`).
- **Permission Denied:** Make sure the binary is executable: `chmod +x server`

## BSD Notes

- Use `gmake` if `make` fails.
- Ensure you have all developer tools installed: `pkg install gcc cmake git gmake`

---
