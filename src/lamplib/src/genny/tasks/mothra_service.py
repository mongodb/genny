import os
from typing import NamedTuple, Optional
from unicodedata import name
from dotenv import load_dotenv
from github import Github
from github import Auth
from github.ContentFile import ContentFile
import yaml

MOTHRA_REPO = "10gen/mothra"
TEAMS_DIR = "mothra/teams"
TEAMS_FILES = ["cloud.yaml", "database.yaml", "rnd_dev_prod.yaml", "star.yaml"]
TEAMS_FILES_PATHS = [f"{TEAMS_DIR}/{file}" for file in TEAMS_FILES]


class Team(NamedTuple):
    name: str
    support_slack_channel_name: Optional[str] = None
    support_slack_channel_id: Optional[str] = None


class MothraService:

    def __init__(self):
        self.team_map = self._load_team_map()

    def get_team(self, team_name: str) -> Optional[Team]:
        return self.team_map.get(team_name, None)

    def _load_team_map(self) -> dict[str, Team]:
        github = Github(auth=Auth.Token(self._get_github_token()))
        repo = github.get_repo(MOTHRA_REPO)
        teams = []
        team_map = {}
        for file_path in TEAMS_FILES_PATHS:
            # TODO: Remove hardcoded ref
            content = repo.get_contents(file_path, ref="DEVPROD-5933")

            # The get_contents method returns a list if the path is a directory, but we know the path is a file and assert here to keep the linter happy.
            assert isinstance(content, ContentFile)

            teams.extend(yaml.safe_load(content.decoded_content.decode())["teams"])

        for team in teams:
            # Filter out any keys that are not part of the Team model
            filtered_data = {key: value for key, value in team.items() if key in Team._fields}
            team_data = Team(**filtered_data)
            team_map[team_data.name] = team_data

        return team_map

    def _get_github_token(self) -> str:
        load_dotenv()
        token = os.getenv("GITHUB_ACCESS_TOKEN")
        if not token:
            raise ValueError("GITHUB_ACCESS_TOKEN is not set in the environment")

        return token
