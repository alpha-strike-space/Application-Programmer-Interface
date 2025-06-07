# Application-Programmer-Interface
The alpha-strike.space Application Programmer Interface development file. Mostly, written in the C++ language utilizing the Crow framework. You may find more information about the amazing Crow framework at https://crowcpp.org/master/ that which makes this service possible. 

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

## Running the API

```sh
# From the build directory (or wherever your binary is output)
./api_binary
```

- The default port is usually `8080` (check your main.cpp or config).
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
- **Permission Denied:** Make sure the binary is executable: `chmod +x api_binary`

## BSD Notes

- Use `gmake` if `make` fails.
- Ensure you have all developer tools installed: `pkg install gcc cmake git gmake`

---
