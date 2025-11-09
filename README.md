# 🔎 Dobrika Search Engine

Lightweight search service built on top of Xapian, exposed via a fast C++ HTTP API with Drogon. Includes a small toolset and tests.

## Features
- Xapian-backed search layer with simple indexing and querying
- HTTP server (Drogon) exposing:
  - `GET /healthz` — health check
  - `POST /index` — index one task
  - `POST /search` — search by text (and optional geo/query type)
- Protobuf models for requests/responses
- Catch2 unit tests
- Dev utilities: JSON dataset and Python load-testing script

## Requirements
- Linux, macOS (Linux recommended)
- CMake ≥ 3.15
- C++17 toolchain (e.g., GCC 11+ or Clang 13+)
- pkg-config
- Protobuf (libprotobuf, protoc)
- Xapian (xapian-core)
- Drogon (and its deps: trantor, jsoncpp, zlib, etc.)

On Ubuntu/Debian:
```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config \
  libprotobuf-dev protobuf-compiler \
  libxapian-dev \
  libdrogon-dev \
  libjsoncpp-dev
```

If your distro provides Drogon via CMake config (`DrogonConfig.cmake`) it will be auto-detected. If not, the build will try a pkg-config fallback and then a manual header/lib search.

## Build
Configure and build (Release):
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDOBRIKA_WITH_SERVER=ON
cmake --build build -j
```

The build produces:
- `build/dobrika_search` (static library)
- `build/dobrika_server_main` (HTTP server)
- `build/dse_tests` (unit tests)
- `build/dse_http_tests` (HTTP tests; only if Drogon found)

## Run the server
The server is configured via environment variables:
- `DOBRIKA_ADDR` (default `127.0.0.1`)
- `DOBRIKA_PORT` (default `8088`)
- `DOBRIKA_DB_PATH` (default `db`)
- `DOBRIKA_COLD_MIN` (default `30`)
- `DOBRIKA_HOT_MIN` (default `15`)
- `DOBRIKA_SEARCH_OFFSET` (default `0`)
- `DOBRIKA_SEARCH_LIMIT` (default `20`)
- `DOBRIKA_GEO_INDEX` (default `2`)

Start:
```bash
./build/dobrika_server_main
```

Custom port/db:
```bash
DOBRIKA_ADDR=0.0.0.0 DOBRIKA_PORT=8090 DOBRIKA_DB_PATH=/tmp/dse-db \
  ./build/dobrika_server_main
```

## API Documentation

### Endpoints Overview
- `GET /healthz` — Health check
- `POST /index` — Index a new task
- `POST /search` — Search for tasks

### Request/Response Models

#### Index Task (`POST /index`)

**Request:**
```json
{
  "task_name": "string",      // Task name (indexed)
  "task_desc": "string",      // Task description (indexed)
  "task_id": "string",        // Unique task identifier (required)
  "geo_data": "lat,lon",      // Geo coordinates as "latitude,longitude"
  "task_type": "string"       // Task type: "TT_OnlineTask" or "TT_OfflineTask"
}
```

**Response:**
```json
{
  "status": "string"          // Status: "SearchIndexOk" or "SearchIndexFall"
}
```

**Example:**
```bash
curl -X POST http://127.0.0.1:8088/index \
  -H 'Content-Type: application/json' \
  -d '{
    "task_name": "Web development",
    "task_desc": "Need help building a React app",
    "geo_data": "55.75,37.61",
    "task_id": "task-123",
    "task_type": "TT_OnlineTask"
  }'
```

#### Search Tasks (`POST /search`)

**Request:**
```json
{
  "user_query": "string",           // Search query (text search in name/desc)
  "query_type": "string",           // Query type (optional):
                                    //   "QT_GeoTasks" - search with geo filter
                                    //   "QT_OnlineTasks" - online tasks only
                                    //   "QT_RandomTasks" - random results
  "geo_data": "lat,lon",            // Geo coordinates for QT_GeoTasks
  "user_tags": ["tag1", "tag2"]     // Tags for filtering (optional)
}
```

**Response:**
```json
{
  "task_id": ["id1", "id2", ...],   // Array of matching task IDs
  "status": "string"                // Status: "SearchOk", "SearchUnknownType", etc.
}
```

**Example 1: Text search**
```bash
curl -X POST http://127.0.0.1:8088/search \
  -H 'Content-Type: application/json' \
  -d '{
    "user_query": "React development",
    "query_type": "QT_OnlineTasks"
  }'
```

**Example 2: Geo-based search**
```bash
curl -X POST http://127.0.0.1:8088/search \
  -H 'Content-Type: application/json' \
  -d '{
    "user_query": "python",
    "query_type": "QT_GeoTasks",
    "geo_data": "55.75,37.61"
  }'
```

### Status Codes
- `SearchOk` — Search completed successfully
- `SearchIndexOk` — Task indexed successfully
- `SearchIndexFall` — Indexing failed
- `SearchHealthOk` — Server is healthy
- `SearchUnknownType` — Unknown query/task type
- `SearchNotImplemented` — Feature not implemented
- `SearchInvalidJson` — Invalid JSON in request

### HTTP quick test (curl)
- Health:
```bash
curl -sS http://127.0.0.1:8088/healthz
```

## Python load test
Files:
- `dev/data/bulk_tasks.json` — 50 задач с разной геолокацией
- `dev/load_test.py` — скрипт обстрела
- `dev/requirements.txt` — зависимости (requests)

Установка зависимостей:
```bash
python3 -m pip install -r dev/requirements.txt
```

Вариант 1: сервер уже поднят на 127.0.0.1:8088
```bash
python3 dev/load_test.py --concurrency 16 --search-requests 500
```

Вариант 2: скрипт сам поднимет сервер и обстреляет (порт 8090):
```bash
python3 dev/load_test.py --run-server --port 8090 --db /tmp/dse-db \
  --concurrency 12 --search-requests 300
```

Полезные флаги:
- `--host`, `--port`
- `--tasks-file` (путь к JSON), `--index-limit` (по умолчанию 50)
- `--concurrency`
- `--search-requests`
- `--binary` (путь к `dobrika_server_main`, по умолчанию `build/dobrika_server_main`)

Скрипт выводит агрегаты: успехи/ошибки, avg/p50/p95 и общее время на индексацию/поиск.

## Tests
Собрать и запустить:
```bash
cmake --build build -j --target dse_tests
ctest --test-dir build --output-on-failure
```

HTTP-тесты (если Drogon найден):
```bash
cmake --build build -j --target dse_http_tests
ctest --test-dir build -R dse_http_tests --output-on-failure
```

## Troubleshooting
- Port already in use:
  ```bash
  sudo ss -lptn 'sport = :8088'
  sudo fuser -k 8088/tcp
  ```
- Drogon not found:
  - Установите `libdrogon-dev` (или соберите Drogon из исходников)
  - Убедитесь, что есть либо CMake-конфиг (`DrogonConfig.cmake`), либо pkg-config файл
- Missing json/json.h:
  - Установите `libjsoncpp-dev`
- Прочее: удалите `build/` и пересоберите:
  ```bash
  rm -rf build
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDOBRIKA_WITH_SERVER=ON
  cmake --build build -j
  ```

## Docker

### Quick Start
Build and run the server in Docker:

```bash
# Build the image
docker compose build

# Start the server (background)
docker compose up -d

# View logs
docker compose logs -f

# Stop the server
docker compose down
```

### Using the Helper Script
A convenience script is provided:

```bash
# Build
./docker-run.sh build

# Start
./docker-run.sh up

# View logs
./docker-run.sh logs

# Open shell in container
./docker-run.sh shell

# Stop
./docker-run.sh down

# Clean up (remove containers, images, volumes)
./docker-run.sh clean

# Check status
./docker-run.sh status
```

### Configuration
The Docker setup:
- **Image**: Multi-stage build (optimized ~200MB runtime)
- **Port**: `8080` (maps to `8088` inside container)
- **Volumes**:
  - `./db` — persists Xapian database
  - `./uploads` — temporary upload storage
- **User**: Non-root user (`dobrika:1000:1000`) for security

### HTTP API Access
Once running, the API is available at `http://localhost:8080`:

```bash
# Health check
curl http://localhost:8080/healthz

# Index a task
curl -X POST http://localhost:8080/index \
  -H 'Content-Type: application/json' \
  -d '{
    "task_name": "demo",
    "task_desc": "simple doc",
    "geo_data": "55.75,37.61",
    "task_id": "task-1",
    "task_type": "default"
  }'

# Search
curl -X POST http://localhost:8080/search \
  -H 'Content-Type: application/json' \
  -d '{"query": "simple"}'
```

### Troubleshooting
- **Permission denied on socket**: Run with `sudo` or add user to docker group:
  ```bash
  sudo usermod -aG docker $USER
  newgrp docker
  ```
- **Port already in use**:
  ```bash
  sudo lsof -i :8080
  sudo kill -9 <PID>
  ```
- **Permission errors on uploads**: Ensure correct ownership:
  ```bash
  sudo chown -R 1000:1000 ./db ./uploads
  sudo chmod -R 755 ./db ./uploads
  ```

## Project structure
- `src/xapian_processor` — Xapian слой
- `src/server` — HTTP сервер (Drogon)
- `src/proto` — protobuf-модели
- `tests` — тесты (Catch2)
- `dev` — помощники для разработки (load test, JSON-данные)

---