# The LLM Tool Universe

This document surveys LLM tools through the lens of the interaction pattern.

The lens asks four questions about a tool:

- Sync or async?
- How much human attention does it demand?
- What is the review surface?
- How does it integrate with git?

## Terminal Code Agents

- Aider is a terminal code agent that edits files and commits with git.
- Aider is git-native.
- Every Aider change is a diff to review.
- Aider refreshes its repo map and warns about externally edited files.
- Aider mitigates stale context but does not isolate it.
- Claude Code is a terminal agent with background modes.
- Background agents run and notify on completion.

## IDE-Integrated Agents

Cursor and GitHub Copilot embed agents in the editor.

Copilot has a background-agents pattern built on git worktrees.

## Worktree Multiplexers

- dmux runs parallel coding agents, each in its own worktree.
- gwt runs AI coding sessions per worktree.
- gtr is a git worktree runner.
- rove runs coding agents on parallel tasks with isolated worktrees.
- These tools exist because the worktree pattern is real practice.

## Harness Environments

- Agent harnesses orchestrate LLM work at scale.
- Harnesses dispatch work to subagents and notify on completion.
- Harnesses can serialize access so the human and the model never write concurrently.
- The environment hosting this conversation is such a harness.

## CI and PR-Based Agents

- OpenHands and similar agents take an issue and produce a pull request.
- The PR is the envelope.
- The platform review and merge queue are the async mechanics.

## Notebooks

- Jupyter notebooks are a thinking medium.
- Notebooks keep state, re-run cells, and mix text with results.
- Notebooks have hidden state and noisy diffs.

## Plain API and Scripting

- The model API can be driven directly.
- A queue of prompts can run as a batch job.
- Results land in files for later review.
- This is the email pattern with minimal tooling.

## Mapping to This Project

- Aider fits the review-surface pattern.
- A worktree gives Aider isolation.
- An episode file is the work order.
- A record file is the outcome.

## Sources

- June 2025 Coding Agent Report: evaluation of 15 coding agents. https://github.com/The-Focus-AI/june-2025-coding-agent-report
- ai-agent-benchmark: comparison of 80+ agents. https://github.com/murataslan1/ai-agent-benchmark
- Claude Code background agents. https://github.com/marc-shade/agentic-system-oss/blob/master/intelligent-agents/CLAUDE_CODE_BACKGROUND_AGENTS.md
- Background coding agents with git worktrees. https://crazyrouter.com/en/blog/background-coding-agents-without-lock-in-git-worktrees
- dmux. https://github.com/sterlingchapman/dmux
- gwt. https://github.com/slowestmonkey/gwt
- gtr. https://landscape.jimmysong.io/zh/projects/git-worktree-runner/
- rove. https://github.com/Sma1lboy/rove
- aider-desk worktree isolation docs. https://deepwiki.com/hotovo/aider-desk/2.5-git-worktrees-and-isolation
- Parallel AI coding context collisions. https://1devtool.com/blog/context-collisions-parallel-ai-coding
- OpenHands. https://github.com/OpenHands/OpenHands

## Status

This document is a living survey.

Verify tool names and features before relying on them.
