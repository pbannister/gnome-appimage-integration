# Lessons from the MI25 Fan-Service Project

This document records the process lessons from the MI25 fan-service project.

The MI25 work surfaced three lessons: a record must track the live state, generated build trees are path-bound, and test runs deserve a timestamped log.

Each lesson names where the skeleton now encodes it.

## 1. A record must track the live state

- The record `09-project-site.md` said "not yet implemented" after the deployment was already live.
- When a task changes a status, update the referenced record in the same commit.
- Encoded in: `prompts/02-workflow.md` §7.1, `records/README.md`.

## 2. Generated build trees are path-bound

- A stale CMakeCache broke the MI25 build after the repository mount path changed (SSHFS versus native mount).
- Build trees embed absolute paths; clean them when the repository is reached through a different path.
- Encoded in: `prompts/03-conventions.md` §6.

## 3. Test runs deserve a timestamped log

- The MI25 project writes `logs/YYYY-MM-DD-HH-MM-SS-test-run.log`.
- The test runner mirrors its transcript into the same timestamped pattern.
- Encoded in: `scripts/tests-run.sh`.
