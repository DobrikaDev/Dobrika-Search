#!/usr/bin/env python3
import argparse
import concurrent.futures
import json
import os
import random
import signal
import subprocess
import sys
import time
from typing import Any, Dict, List, Optional, Tuple

import requests


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Dobrika server load tester")
    parser.add_argument("--host", default="127.0.0.1", help="Server host")
    parser.add_argument("--port", type=int, default=8088, help="Server port")
    parser.add_argument(
        "--db",
        default="db",
        help="Database path (used when starting server from this script)",
    )
    parser.add_argument(
        "--tasks-file",
        default=os.path.join(
            os.path.dirname(__file__), "data", "bullets.json"
        ),
        help="Path to JSON file with tasks to index",
    )
    parser.add_argument(
        "--concurrency",
        type=int,
        default=8,
        help="Concurrent workers for requests",
    )
    parser.add_argument(
        "--index-limit",
        type=int,
        default=50,
        help="Max tasks to index from tasks-file",
    )
    parser.add_argument(
        "--search-requests",
        type=int,
        default=200,
        help="Number of search requests to execute",
    )
    parser.add_argument(
        "--run-server",
        action="store_true",
        help="Start server binary as a subprocess",
    )
    parser.add_argument(
        "--binary",
        default=os.path.join(
            os.path.dirname(os.path.dirname(__file__)),
            "build",
            "dobrika_server_main",
        ),
        help="Path to built server binary",
    )
    parser.add_argument(
        "--startup-timeout",
        type=float,
        default=20.0,
        help="Seconds to wait for server readiness",
    )
    return parser.parse_args()


def wait_for_server(base_url: str, timeout_s: float) -> bool:
    deadline = time.time() + timeout_s
    last_error: Optional[str] = None
    while time.time() < deadline:
        try:
            r = requests.get(f"{base_url}/healthz", timeout=1.5)
            if r.status_code == 200:
                return True
            last_error = f"HTTP {r.status_code}"
        except Exception as e:
            last_error = str(e)
        time.sleep(0.25)
    if last_error:
        print(f"Server not ready: {last_error}", file=sys.stderr)
    return False


def start_server_subprocess(binary_path: str, host: str, port: int, db: str) -> subprocess.Popen:
    env = os.environ.copy()
    env["DOBRIKA_ADDR"] = host
    env["DOBRIKA_PORT"] = str(port)
    env["DOBRIKA_DB_PATH"] = db
    os.makedirs(db, exist_ok=True)
    return subprocess.Popen([binary_path], env=env, stdout=sys.stdout, stderr=sys.stderr)


def safe_post_json(session: requests.Session, url: str, payload: Dict[str, Any]) -> Tuple[bool, float]:
    t0 = time.perf_counter()
    try:
        r = session.post(url, json=payload, timeout=5.0)
        ok = r.status_code == 200
        return ok, (time.perf_counter() - t0)
    except Exception:
        return False, (time.perf_counter() - t0)


def index_tasks(base_url: str, tasks: List[Dict[str, Any]], concurrency: int) -> Dict[str, Any]:
    url = f"{base_url}/index"
    total = len(tasks)
    successes = 0
    latencies: List[float] = []
    with requests.Session() as session:
        with concurrent.futures.ThreadPoolExecutor(max_workers=concurrency) as pool:
            futures = [pool.submit(safe_post_json, session, url, t) for t in tasks]
            for fut in concurrent.futures.as_completed(futures):
                ok, elapsed = fut.result()
                if ok:
                    successes += 1
                latencies.append(elapsed)
    return {
        "total": total,
        "successes": successes,
        "failures": total - successes,
        "p50_ms": round(percentile_ms(latencies, 50), 2),
        "p95_ms": round(percentile_ms(latencies, 95), 2),
        "avg_ms": round(average_ms(latencies), 2),
    }


def run_searches(base_url: str, queries: List[str], num_requests: int, concurrency: int) -> Dict[str, Any]:
    url = f"{base_url}/search"
    successes = 0
    latencies: List[float] = []
    payloads = [
        {
            "user_query": random.choice(queries),
            "geo_data": random_geo_hint(),
            "query_type": "default",
        }
        for _ in range(num_requests)
    ]
    with requests.Session() as session:
        with concurrent.futures.ThreadPoolExecutor(max_workers=concurrency) as pool:
            futures = [pool.submit(safe_post_json, session, url, pl) for pl in payloads]
            for fut in concurrent.futures.as_completed(futures):
                ok, elapsed = fut.result()
                if ok:
                    successes += 1
                latencies.append(elapsed)
    return {
        "total": num_requests,
        "successes": successes,
        "failures": num_requests - successes,
        "p50_ms": round(percentile_ms(latencies, 50), 2),
        "p95_ms": round(percentile_ms(latencies, 95), 2),
        "avg_ms": round(average_ms(latencies), 2),
    }


def percentile_ms(samples: List[float], p: float) -> float:
    if not samples:
        return 0.0
    s = sorted(samples)
    k = (len(s) - 1) * (p / 100.0)
    f = int(k)
    c = min(f + 1, len(s) - 1)
    if f == c:
        return s[int(k)] * 1000.0
    d0 = s[f] * (c - k)
    d1 = s[c] * (k - f)
    return (d0 + d1) * 1000.0


def average_ms(samples: List[float]) -> float:
    if not samples:
        return 0.0
    return (sum(samples) / len(samples)) * 1000.0


def extract_queries_from_tasks(tasks: List[Dict[str, Any]]) -> List[str]:
    queries: List[str] = []
    for t in tasks:
        name = str(t.get("task_name", ""))
        desc = str(t.get("task_desc", ""))
        queries.extend([w for w in (name + " " + desc).replace(",", " ").split() if len(w) >= 3])
    # fallback
    if not queries:
        queries = ["demo", "task", "search", "index", "geo", "default"]
    # deduplicate while preserving order
    seen = set()
    uniq: List[str] = []
    for q in queries:
        if q not in seen:
            seen.add(q)
            uniq.append(q)
    return uniq[:100]


def random_geo_hint() -> str:
    # Slightly randomized geo to exercise geo parsing
    lat = random.uniform(-60, 60)
    lon = random.uniform(-160, 160)
    return f"{lat:.4f},{lon:.4f}"


def main() -> int:
    args = parse_args()
    base_url = f"http://{args.host}:{args.port}"

    server_proc: Optional[subprocess.Popen] = None
    try:
        if args.run_server:
            if not os.path.isfile(args.binary):
                print(f"Server binary not found: {args.binary}", file=sys.stderr)
                return 2
            print(f"Starting server: {args.binary} on {args.host}:{args.port} (db={args.db})")
            server_proc = start_server_subprocess(args.binary, args.host, args.port, args.db)
            if not wait_for_server(base_url, args.startup_timeout):
                print("Server failed to become ready in time", file=sys.stderr)
                return 3
            print("Server is ready")
        else:
            if not wait_for_server(base_url, 3.0):
                print("Server is not responding on /healthz", file=sys.stderr)
                return 4

        with open(args.tasks_file, "r", encoding="utf-8") as f:
            tasks_all: List[Dict[str, Any]] = json.load(f)
        tasks = tasks_all[: args.index_limit]

        print(f"Indexing {len(tasks)} tasks with concurrency={args.concurrency} ...")
        t0 = time.perf_counter()
        idx_stats = index_tasks(base_url, tasks, args.concurrency)
        t1 = time.perf_counter()
        elapsed_idx = t1 - t0
        print(
            f"Indexed {idx_stats['successes']}/{idx_stats['total']} "
            f"(fail={idx_stats['failures']}), "
            f"avg={idx_stats['avg_ms']}ms p50={idx_stats['p50_ms']}ms p95={idx_stats['p95_ms']}ms, "
            f"time={elapsed_idx:.2f}s"
        )

        queries = extract_queries_from_tasks(tasks)
        print(f"Running {args.search_requests} searches (unique terms={len(queries)}) ...")
        t0 = time.perf_counter()
        srch_stats = run_searches(base_url, queries, args.search_requests, args.concurrency)
        t1 = time.perf_counter()
        elapsed_srch = t1 - t0
        print(
            f"Search ok {srch_stats['successes']}/{srch_stats['total']} "
            f"(fail={srch_stats['failures']}), "
            f"avg={srch_stats['avg_ms']}ms p50={srch_stats['p50_ms']}ms p95={srch_stats['p95_ms']}ms, "
            f"time={elapsed_srch:.2f}s"
        )

        return 0
    finally:
        if server_proc is not None:
            try:
                server_proc.send_signal(signal.SIGINT)
                server_proc.wait(timeout=5.0)
            except Exception:
                try:
                    server_proc.terminate()
                except Exception:
                    pass


if __name__ == "__main__":
    sys.exit(main())


