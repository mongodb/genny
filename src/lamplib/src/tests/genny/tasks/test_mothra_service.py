from re import M
from unittest import mock
import pytest
from unittest.mock import call, patch
from genny.tasks.mothra_service import MOTHRA_REPO, MothraService, TEAMS_FILES_PATHS


@pytest.fixture
def data():
    return {
        "teams": [
            {
                "name": "team1",
                "support_slack_channel_name": "team1_support",
                "support_slack_channel_id": "team1_id",
            },
            {
                "name": "team2",
                "support_slack_channel_name": "team2_support",
                "support_slack_channel_id": "team2_id",
            },
        ]
    }


class TestMothraService:

    @patch("genny.tasks.mothra_service.subprocess")
    @patch("genny.tasks.mothra_service.tempfile")
    @patch("genny.tasks.mothra_service.open")
    @patch("genny.tasks.mothra_service.yaml.safe_load")
    def test_get_team(self, mock_safe_load, mock_open, mock_tempfile, mock_subprocess, data):
        # Mock the subprocess.run method to avoid actual cloning of the repository
        mock_tempfile.TemporaryDirectory.return_value.__enter__.return_value = "temp_dir"
        mock_subprocess.run.return_value = None
        mock_safe_load.return_value = data

        service = MothraService()

        mock_subprocess.run.assert_called_once_with(
            ["git", "clone", "--depth=1", MOTHRA_REPO, "temp_dir"]
        )
        mock_open.assert_has_calls(
            [call(f"temp_dir/{file}") for file in TEAMS_FILES_PATHS], any_order=True
        )
        assert service.get_team("team1") is not None
        assert service.get_team("team2") is not None
