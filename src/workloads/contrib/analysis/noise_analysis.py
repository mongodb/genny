import argparse
import json
import numpy as np

from dataclasses import dataclass
from pathlib import Path

def rnd(number):
    """Round a number to 4 significant digits, without going to scientific notation."""
    return np.format_float_positional(
        number, 
        precision=4,
        unique=False,
        fractional=False,
        trim="k",
    )


@dataclass
class NoiseAnalysisResult:
    actor: str
    min: float
    max: float
    mean: float

    @property
    def relative_range(self):
        return (self.max - self.min) / self.mean


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--files", nargs="+")
    parser.add_argument("--metric", default="throughput")
    parser.add_argument("--summary-statistic", default="ops per second")
    return parser.parse_args()

def main():
    args = parse_args()

    summary_files = {
        path.split("/")[-1]: json.loads(Path(path).read_text())
        for path in args.files
    }

    metrics_by_actor = {}
    for _, summary in summary_files.items():
        for actor, metrics in summary.items():
            metrics_by_actor[actor] = metrics_by_actor.get(actor, []) + [metrics]

    stats_by_actor = {}
    for actor, metrics_list in metrics_by_actor.items():
        for metrics in metrics_list:
            stats_by_actor[actor] = (
                stats_by_actor.get(actor, [])
                + [metrics[args.metric][args.summary_statistic]]
            )

    results = [
        NoiseAnalysisResult(
            actor,
            min(stats),
            max(stats),
            sum(stats) / len(stats),
        )
        for actor, stats in stats_by_actor.items()
    ]

    results.sort(key=lambda x: x.relative_range, reverse=True)
    for result in results:
        print(result.actor, rnd(result.relative_range))

if __name__ == "__main__":
    main()
