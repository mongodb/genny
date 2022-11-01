import os
import unittest
import requests
from unittest.mock import patch, Mock, call
import mongosync_actor as actor

ENV_VARS = {"MongosyncConnectionUrls": "http://localhost:27182"}
ENV_VARS_MULTIPLE = {
    "MongosyncConnectionUrls": "http://localhost:27182 http://localhost:27183 http://localhost:27184"
}


class TestMongosyncActor(unittest.TestCase):
    def test_required_env_vars(self):
        with self.assertRaisesRegex(Exception, "MongosyncConnectionUrls"):
            actor.poll(lambda x: x == 10, "progress")

    @patch.dict(os.environ, ENV_VARS)
    @patch.object(requests, "get")
    def test_poll_one_ms(self, mock_get):
        mock_get.return_value = Mock(status_code=200, json=lambda: {"progress": {"lag": 2}})
        actor.poll(lambda x: x > 10, "lag")
        mock_get.assert_called_once_with("http://localhost:27182/api/v1/progress")

    @patch.dict(os.environ, ENV_VARS_MULTIPLE)
    @patch.object(requests, "get")
    def test_poll_multiple_ms(self, mock_get):
        mock_get.return_value = Mock(status_code=200, json=lambda: {"progress": {"lag": 2}})
        actor.poll(lambda x: x > 10, "lag")
        expected_calls = [
            call("http://localhost:27182/api/v1/progress"),
            call("http://localhost:27183/api/v1/progress"),
            call("http://localhost:27184/api/v1/progress"),
        ]
        mock_get.assert_has_calls(expected_calls, any_order=True)

    @patch.dict(os.environ, ENV_VARS)
    @patch.object(requests, "post")
    def test_change_state(self, mock_post):
        actor.change_state("/api/v1/start", {})
        mock_post.assert_called_once_with("http://localhost:27182/api/v1/start", json={})

    @patch.dict(os.environ, ENV_VARS)
    @patch.object(requests, "post")
    def test_change_state_failed(self, mock_post):
        mock_post.return_value = Mock(status_code=200, json=lambda: {"success": False})
        with self.assertRaisesRegex(Exception, "route /api/v1/start"):
            actor.change_state("/api/v1/start", {})

    @patch.dict(os.environ, ENV_VARS_MULTIPLE)
    @patch.object(requests, "post")
    def test_change_state_multiple_ms(self, mock_post):
        actor.change_state("/api/v1/start", {})
        expected_calls = [
            call("http://localhost:27182/api/v1/start", json={}),
            call("http://localhost:27183/api/v1/start", json={}),
            call("http://localhost:27184/api/v1/start", json={}),
        ]
        mock_post.assert_has_calls(expected_calls, any_order=True)
