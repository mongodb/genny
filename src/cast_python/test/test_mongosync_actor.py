import unittest
import builtins
import requests
from unittest.mock import patch, Mock, call, mock_open
import mongosync_actor as actor

ONE_MS = """
EnvironmentDetails:
  MongosyncConnectionURIs:
    - "http://localhost:27182"
"""

MULTIPLE_MS = """
EnvironmentDetails:
  MongosyncConnectionURIs:
    - "http://localhost:27182"
    - "http://localhost:27183"
    - "http://localhost:27184"
"""


class TestMongosyncActor(unittest.TestCase):
    @patch.object(builtins, "open", new_callable=mock_open, read_data="foo: bar")
    def test_required_config(self, mock_open_file):
        with self.assertRaisesRegex(Exception, "MongosyncConnectionURIs"):
            actor.poll("", lambda x: x == 10, "progress")

    @patch.object(builtins, "open", new_callable=mock_open, read_data=ONE_MS)
    @patch.object(requests, "get")
    def test_poll_one_ms(self, mock_get, mock_open_file):
        mock_get.return_value = Mock(status_code=200, json=lambda: {"progress": {"lag": 2}})
        actor.poll("", lambda x: x > 10, "lag")
        mock_get.assert_called_once_with("http://localhost:27182/api/v1/progress")

    @patch.object(builtins, "open", new_callable=mock_open, read_data=MULTIPLE_MS)
    @patch.object(requests, "get")
    def test_poll_multiple_ms(self, mock_get, mock_open_file):
        mock_get.return_value = Mock(status_code=200, json=lambda: {"progress": {"lag": 2}})
        actor.poll("", lambda x: x > 10, "lag")
        expected_calls = [
            call("http://localhost:27182/api/v1/progress"),
            call("http://localhost:27183/api/v1/progress"),
            call("http://localhost:27184/api/v1/progress"),
        ]
        mock_get.assert_has_calls(expected_calls, any_order=True)

    @patch.object(builtins, "open", new_callable=mock_open, read_data=ONE_MS)
    @patch.object(requests, "post")
    def test_change_state(self, mock_post, mock_open_file):
        actor.change_state("", "/api/v1/start", {})
        mock_post.assert_called_once_with("http://localhost:27182/api/v1/start", json={})

    @patch.object(builtins, "open", new_callable=mock_open, read_data=ONE_MS)
    @patch.object(requests, "post")
    def test_change_state_failed(self, mock_post, mock_open_file):
        mock_post.return_value = Mock(status_code=200, json=lambda: {"success": False})
        with self.assertRaisesRegex(Exception, "route /api/v1/start"):
            actor.change_state("", "/api/v1/start", {})

    @patch.object(builtins, "open", new_callable=mock_open, read_data=MULTIPLE_MS)
    @patch.object(requests, "post")
    def test_change_state_multiple_ms(self, mock_post, mock_open_file):
        actor.change_state("", "/api/v1/start", {})
        expected_calls = [
            call("http://localhost:27182/api/v1/start", json={}),
            call("http://localhost:27183/api/v1/start", json={}),
            call("http://localhost:27184/api/v1/start", json={}),
        ]
        mock_post.assert_has_calls(expected_calls, any_order=True)
