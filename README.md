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

## HTTP quick test (curl)
- Health:
```bash
curl -sS http://127.0.0.1:8088/healthz
```
- Index task:
```bash
curl -sS -X POST http://127.0.0.1:8088/index \
  -H 'Content-Type: application/json' \
  -d '{
    "task_name": "demo",
    "task_desc": "simple doc",
    "geo_data": "55.75,37.61",
    "task_id": "task-1",
    "task_type": "default"
  }'
```
- Search:
```bash
curl -sS -X POST http://127.0.0.1:8088/search \
  -H 'Content-Type: application/json' \
  -d '{
    "user_query": "demo",
    "geo_data": "55.75,37.61",
    "query_type": "QT_GeoTasks"
  }'
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

## Project structure
- `src/Components/xapian_processor` — Xapian слой
- `src/server` — HTTP сервер (Drogon)
- `src/Proto` — protobuf-модели
- `tests` — тесты (Catch2)
- `dev` — помощники для разработки (load test, JSON-данные)

---