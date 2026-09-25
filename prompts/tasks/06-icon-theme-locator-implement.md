# Task: Implement the Icon Theme Locator feature

## TASK-DESCRIPTION
- Create: `sources/desktop/icon_theme_locator.h`
- Create: `sources/desktop/icon_theme_locator.cpp`
- Create: `tests/icon_theme_locator_test.cpp`
- Create: `tests/12-icon-theme-locator.sh`
The locator resolves an icon name through the icon theme search path, and implements the requirements in `prompts/features/07-icon-theme-locator.md`.
The test builds its own XDG tree under a temporary directory, so it reads no real theme.

## TASK-OUTPUT
Produce these complete files in this order, then the `VERIFICATION:` line:
1. `sources/desktop/icon_theme_locator.h`
2. `sources/desktop/icon_theme_locator.cpp`
3. `tests/icon_theme_locator_test.cpp`
4. `tests/12-icon-theme-locator.sh`

## TASK-CONTEXT
<note>
The icon search rules are described in `documents/12-desktop-loading-and-provenance.md` section 3. A lookup returns every candidate with its theme, size, and context rather than a single winner.
</note>

## TASK-FILES

| Operation | Path |
|---|---|
| create | `sources/desktop/icon_theme_locator.h` |
| create | `sources/desktop/icon_theme_locator.cpp` |
| create | `tests/icon_theme_locator_test.cpp` |
| create | `tests/12-icon-theme-locator.sh` |

## TASK-VERIFY
- Run: `make test` from the repository root.
- Expected: exit status 0, and `tests/12-icon-theme-locator.sh` reports ok.

## TASK-FEATURES
- `prompts/features/07-icon-theme-locator.md`

## TASK-ACCEPTANCE
- `ICON-THEME-LOCATOR-R001`
- `ICON-THEME-LOCATOR-R004`
- `ICON-THEME-LOCATOR-R005`
- `ICON-THEME-LOCATOR-R006`
- `ICON-THEME-LOCATOR-R007`
- `ICON-THEME-LOCATOR-R009`
- `ICON-THEME-LOCATOR-R012`

OUTPUT: the four complete files in the order listed, ending with the `VERIFICATION:` line.
