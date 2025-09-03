###################
# Note, you must use the CMakeLists.
# Otherwise, not all dependencies will be necessarily found in the docker container.
###################
# Latest GCC image
FROM gcc:latest
# Run installation of necessary packages for the CMake and crow header.
RUN apt update && apt install -y \
    g++ \
    cmake \
    make \
    git
# Create a working directory and copy everything over in our current file path.
WORKDIR /app
COPY . .
# Build the file with our crow header.
RUN cmake -S . -B build && cmake --build build
# Run the API written in server.cpp
CMD ["./build/server"]
