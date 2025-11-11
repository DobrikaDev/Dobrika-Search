import os
import shutil
import signal
import socket
import subprocess
import time
from contextlib import closing
from pathlib import Path
from typing import Optional

import pytest
import requests


def _get_env_bool(name: str, default: bool = False) -> bool:
    val = os.environ.get(name)
    if val is None:
        return default
    return str(val).strip().lower() in {"1", "true", "yes", "on"}


def _pick_free_port(preferred: Optional[int] = None) -> int:
    if preferred:
        # try preferred first
        with closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try:
                s.bind(("127.0.0.1", preferred))
                return preferred
            except OSError:
                pass
    with closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
        s.bind(("", 0))
        return s.getsockname()[1]


def _wait_for_health(url: str, timeout_s: float = 15.0) -> None:
    deadline = time.time() + timeout_s
    last_exc = None
    while time.time() < deadline:
        try:
            resp = requests.get(f"{url}/healthz", timeout=1.5)
            if resp.status_code == 200:
                return
        except Exception as exc:  # noqa: BLE001
            last_exc = exc
        time.sleep(0.25)
    if last_exc:
        raise RuntimeError(f"Server not healthy at {url}/healthz within timeout: {last_exc}")
    raise RuntimeError(f"Server not healthy at {url}/healthz within timeout")


@pytest.fixture(scope="session")
def server_url(tmp_path_factory: pytest.TempPathFactory) -> str:
    """
    Provides base URL of the Dobrika server.
    If RUN_SERVER=1, starts the binary from DOBRIKA_BINARY and waits for health.
    Environment variables honored:
      - DOBRIKA_BINARY: path to built server binary
      - DOBRIKA_ADDR / DOBRIKA_HOST: bind address (default 127.0.0.1)
      - DOBRIKA_PORT: port (default 8088 or a free port)
      - DOBRIKA_DB_PATH: database directory (default temp dir)
    """
    run_server = _get_env_bool("RUN_SERVER", False)
    addr = os.environ.get("DOBRIKA_ADDR") or os.environ.get("DOBRIKA_HOST") or "127.0.0.1"
    port_env = os.environ.get("DOBRIKA_PORT")
    port = _pick_free_port(int(port_env)) if port_env else _pick_free_port(8088)

    base_url = f"http://{addr}:{port}"

    if not run_server:
        # Assume server is already running at provided URL
        return base_url

    binary = os.environ.get("DOBRIKA_BINARY")
    if not binary:
        raise RuntimeError("RUN_SERVER=1 is set but DOBRIKA_BINARY is not provided")

    binary_path = Path(binary).resolve()
    if not binary_path.exists():
        raise FileNotFoundError(f"DOBRIKA_BINARY not found: {binary_path}")

    # Prepare DB path
    db_path = os.environ.get("DOBRIKA_DB_PATH")
    cleanup_db = False
    if not db_path:
        tmp_dir = tmp_path_factory.mktemp("dobrika_db")
        db_path = str(tmp_dir)
        cleanup_db = True

    # Environment for the server process
    env = os.environ.copy()
    env.setdefault("DOBRIKA_ADDR", addr)
    env["DOBRIKA_PORT"] = str(port)
    env.setdefault("DOBRIKA_DB_PATH", db_path)
    env.setdefault("DOBRIKA_COLD_MIN", "30")
    env.setdefault("DOBRIKA_HOT_MIN", "15")
    env.setdefault("DOBRIKA_SEARCH_OFFSET", "0")
    env.setdefault("DOBRIKA_SEARCH_LIMIT", "20")
    env.setdefault("DOBRIKA_GEO_INDEX", "9")

    proc = subprocess.Popen(
        [str(binary_path)],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        cwd=str(binary_path.parent),
        start_new_session=True,  # allow killing the whole process group
    )

    try:
        _wait_for_health(base_url, timeout_s=20.0)
    except Exception:
        # Dump some logs to help debugging before killing
        try:
            if proc.stderr:
                err = proc.stderr.read()
                print("Dobrika server stderr:\n", err)
        finally:
            os.killpg(proc.pid, signal.SIGTERM)
            proc.wait(timeout=5)
        raise

    yield base_url

    # Teardown
    try:
        os.killpg(proc.pid, signal.SIGTERM)
        proc.wait(timeout=10)
    except Exception:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except Exception:
            pass
    if cleanup_db:
        try:
            shutil.rmtree(db_path, ignore_errors=True)
        except Exception:
            pass


