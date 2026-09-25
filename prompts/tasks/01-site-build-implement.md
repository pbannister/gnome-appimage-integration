# Task: Implement the Site Build feature

## TASK-DESCRIPTION
- Create: `scripts/site-build.sh`
The script generates `site.out/` from `site.in/`, and implements the requirements in `prompts/features/01-site-build.md` within the files in `TASK-FILES`.
- Create: `site.in/hello.txt`
The example page proves the build converts a page and substitutes its title.

## TASK-OUTPUT
Provide the complete content of each created file, in the order listed in `TASK-FILES`, then the `VERIFICATION:` line.

## TASK-CONTEXT
<note>
The authoritative requirements are `prompts/features/01-site-build.md`; the conventions are `prompts/03-conventions.md`. This task is the worked example for a task file: it is the shape every task in `prompts/tasks/` follows.
</note>

## TASK-FILES

| Operation | Path |
|---|---|
| create | `scripts/site-build.sh` |
| create | `site.in/hello.txt` |

## TASK-VERIFY
- Run: `make test` from the repository root.
- Expected: exit status 0, and `site.out/hello.html` exists.

## TASK-FEATURES
- `prompts/features/01-site-build.md`

## TASK-ACCEPTANCE
- `SITE-BUILD-R001`
- `SITE-BUILD-R002`
- `SITE-BUILD-R003`
- `SITE-BUILD-R004`
- `SITE-BUILD-R006`
- `SITE-BUILD-R009`
- `SITE-BUILD-R010`
- `SITE-BUILD-R011`

OUTPUT: the complete content of each created file in the order listed in `TASK-FILES`, ending with the `VERIFICATION:` line.
