# C++ Conventions

This flavor applies when the project targets C++.

## Comparison Conventions

- Apply the comparison conventions from `prompts/03-conventions.md` section 1.1.
- Compare constants first: write `if (0 == result)`, never `if (result == 0)`.
- Write relational comparisons lesser-to-greater: write `if (5 < value)`, never `if (value > 5)`.

## Naming

These rules do not apply to external names (API, sysfs, command-line, and CMake-defined names).

- Use structured naming so related items sort together.
- Do not use CamelCase.
- Use long names for names with larger scope.
- Use shorter names only in local scopes.
- Avoid single-word names outside very local scopes.
- Use a simplified version of Hungarian naming.
- Suffix struct and class names with `_o`.
- Prefix index variables with `i_`.
- Prefix string variables with `s_`.
- Prefix pointer variables with `p_`.
- Prefix object variables with `o_`.
- Use `clang-format` with a local `.clang-format`.

## Compilation

- Use the highest level of compiler warnings.
- Treat compiler warnings as errors with `-Werror`.
