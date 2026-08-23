# Agent Instructions

Before making any change, read the relevant documentation under `docs/`.

At minimum, always read:

- `docs/REPOSITORY_POLICY.adoc`
- `docs/ARCHITECTURE.adoc`
- `docs/DEVELOPMENT.adoc`

Also read any topic-specific documentation relevant to the task, including:

- `docs/NETWORKING.adoc`
- files under `docs/design/`
- Architecture Decision Records under `docs/adr/`

The files under `docs/` are the canonical source of project rules and design intent.
Do not duplicate or reinterpret those rules here.

If implementation and documentation disagree, stop and resolve the discrepancy rather than silently choosing one.

For every task:

1. Read the relevant docs first.
2. Keep the change within the issue scope.
3. Update documentation when behavior, architecture, interfaces, or design changes.
4. Run the complete local validation command before pushing.
5. Do not weaken, remove, or bypass tests, linters, warnings, validation, or CI checks merely to make a change pass.
6. Do not add a license or grant reuse rights unless explicitly instructed by the owner.
