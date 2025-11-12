# 🔎 Dobrika Search Engine

Dobrika — лёгкий HTTP‑поисковик на C++17: Xapian в качестве движка, Drogon как веб‑фреймворк, gRPC/Protobuf модели и минимальный набор утилит.

---

## Quick Start

### 1. Native build (Linux)
```bash
sudo apt update && sudo apt install -y \
  build-essential cmake pkg-config libprotobuf-dev protobuf-compiler \
  libxapian-dev libdrogon-dev libjsoncpp-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDOBRIKA_WITH_SERVER=ON
cmake --build build -j
```

Запуск:
```bash
./build/dobrika_server_main
# или с переопределением окружения
DOBRIKA_ADDR=0.0.0.0 DOBRIKA_PORT=8088 DOBRIKA_DB_PATH=/tmp/dobrika-db \
  ./build/dobrika_server_main
```

API:
- `POST /index` — добавить задачу
- `POST /search` — поиск (текст, гео, тэги)
- `GET /healthz` — проверка живости
- `GET /metrics` — Prometheus‑метрики

### 2. Docker / docker-compose
```bash
./docker-run.sh build   # собрали образ
./docker-run.sh up      # подняли сервис на 127.0.0.1:8088
./docker-run.sh logs    # смотрим логи
```

Переменная `DOBRIKA_LOG_REQUESTS=1` включает вывод каждого HTTP‑запроса (полезно в тестах).

### 3. Kubernetes (demo manifests)
```bash
kubectl apply -f deployments/k8s/configmap.yaml
kubectl apply -f deployments/k8s/deployment.yaml
kubectl apply -f deployments/k8s/service.yaml
```

Манифесты развёртывают один Pod с образом `ghcr.io/slipneff/dobrika-search:latest`, пробрасывают `/metrics` и `/healthz`, используют `emptyDir` под Xapian‑базу (замените на PVC для продакшена).

---

## Configuration

| Env var | Default | Назначение |
| --- | --- | --- |
| `DOBRIKA_ADDR` | `127.0.0.1` | Адрес, на котором слушает сервер |
| `DOBRIKA_PORT` | `8088` | Порт HTTP‑сервера |
| `DOBRIKA_DB_PATH` | `db` | Путь к каталогу с Xapian БД |
| `DOBRIKA_COLD_MIN` / `DOBRIKA_HOT_MIN` | `30` / `15` | Периоды бэкапов (мин.) |
| `DOBRIKA_SEARCH_OFFSET` | `0` | Начальный offset результатов |
| `DOBRIKA_SEARCH_LIMIT` | `20` | Количество результатов |
| `DOBRIKA_GEO_INDEX` | `9` | Слот Xapian для гео‑индекса |
| `DOBRIKA_LOG_REQUESTS` | `0` | `1/true/on` — логировать каждую жалобу с телом |

Каталог `db/` должен принадлежать пользователю процесса. Для Docker Compose мы используем именованный volume `dobrika-db`, поэтому проблем с правами не возникает.

---

## Monitoring & Metrics

Сервер экспортирует Prometheus-метрики на том же порту, что и HTTP API:
```
GET /metrics
```

### Просмотр метрик из «боевого» кластера локально

1. **Пробросьте сервис из кластера:**
   ```bash
   kubectl port-forward svc/search-engine 18088:8080
   ```
   Пока команда работает, `http://localhost:18088/metrics` отдаёт live-метрики.

2. **Запустите локальные Prometheus + Grafana** (используется `network_mode: host`):
   ```bash
   docker compose -f monitoring/docker-compose.yml up -d
   ```
   Конфиг `monitoring/prometheus.yml` уже нацелен на `localhost:18088`.

3. **Проверьте Prometheus:** `http://localhost:9090/targets` → job `dobrika-prod` должен быть `UP`.

4. **Grafana:** `http://localhost:3000` (логин/пароль `admin / admin`).  
   Источник данных → Prometheus → URL `http://localhost:9090` → Save & Test.  
   Панели можно собирать из метрик `dobrika_search_requests_total`, `rate(...)` и т.п.

5. **Lens:** в настройках кластера укажите Prometheus endpoint `http://localhost:9090` — графики в Lens подтянутся автоматически.

> Минимальный мониторинг в кластере: установите `metrics-server`, чтобы Lens показывал нагрузку CPU/Mem даже без Prometheus.

---

## Testing & Tooling

- **CMake tests**
  ```bash
  cmake --build build -j --target dse_tests
  ctest --test-dir build --output-on-failure
  ```
- **HTTP интеграционные тесты** (`dev/test_quality.py`): требуют работающий сервер (локально или по `RUN_SERVER=1`).
- **Нагрузочный скрипт** `dev/load_test.py`: использует `dev/data/bulk_tasks.json`.

---

## Troubleshooting

- **DatabaseLockError при старте** — у процесса нет прав на `DOBRIKA_DB_PATH`. Исправьте владельца (`chown -R $USER db`) или используйте отдельный каталог (`DOBRIKA_DB_PATH=/tmp/dobrika-db`).
- **Порт занят** — проверьте `sudo ss -lptn 'sport = :8088'` или поменяйте `DOBRIKA_PORT`.
- **Метрики не видны в Grafana** — убедитесь, что порт‑форвард активен и Prometheus (`http://localhost:9090/targets`) показывает target `UP`.
- **Логи запросов не появляются** — проверьте переменную `DOBRIKA_LOG_REQUESTS` и перезапустите сервис после изменения.

---

## Project Layout

- `src/server/` — HTTP‑сервер и запуск (`main.cpp`, `web_server.cpp`)
- `src/xapian_processor/` — работа с Xapian (индексация, поиск, бэкапы)
- `src/tools/` — утилиты (генератор конфигурации и пр.)
- `dev/` — тесты, pytest-fixtures, данные для нагрузочного прогона
- `monitoring/` — локальный Prometheus + Grafana для просмотра метрик
- `deployments/k8s/` — Kubernetes manifests

---

На этом всё: собирайте, запускайте, смотрите метрики и логи. Если нужна дополнительная автоматизация или дашборды — добавляйте поверх существующей структуры. Удачи! 