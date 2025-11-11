#!/usr/bin/env python3
"""Quality tests for Dobrika search server"""
import time

import pytest
import requests


class TestTagSearch:
    """Tests for tag-based search functionality"""
    
    @pytest.fixture(autouse=True)
    def setup(self, server_url):
        """Index test tasks before each test"""
        self.base_url = server_url
        self.url_index = f"{server_url}/index"
        self.url_search = f"{server_url}/search"
        
        self.test_tasks = [
            {
                "task_id": "tag_test_1",
                "task_name": "Frontend Task",
                "task_desc": "Build UI",
                "task_type": "TT_OnlineTask",
                "task_tags": ["frontend", "react", "javascript"]
            },
            {
                "task_id": "tag_test_2",
                "task_name": "Backend Task",
                "task_desc": "Build API",
                "task_type": "TT_OnlineTask",
                "task_tags": ["backend", "nodejs", "javascript"]
            },
            {
                "task_id": "tag_test_3",
                "task_name": "Fullstack Task",
                "task_desc": "Full development",
                "task_type": "TT_OnlineTask",
                "task_tags": ["frontend", "backend", "react", "nodejs"]
            },
        ]
        
        # Index test tasks
        with requests.Session() as session:
            for task in self.test_tasks:
                resp = session.post(self.url_index, json=task, timeout=5.0)
                assert resp.status_code == 200, f"Failed to index task: {resp.text}"
        
        time.sleep(1)  # Wait for indexing
    
    def test_search_javascript_tag(self):
        """Search for javascript tag should return all three test tasks"""
        resp = requests.post(self.url_search, json={
            "query_type": "QT_TagTasks",
            "user_tags": ["javascript"]
        }, timeout=5.0)
        
        assert resp.status_code == 200
        task_ids = resp.json().get("task_id", [])
        test_task_ids = [t for t in task_ids if t.startswith("tag_test_")]
        
        assert len(test_task_ids) == 2, f"Expected 2 tasks with javascript, got {len(test_task_ids)}: {test_task_ids}"
    
    def test_search_frontend_and_react_tags(self):
        """Search for frontend + react should find both tag_test_1 and tag_test_3"""
        resp = requests.post(self.url_search, json={
            "query_type": "QT_TagTasks",
            "user_tags": ["frontend", "react"]
        }, timeout=5.0)
        
        assert resp.status_code == 200
        task_ids = resp.json().get("task_id", [])
        test_task_ids = [t for t in task_ids if t.startswith("tag_test_")]
        
        assert "tag_test_3" in test_task_ids, f"Expected tag_test_3 in results, got: {test_task_ids}"
        assert "tag_test_1" in test_task_ids, f"Expected tag_test_1 in results, got: {test_task_ids}"
    
    def test_search_backend_tag(self):
        """Search for backend tag should return tag_test_2 and tag_test_3"""
        resp = requests.post(self.url_search, json={
            "query_type": "QT_TagTasks",
            "user_tags": ["backend"]
        }, timeout=5.0)
        
        assert resp.status_code == 200
        task_ids = resp.json().get("task_id", [])
        test_task_ids = [t for t in task_ids if t.startswith("tag_test_")]
        
        assert len(test_task_ids) == 2, f"Expected 2 tasks with backend, got {len(test_task_ids)}: {test_task_ids}"
        assert "tag_test_2" in test_task_ids
        assert "tag_test_3" in test_task_ids
    
    def test_search_non_existent_tag(self):
        """Search for non-existent tag should return no results"""
        resp = requests.post(self.url_search, json={
            "query_type": "QT_TagTasks",
            "user_tags": ["python_lang_xyz_nonexistent"]
        }, timeout=5.0)
        
        assert resp.status_code == 200
        task_ids = resp.json().get("task_id", [])
        
        assert len(task_ids) == 0, f"Expected 0 tasks for non-existent tag, got {len(task_ids)}"


class TestGeoSearch:
    """Tests for geo-based search functionality"""
    
    @pytest.fixture(autouse=True)
    def setup(self, server_url):
        """Index test tasks before each test"""
        self.base_url = server_url
        self.url_index = f"{server_url}/index"
        self.url_search = f"{server_url}/search"
        
        self.test_tasks = [
            {
                "task_id": "geo_test_1",
                "task_name": "Moscow Center Task",
                "task_desc": "Task in Moscow center",
                "task_type": "TT_OnlineTask",
                "geo_data": "55.7558,37.6173"  # Red Square
            },
            {
                "task_id": "geo_test_2",
                "task_name": "Moscow South Task",
                "task_desc": "Task in South Moscow",
                "task_type": "TT_OnlineTask",
                "geo_data": "55.6195,37.5890"  # South Moscow
            },
            {
                "task_id": "geo_test_3",
                "task_name": "Moscow North Task",
                "task_desc": "Task in North Moscow",
                "task_type": "TT_OnlineTask",
                "geo_data": "55.8706,37.6350"  # North Moscow
            },
            {
                "task_id": "geo_test_4",
                "task_name": "SPB Task",
                "task_desc": "Task in St Petersburg",
                "task_type": "TT_OnlineTask",
                "geo_data": "59.9311,30.3609"  # St Petersburg
            },
        ]
        
        # Index test tasks
        with requests.Session() as session:
            for task in self.test_tasks:
                resp = session.post(self.url_index, json=task, timeout=5.0)
                assert resp.status_code == 200, f"Failed to index task: {resp.text}"
        
        time.sleep(1)  # Wait for indexing
    
    def test_search_moscow_center(self):
        """Search near Moscow center should return Moscow geo_test tasks first"""
        resp = requests.post(self.url_search, json={
            "query_type": "QT_GeoTasks",
            "geo_data": "55.7558,37.6173"
        }, timeout=5.0)
        
        assert resp.status_code == 200
        task_ids = resp.json().get("task_id", [])
        geo_test_ids = [t for t in task_ids if t.startswith("geo_test_")]
        
        assert len(geo_test_ids) >= 1, f"Expected at least 1 geo_test task, got {len(geo_test_ids)}"
        assert geo_test_ids[0] in ["geo_test_1", "geo_test_2", "geo_test_3"], \
            f"Expected Moscow task first, got: {geo_test_ids}"
    
    def test_search_st_petersburg(self):
        """Search near St Petersburg should return geo_test_4"""
        resp = requests.post(self.url_search, json={
            "query_type": "QT_GeoTasks",
            "geo_data": "59.9311,30.3609"
        }, timeout=5.0)
        
        assert resp.status_code == 200
        task_ids = resp.json().get("task_id", [])
        geo_test_ids = [t for t in task_ids if t.startswith("geo_test_")]
        
        assert "geo_test_4" in geo_test_ids, f"Expected geo_test_4 in results, got: {geo_test_ids}"
    
    def test_geo_search_returns_results(self):
        """Geo search should return our test tasks"""
        resp = requests.post(self.url_search, json={
            "query_type": "QT_GeoTasks",
            "geo_data": "55.7558,37.6173"
        }, timeout=5.0)
        
        assert resp.status_code == 200
        task_ids = resp.json().get("task_id", [])
        geo_test_ids = [t for t in task_ids if t.startswith("geo_test_")]
        
        assert len(geo_test_ids) >= 1, "Geo search should return results"


class TestHealthCheck:
    """Basic server health checks"""
    
    def test_server_health(self, server_url):
        """Server should respond to health check"""
        resp = requests.get(f"{server_url}/healthz", timeout=5.0)
        assert resp.status_code == 200
        assert "SearchHealthOk" in resp.text
    
    def test_invalid_search_query_type(self, server_url):
        """Invalid query type should return error"""
        resp = requests.post(f"{server_url}/search", json={
            "query_type": "INVALID_TYPE",
            "user_tags": []
        }, timeout=5.0)
        
        assert resp.status_code == 200
        assert resp.json().get("status") in ["SearchUnknownType", "SearchInvalidJson"]


class TestTextSearch:
    """Tests for BM25 text search in task_name and task_desc"""

    @pytest.fixture(autouse=True)
    def setup(self, server_url):
        """Index test tasks before each test"""
        self.base_url = server_url
        self.url_index = f"{server_url}/index"
        self.url_search = f"{server_url}/search"

        self.test_tasks = [
            {
                "task_id": "text_test_1",
                "task_name": "React frontend разработчик",
                "task_desc": "Строить UI дашборд на react",
                "task_type": "TT_OnlineTask",
            },
            {
                "task_id": "text_test_2",
                "task_name": "Python backend инженер",
                "task_desc": "Реализация микросервиса на FastApi",
                "task_type": "TT_OnlineTask",
            },
            {
                "task_id": "text_test_3",
                "task_name": "Fullstack JavaScript разрабочти",
                "task_desc": "Node.js and React разработчик приложений",
                "task_type": "TT_OnlineTask",
            },
        ]

        with requests.Session() as session:
            for task in self.test_tasks:
                resp = session.post(self.url_index, json=task, timeout=5.0)
                assert resp.status_code == 200, f"Failed to index task: {resp.text}"

        time.sleep(1)  # Wait for indexing

    def test_search_react_developer(self):
        """'react developer' should rank text_test_1 first and include text_test_3"""
        resp = requests.post(self.url_search, json={
            "user_query": "react разработчик"
        }, timeout=5.0)

        assert resp.status_code == 200
        task_ids = resp.json().get("task_id", [])
        text_ids = [t for t in task_ids if t.startswith("text_test_")]

        assert "text_test_1" in text_ids, f"Expected text_test_1 in results, got: {text_ids}"
        # If both present, ensure text_test_1 ranks before text_test_3
        if "text_test_3" in text_ids:
            assert text_ids.index("text_test_1") < text_ids.index("text_test_3"), \
                f"Expected text_test_1 ranked before text_test_3: {text_ids}"

    def test_search_python_backend(self):
        """'python backend' should return text_test_2"""
        resp = requests.post(self.url_search, json={
            "user_query": "python backend разработчик"
        }, timeout=5.0)

        assert resp.status_code == 200
        task_ids = resp.json().get("task_id", [])
        text_ids = [t for t in task_ids if t.startswith("text_test_")]

        assert "text_test_2" in text_ids, f"Expected text_test_2 in results, got: {text_ids}"
        # If multiple, ensure text_test_2 ranks first among text_test_*
        if text_ids:
            assert text_ids[0] == "text_test_2", f"Expected text_test_2 first, got: {text_ids}"

    def test_search_fastapi_keyword(self):
        """'FastAPI' in description should match text_test_2"""
        resp = requests.post(self.url_search, json={
            "user_query": "FastAPI"
        }, timeout=5.0)

        assert resp.status_code == 200
        task_ids = resp.json().get("task_id", [])
        assert "text_test_2" in task_ids, f"Expected text_test_2 in results, got: {task_ids}"
