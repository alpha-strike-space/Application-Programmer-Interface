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
  
## 4. Run the Server

You can run the server from the project's root directory using the following command.

**Important:** Replace the placeholder values (`your_db`, `your_user`, `your_pass`) with your actual database credentials.

The server should now be running and connected to your database. You will see output indicating that it is listening for notifications on the `killmail` channel. 
