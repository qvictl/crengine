# Review policy

This fork carries the crengine changes used by qvictl/koreader-base. Changes are
submitted to this fork, not to the upstream project.

Before merging a pull request:

- Both Debug and Release build, lint and native font-lifecycle tests must pass.
- CodeRabbit must finish reviewing the latest commit; blocking findings must be
  fixed or answered, and review threads resolved.
- The candidate commit must pass the required qvictl/koreader-base checks and the
  matching KOReader emulator integration tests before the engine PR is merged.
- Font-memory changes require paired measurements with the same book, fonts,
  device settings and native dependencies, plus rendering and reopen checks.

No human review is required. The agent verifies these conditions and merges the
reviewed commit. This new fork has no automatic review-gate workflow; companion
base and KOReader PRs retain their existing required checks and review gates.
