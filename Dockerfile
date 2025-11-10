# Dobrika Search Engine Docker Image
FROM ubuntu:22.04 AS builder

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git \
    libxapian-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libjsoncpp-dev \
    libuuid1 \
    uuid-dev \
    zlib1g-dev \
    libssl-dev \
    wget \
    curl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Build and install Drogon from source
RUN cd /tmp && \
    git clone --depth 1 --branch v1.8.7 https://github.com/drogonframework/drogon.git && \
    cd drogon && \
    git submodule update --init && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF -DBUILD_CTL=OFF && \
    make -j$(nproc) && \
    make install && \
    ldconfig && \
    cd / && rm -rf /tmp/drogon

# Set working directory for our project
WORKDIR /build

# Copy source files
COPY CMakeLists.txt buf.yaml ./
COPY src/ ./src/
COPY tests/ ./tests/

# Build the project
RUN mkdir -p build && \
    cd build && \
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DDOBRIKA_WITH_SERVER=ON && \
    cmake --build . -j$(nproc) --target dobrika_server_main && \
    strip dobrika_server_main

# Runtime stage
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install only runtime dependencies
RUN apt-get update && apt-get install -y \
    libxapian30 \
    libprotobuf23 \
    libjsoncpp25 \
    libuuid1 \
    libssl3 \
    zlib1g \
    wget \
    curl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy Drogon libraries from builder
COPY --from=builder /usr/local/lib/libdrogon.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libtrantor.so* /usr/local/lib/

# Update library cache
RUN ldconfig

# Create app user
RUN useradd -m -u 1000 dobrika

# Create app directories with proper permissions
RUN mkdir -p /app/db /app/uploads/tmp && \
    chown -R dobrika:dobrika /app && \
    chmod -R 755 /app

WORKDIR /app

# Copy built binary from builder
COPY --from=builder --chown=dobrika:dobrika /build/build/dobrika_server_main /app/

USER dobrika

ENV DOBRIKA_ADDR=0.0.0.0 \
    DOBRIKA_PORT=8080 \
    DOBRIKA_DB_PATH=/app/db \
    DOBRIKA_COLD_MIN=30 \
    DOBRIKA_HOT_MIN=15 \
    DOBRIKA_SEARCH_OFFSET=0 \
    DOBRIKA_SEARCH_LIMIT=20 \
    DOBRIKA_GEO_INDEX=2

# Expose default port
EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD wget --no-verbose --tries=1 --spider http://localhost:8080/healthz || exit 1

# Run the server
CMD ["./dobrika_server_main"]
