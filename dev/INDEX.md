# Dobrika Search Testing - Quick Index

## 📍 Начните отсюда

### 1️⃣ Установка
```bash
cd dev
pip install -r requirements.txt
```

### 2️⃣ Быстрый запуск
```bash
pytest test_quality.py -v                    # Качество
pytest test_performance.py -v --benchmark-only  # Производительность
RUN_SERVER=1 pytest -v                       # Все с запуском сервера
```

## 🧪 Тесты

### Quality Tests (`test_quality.py`)
- 🏷️ Tag Search - 4 теста
- 🌍 Geo Search - 3 теста  
- 🏥 Health Check - 2 теста

**Запуск:**
```bash
pytest test_quality.py -v
pytest test_quality.py::TestTagSearch -v
pytest test_quality.py::TestGeoSearch -v
```

### Performance Tests (`test_performance.py`)
- 📈 Index Performance - 3 бенчмарка
- 🔍 Search Performance - 3 бенчмарка

**Запуск:**
```bash
pytest test_performance.py -v --benchmark-only
pytest test_performance.py::TestIndexPerformance -v
```

## 🛠️ Конфигурация

| Variable | Default | Назначение |
|----------|---------|-----------|
| `DOBRIKA_HOST` | 127.0.0.1 | Host сервера |
| `DOBRIKA_PORT` | 8088 | Port сервера |
| `RUN_SERVER` | - | Запустить сервер |
| `DOBRIKA_DB` | db_test | Database path |

**Примеры:**
```bash
DOBRIKA_HOST=localhost pytest -v
RUN_SERVER=1 pytest test_quality.py -v
DOBRIKA_DB=custom_db pytest -v
```