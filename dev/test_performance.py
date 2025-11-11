#!/usr/bin/env python3
"""Performance and load tests for Dobrika search server"""
import concurrent.futures
import json
import os
import time
from typing import Any, Dict, List

import pytest
import requests


class TestIndexPerformance:
    """Index operation performance tests"""
    
    @pytest.fixture
    def tasks_data(self):
        """Load test tasks"""
        tasks_file = os.path.join(
            os.path.dirname(__file__), "data", "bullets.json"
        )
        if not os.path.exists(tasks_file):
            pytest.skip(f"Test data not found: {tasks_file}")
        
        with open(tasks_file, "r", encoding="utf-8") as f:
            tasks = json.load(f)
        return tasks[:50]  # Use first 50 tasks
    
    def test_index_single_task(self, server_url, benchmark):
        """Benchmark single task indexing"""
        task = {
            "task_id": "perf_test_1",
            "task_name": "Performance Test Task",
            "task_desc": "Test task for performance measurement",
            "task_type": "TT_OnlineTask",
            "task_tags": ["performance", "test"]
        }
        
        def index_task():
            resp = requests.post(
                f"{server_url}/index",
                json=task,
                timeout=5.0
            )
            assert resp.status_code == 200
            return resp
        
        result = benchmark(index_task)
        assert result.status_code == 200
    
    def test_index_batch_concurrency_4(self, server_url, tasks_data, benchmark):
        """Benchmark batch indexing with concurrency=4"""
        def batch_index():
            successes = 0
            with requests.Session() as session:
                with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
                    futures = [
                        pool.submit(
                            session.post,
                            f"{server_url}/index",
                            json=task,
                            timeout=5.0
                        )
                        for task in tasks_data
                    ]
                    for fut in concurrent.futures.as_completed(futures):
                        resp = fut.result()
                        if resp.status_code == 200:
                            successes += 1
            return successes
        
        result = benchmark(batch_index)
        assert result == len(tasks_data)
    
    def test_index_batch_concurrency_8(self, server_url, tasks_data, benchmark):
        """Benchmark batch indexing with concurrency=8"""
        def batch_index():
            successes = 0
            with requests.Session() as session:
                with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
                    futures = [
                        pool.submit(
                            session.post,
                            f"{server_url}/index",
                            json=task,
                            timeout=5.0
                        )
                        for task in tasks_data
                    ]
                    for fut in concurrent.futures.as_completed(futures):
                        resp = fut.result()
                        if resp.status_code == 200:
                            successes += 1
            return successes
        
        result = benchmark(batch_index)
        assert result == len(tasks_data)


class TestSearchPerformance:
    """Search operation performance tests"""
    
    @pytest.fixture(autouse=True)
    def setup(self, server_url):
        """Index test data before search tests"""
        self.server_url = server_url
        tasks_file = os.path.join(
            os.path.dirname(__file__), "data", "bullets.json"
        )
        if not os.path.exists(tasks_file):
            pytest.skip(f"Test data not found: {tasks_file}")
        
        with open(tasks_file, "r", encoding="utf-8") as f:
            tasks = json.load(f)
        
        # Index first 50 tasks
        with requests.Session() as session:
            for task in tasks[:50]:
                session.post(f"{server_url}/index", json=task, timeout=5.0)
        
        time.sleep(1)  # Wait for indexing
    
    def test_tag_search_single(self, server_url, benchmark):
        """Benchmark single tag search"""
        def search():
            resp = requests.post(
                f"{server_url}/search",
                json={
                    "query_type": "QT_TagTasks",
                    "user_tags": ["test"]
                },
                timeout=5.0
            )
            assert resp.status_code == 200
            return resp
        
        result = benchmark(search)
        assert result.status_code == 200
    
    def test_geo_search_single(self, server_url, benchmark):
        """Benchmark single geo search"""
        def search():
            resp = requests.post(
                f"{server_url}/search",
                json={
                    "query_type": "QT_GeoTasks",
                    "geo_data": "55.7558,37.6173"
                },
                timeout=5.0
            )
            assert resp.status_code == 200
            return resp
        
        result = benchmark(search)
        assert result.status_code == 200
    
    def test_search_batch_concurrency_8(self, server_url, benchmark):
        """Benchmark batch search with concurrency=8"""
        search_queries = [
            {
                "query_type": "QT_TagTasks",
                "user_tags": ["test"]
            },
            {
                "query_type": "QT_GeoTasks",
                "geo_data": "55.7558,37.6173"
            }
        ]
        
        def batch_search():
            successes = 0
            with requests.Session() as session:
                with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
                    futures = []
                    for _ in range(100):
                        for query in search_queries:
                            futures.append(
                                pool.submit(
                                    session.post,
                                    f"{server_url}/search",
                                    json=query,
                                    timeout=5.0
                                )
                            )
                    
                    for fut in concurrent.futures.as_completed(futures):
                        resp = fut.result()
                        if resp.status_code == 200:
                            successes += 1
            return successes
        
        result = benchmark(batch_search)
        assert result == 200  # 100 iterations * 2 query types

