# How to Write Features

This document defines how a human must write feature requirements for the LLM.

Apply naming rules from `prompts/flavors/01-semantic-sort-naming.md`.

## 1. Feature Purpose

* A feature defines a project capability, its behavior, and its requirements.
* A feature is stable project context that may be implemented by one or more tasks.
* A feature must not describe a single implementation step or TODO status update.

## 2. Feature File Location and Naming

* Feature requirements belong in `prompts/features/`.
* Feature numbers must be unique.
* Use the established feature name.
* Do not invent a feature name when an existing project name applies.

## 3. Feature Structure

Every feature must contain these sections in order:
```markdown
# Feature: <feature name>

## Purpose

Describe the capability and the problem it solves.

## Requirements

List the mandatory behavior and constraints.

## Behavior

* Describe externally observable behavior.

## Dependencies

* List directly required features, or state `None`.
```

## 4. Requirement Rules

* Requirements must be specific, testable, and implementation-independent.
* Requirements must describe what the feature does, not how a task must implement it.
* Each requirement must be stated as a separate bullet.
* Do not include TODO status, task sequencing, or response-format instructions in a feature file.

## 5. Feature and Task Relationship

* A feature file defines capability requirements.
* A task requests a bounded change that implements, tests, documents, or modifies a feature.
* A task must explicitly reference every feature file whose requirements apply.
* A feature may be implemented by multiple tasks.
* A task may address multiple features only when it explicitly references each feature.

## 6. Feature Changes

* Modify an existing feature file when the capability requirements change.
* Create a new feature file when the project gains a distinct capability.
* Do not duplicate requirements across feature files.
* Update dependent tasks when a feature change makes their requirements or assumptions invalid.
