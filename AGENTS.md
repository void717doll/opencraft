# working in opencraft

- keep changes focused and match nearby code.
- keep new comments, commit messages, and explanations lowercase; preserve identifier and api casing.
- inspect the working tree before editing. preserve unrelated changes.
- stage intended files explicitly, review the staged diff, and commit completed changes after relevant checks.

# versioning

- bump the game version for each completed batch of gameplay, interface, bug-fix, or packaging changes before committing.
- the source of truth is `VERSION_MAJOR`, `VERSION_MINOR`, `VERSION_PATCH`, and `VERSION_SUFFIX` in `source/common/core/EngineConfig.hpp`.
- use a patch bump for ordinary changes unless a different version is requested. bump once per completed batch, including its follow-up fixes.
- read the current version first so concurrent changes are not overwritten. keep package versions and release tags consistent with the game version.
- documentation-only changes do not require a version bump.

# checks

- build `opencraft` and run the relevant regression checks.
- for native interface changes, check the actual screen and interactions when possible. state any untested platforms or runtime paths.
