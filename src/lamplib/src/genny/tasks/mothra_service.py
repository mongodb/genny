from dataclasses import dataclass
import os
from typing import Optional
import structlog
import yaml

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
    def __init__(self, genny_repo_root: str):
        self.genny_repo_root = genny_repo_root
        self.team_map = self._load_team_map()

    def get_team(self, team_name: str) -> Optional[Team]:
        return self.team_map.get(team_name, None)

    def _load_team_map(self) -> dict[str, Team]:
        mothra_dir = os.path.join(self.genny_repo_root, "mothra")
        if not os.path.exists(mothra_dir):
            raise FileNotFoundError(
                "Mothra repository does not exist on your system. Please clone the repository in the root of your genny repo using: git clone https://github.com/10gen/mothra.git"
            )

        teams = []
        team_map = {}
        for file in TEAMS_FILES_PATHS:
            with open(f"{mothra_dir}/{file}") as f:
                teams.extend(yaml.safe_load(f)["teams"])

        for team in teams:
            team_data = Team.create(**team)
            team_map[team_data.name] = team_data

        return team_map
