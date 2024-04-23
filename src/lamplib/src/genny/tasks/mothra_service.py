from dataclasses import dataclass
import subprocess
import tempfile
from typing import Optional
import structlog
import yaml

MOTHRA_REPO = "git@github.com:10gen/mothra.git"
TEAMS_DIR = "mothra/teams"
TEAMS_FILES = ["cloud.yaml", "database.yaml", "rnd_dev_prod.yaml", "star.yaml"]
TEAMS_FILES_PATHS = [f"{TEAMS_DIR}/{file}" for file in TEAMS_FILES]
SLOG = structlog.get_logger(__name__)


@dataclass
class Team:
    name: str
    support_slack_channel_name: Optional[str] = None
    support_slack_channel_id: Optional[str] = None

    @classmethod
    def create(cls, **kwargs) -> "Team":
        return cls(**{k: v for k, v in kwargs.items() if k in cls.__annotations__})


class MothraService:

    def __init__(self):
        self.team_map = self._load_team_map()

    def get_team(self, team_name: str) -> Optional[Team]:
        return self.team_map.get(team_name, None)

    def _load_team_map(self) -> dict[str, Team]:
        SLOG.info("Cloning Mothra repository to get team information.")
        with tempfile.TemporaryDirectory() as temp_dir:
            subprocess.run(["git", "clone", "--depth=1", MOTHRA_REPO, temp_dir])
            teams = []
            team_map = {}
            for file in TEAMS_FILES_PATHS:
                with open(f"{temp_dir}/{file}") as f:
                    teams.extend(yaml.safe_load(f)["teams"])

            for team in teams:
                team_data = Team.create(**team)
                team_map[team_data.name] = team_data

            return team_map
