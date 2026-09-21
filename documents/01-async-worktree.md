# Async Work with Git Worktrees

This document is the worked example for running an LLM asynchronously.

The LLM works in its own worktree on its own branch.

The human works in the main checkout.

Git is the message channel.

## The Problem

- The model's context is a snapshot of the files as it last saw them.
- When the human edits a file the model has read, the model's next write is stale.
- The model can clobber the human change or build on an outdated version.
- A branch alone does not isolate anything.
- All branches in one checkout share one working tree.
- A git worktree is a separate directory with its own checkout.

## The Pattern

1. Create a worktree for the episode.
2. The LLM works only inside the worktree.
3. The LLM commits on its branch.
4. The human works in the main checkout.
5. When the episode settles, review the branch diff.
6. Merge the branch.

## The Worked Example

This transcript is verified.

It was executed against git 2.x on a scratch repository.

### 1. Baseline

```sh
git init -b main
# add files and commit
git commit -m "chore: baseline"
```

### 2. Create the LLM worktree

```sh
git worktree add ../llm-work -b llm/episode-42
```

Verified output:

```
/tmp/worktree-demo/main      bea24c9 [main]
/tmp/worktree-demo/llm-work  bea24c9 [llm/episode-42]
```

### 3. Parallel work

- The LLM commits inside `llm-work`.
- The human commits in the main checkout.
- Neither interferes with the other.

Verified output:

```
main log:
e739c20 docs: record design decision (human work)
bea24c9 chore: baseline

llm branch log:
524bf13 feat: add feature (LLM episode work)
bea24c9 chore: baseline
```

### 4. Review the episode before merging

```sh
git log --oneline main..llm/episode-42
git diff main...llm/episode-42 --stat
```

Verified output:

```
524bf13 feat: add feature (LLM episode work)
 src/feature.py | 2 ++
 1 file changed, 2 insertions(+)
```

The review surface is one commit and one file.

This is the size of a one-sitting review.

### 5. Merge

```sh
git merge --no-ff llm/episode-42
```

Verified output:

```
c5b3a4a merge: episode-42 (LLM feature work)
e739c20 docs: record design decision (human work)
524bf13 feat: add feature (LLM episode work)
bea24c9 chore: baseline
```

The merge is conflict-free when the streams touched disjoint files.

The merge is the check-the-mailbox moment.

### 6. Cleanup

```sh
git worktree remove ../llm-work
```

## Automation

- The merge can be automated along a spectrum.
- Level 1: run the merge command when the episode settles.
- Level 2: a small script or alias that tests, merges, and reports.
- Level 3: CI with a merge queue that merges on green.
- Automation must not silently eat the review gate.

Two safe designs:

- Automation merges into a staging branch; the human flips staging to main.
- Some episode classes are declared auto-mergeable and the risk is accepted.

A conflict is a feature.

A conflict forces a human decision where the two streams intersect.

## Alternatives

- Patch exchange: the model writes a diff and the human applies it when ready.
- Fork and pull request: the model pushes and opens a PR and the platform reviews.
- Serialized access: the environment prevents concurrent writes instead of isolating trees.

## Practice

- This pattern is used in practice.
- Tools built on it include dmux, gwt, gtr, and rove.
- Sources are listed in `02-tool-universe.md`.
