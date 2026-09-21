# Semantic-Sort Naming

A semantic-sort name uses stable components in this order:

`<domain>_<role>_<purpose>_<variant>`

* Components are ordered from broad meaning to narrow meaning.
* Components are separated by underscores for identifiers.
* Missing components are omitted.
* Names must use the established vocabulary for the project.
* Acronyms use the established project spelling.
* If no project spelling exists, use uppercase only when the target language or external API convention requires it.
* Plural names describe collections or multiple values.
* Singular names describe one entity, value, or operation target.
* Boolean names use an established predicate prefix such as `is`, `has`, `can`, or `should` when the target language permits it.
* Constants use the target language's conventional constant style.
* Public API names follow the naming required by the target language, framework, or external API.
* External names that cannot be changed are preserved.
* Names required by a language or framework are preserved when changing them would harm correctness or interoperability.

## Directory Naming

* Use stable, semantic-sort directory names.

## Filename Structure

* Use semantic-sort filenames.
* Related files must share a structured prefix.
* Feature files use the exact pattern `<feature-number>-<feature-name>.md`.
* Avoid CamelCase in filenames unless required by an external convention.
* Use hyphens or underscores for readability.
* Do not invent naming schemes.

## Identifier Naming

* Use semantic-sort identifiers composed of ordered semantic components.
* Use the order:

`<domain>_<role>_<purpose>_<variant>`

* Order components from broad meaning to narrow meaning.
* Use the established project vocabulary.
* Use descriptive names for identifiers with broad scope.
* Short identifiers such as `i`, `s`, `n`, `p`, and `o` are permitted only in very small scopes.
* Use `o1`, `o2`, and `o3` for ordered transformations only when the target language and scope make the sequence clear.
* Preserve external names that cannot be changed.
* Preserve names required by a language, framework, or public API.
* Use the target language's conventional styles for constants, booleans, classes, and public APIs when those styles are required for correctness or interoperability.

## Type Alias Naming

* When a type is complex, nested, or structural (a template, a collection of collections, or a multi-layered container), do not define it inline.
* Use a type alias (`using` in C++, `typedef` in C) to give the type a semantic, nominal name.
* The alias must follow the `<domain>_<role>_<purpose>_<variant>` pattern.
* A named identity improves code scannability, reduces cognitive load, and communicates intent.

## Plan Block Naming

* A plan block uses semantic-sort naming where applicable.

## Log Filename Rules

* Log filenames must begin with the sortable prefix:
    * `YYYY-MM-DD-HH-MM-SS-<description>.log`.

## Script Filename Rules

* When shell scripts have a likely order in which they might be invoked, use filenames of the form:
    * `<number>-<name>.sh`
* Script names use semantic-sort component order, from broad meaning to narrow meaning:
    * `<domain>-<role>.sh`
* Ordered script names combine both patterns:
    * `<number>-<domain>-<role>.sh`
* For example, the script that builds the site is `site-build.sh`, not `build-site.sh`.
* For example, the script that syncs the site is `site-sync.sh`, not `sync-site.sh`.

## Ordered Prompt Filename rules

* When prompt files have an order in which applied, use filenames of the form:
    * `<number>-<name>.md`


