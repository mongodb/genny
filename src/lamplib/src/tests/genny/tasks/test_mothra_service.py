import os
import pytest
from unittest.mock import call, patch
from genny.tasks.mothra_service import MothraService, TEAMS_FILES_PATHS


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
    @patch("genny.tasks.mothra_service.open")
    @patch("genny.tasks.mothra_service.yaml.safe_load")
    def test_get_team(self, mock_safe_load, mock_open, data):
        mock_safe_load.return_value = data

        genny_repo_root = os.environ.get("GENNY_REPO_ROOT", ".")
        service = MothraService(genny_repo_root)

        mock_open.assert_has_calls(
            [call(f"{genny_repo_root}/mothra/{file}") for file in TEAMS_FILES_PATHS],
            any_order=True,
        )
        assert service.get_team("team1") is not None
        assert service.get_team("team2") is not None
