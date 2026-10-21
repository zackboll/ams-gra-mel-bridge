#!/usr/bin/env python3
"""Retain exact GitHub job/artifact evidence and queue/runtime measurements."""

import argparse
from datetime import datetime
import json
from pathlib import Path
import subprocess


def api(path):
    return json.loads(subprocess.check_output(["gh", "api", path], text=True))


def seconds(start, end):
    if not start or not end:
        return None
    return max(0, (datetime.fromisoformat(end.replace("Z", "+00:00")) -
                   datetime.fromisoformat(start.replace("Z", "+00:00"))).total_seconds())


def collect(repository, run_id):
    base = f"repos/{repository}/actions/runs/{run_id}"
    run = api(base)
    attempt = run["run_attempt"]
    jobs = []
    page = 1
    while True:
        batch = api(f"{base}/attempts/{attempt}/jobs?per_page=100&page={page}")["jobs"]
        jobs.extend(batch)
        if len(batch) < 100:
            break
        page += 1
    artifacts = []
    page = 1
    while True:
        batch = api(f"{base}/artifacts?per_page=100&page={page}")["artifacts"]
        artifacts.extend(batch)
        if len(batch) < 100:
            break
        page += 1
    rows = [{"id": job["id"], "name": job["name"], "status": job["status"],
             "conclusion": job["conclusion"], "created_at": job["created_at"],
             "started_at": job["started_at"], "completed_at": job["completed_at"],
             "queue_seconds": seconds(job["created_at"], job["started_at"]),
             "runner_seconds": seconds(job["started_at"], job["completed_at"]),
             "steps": job["steps"]} for job in jobs]
    completed = [row["completed_at"] for row in rows if row["completed_at"]]
    active = [row for row in rows if row["conclusion"] != "skipped"]
    total = sum(row["runner_seconds"] or 0 for row in active)
    prepare = next((row for row in rows if row["name"] == "prepare-documentation"), None)
    aggregate = next((row for row in rows if row["name"] == "build"), None)
    shards = [row for row in rows if row["name"].startswith("enhanced-tests (")]
    critical = None
    if prepare and aggregate and len(shards) == 6 and all(
            row["runner_seconds"] is not None for row in [prepare, aggregate, *shards]):
        critical = prepare["runner_seconds"] + max(row["runner_seconds"] for row in shards) + aggregate["runner_seconds"]
    return {"repository": repository, "run_id": str(run_id), "run_attempt": attempt,
            "head_sha": run["head_sha"], "status": run["status"], "conclusion": run["conclusion"],
            "html_url": run["html_url"], "created_at": run["created_at"], "jobs": rows,
            "artifacts": artifacts, "completed_runner_minutes": total / 60,
            "qualification_critical_path_seconds_excluding_queues": critical,
            "baseline_speedup": 5322 / critical if critical else None,
            "observed_wall_seconds": seconds(run["created_at"], max(completed)) if completed else None,
            "qualification_complete": run["status"] == "completed" and run["conclusion"] == "success",
            "baseline_seconds": 5322,
            "note": "Incomplete runs have partial measurements; queue times use GitHub job timestamps."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_id", type=int)
    parser.add_argument("--repository", default="zackboll/ams-gra-mel-bridge")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.write_text(json.dumps(collect(args.repository, args.run_id), indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
