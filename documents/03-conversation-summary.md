# Conversation Summary

This document summarizes a conversation about working with an LLM.

The conversation started as a question about interaction patterns.

It ended with changes to this skeleton.

## The Pattern

- The unit of work is a work item, not a conversation.
- A prompt is a work order, not a whitepaper.
- Group strongly related items into an episode with one goal and one acceptance.
- Phrase risk items as questions and put them first.
- Treat the TODO list as a queue of episodes.
- Decouple human attention from the model's process.
- Dispatch work asynchronously and review artifacts later.
- Use git as the message channel.
- Keep the human as the synthesis point and final authority.
- Use worktrees to isolate LLM work.
- Separate intent from record.
- Use notebooks for exploration, not for work items.
- The full pattern is in `00-pattern-of-interaction.md`.

## The Skeleton Changes

- Captured the interaction pattern in `00-pattern-of-interaction.md`.
- Added the async-worktree worked example in `01-async-worktree.md`.
- Added the tool-universe survey in `02-tool-universe.md`.
- Added `prompts/how-to-write-episodes.md` and `prompts/episodes/01-episode-template.md`.
- Established the record convention in `records/README.md`.
- Registered `documents/` and `records/` in the contract, conventions, README map, and skeleton test.
- Renamed `docs/` to `documents/`.
- Codified whole-word naming in `prompts/03-conventions.md`.
- Codified one-item-per-line shell lists in `prompts/03-conventions.md`.
- Reformatted episode rules as Markdown lists.
- Noted the DELTA provenance as an open question.
- Added open questions to `TODO.md`.

## Preferences

- Use whole words in directory and filenames.
- Break long quoted lists in shell scripts to one item per line.
- Format rule lists as Markdown lists.
- Keep the skeleton tool-agnostic where possible.
- Communicate with substance only; no mechanical compliments.

## Static-Site Discussion

- The skeleton demonstrates static-site generation without an SSG.
- `scripts/site-build.sh` already wraps `site.in/*.txt` in literal HTML pages.
- A minimal Markdown-to-HTML transformation is tempting.
- It cuts close to replicating an SSG.
- A hand-rolled transformer owns escaping, code fences, and Mermaid blocks.
- Mermaid can render client-side in the browser, keeping the host rsync-only.
- The client-side Mermaid rendering choice is accepted.
- A Markdown-to-HTML transformation still needs a parser, which is a build-time dependency.
- Near-literal output serves developer readers.
- Non-developer readers want styling and navigation, which is SSG territory.
- The decision is open and tracked in `TODO.md`.

## Open Questions

The open questions live in `TODO.md`.

They include Jupyter, static-site generation, the DELTA protocol, and the tool universe.

## Related Documents

- `00-pattern-of-interaction.md` — the interaction pattern.
- `01-async-worktree.md` — the async-worktree worked example.
- `02-tool-universe.md` — the tool survey.
