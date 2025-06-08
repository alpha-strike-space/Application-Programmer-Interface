# How to Build on macOS

This guide provides step-by-step instructions for building and running the application on a macOS system using Homebrew for package management.

## 1. Prerequisites: Install Dependencies

You will need to install the necessary PostgreSQL C client library (`libpq`).

Open your terminal and run the following command:

```bash
# Install the PostgreSQL client libraries (libpq)
brew install libpq
```

After installation, `libpq` is usually located at `/opt/homebrew/opt/libpq`. We will use this path during the build process.

## 2. Build the Application

We have prepared a single command that cleans any previous build artifacts, configures the project with CMake, and compiles the source code.

This command points CMake to the correct location for the PostgreSQL libraries you just installed.

```bash
rm -rf build && mkdir build && cd build && \
cmake .. \
  -DPostgreSQL_ROOT=/opt/homebrew/opt/libpq \
  -DPostgreSQL_LIBRARY=/opt/homebrew/opt/libpq/lib/libpq.dylib \
  -DPostgreSQL_INCLUDE_DIR=/opt/homebrew/opt/libpq/include && \
make
```

If the command succeeds, you will have a `server` executable inside the `build` directory.

## 3. Configure Environment Variables

The server connects to a PostgreSQL database and requires connection details to be set as environment variables. The application will read these variables at runtime.

The following variables are required:
- `POSTGRES_DB`: The name of your database.
- `POSTGRES_USER`: Your PostgreSQL username.
- `POSTGRES_PASSWORD`: Your PostgreSQL password.
- `POSTGRES_HOST`: The database server address (e.g., `localhost`).
- `POSTGRES_PORT`: The port the database is running on (e.g., `5432`).

## 4. Run the Server

You can run the server from the project's root directory using the following command.

**Important:** Replace the placeholder values (`your_db`, `your_user`, `your_pass`) with your actual database credentials.

```bash
POSTGRES_DB=your_db POSTGRES_USER=your_user POSTGRES_PASSWORD=your_pass POSTGRES_HOST=localhost POSTGRES_PORT=5432 ./build/server
```

The server should now be running and connected to your database. You will see output indicating that it is listening for notifications on the `killmail` channel. 