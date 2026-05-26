# hiredis-happ Agent Skills

This directory contains project skills in the Agent Skills format. Each skill is loaded on demand through its `name` and `description`, so keep descriptions precise and avoid duplicating always-on rules from `AGENTS.md`.

This is the canonical repository skill tree. If a client already supports `.agents/skills/`, do not keep mirrored copies under tool-specific paths unless `doc/ai/source-index.md` records a verified requirement.

## Skills

| Skill | Use when |
| --- | --- |
| `build` | Configuring CMake, building, packaging, CI, install-tree checks, or dependency/runtime-path issues. |
| `testing` | Running tests, adding regression coverage, debugging CTest/test executable failures, or planning Redis integration tests. |
| `code-review` | Reviewing C++ changes, Redis/hiredis async lifecycle, cluster routing, command ownership, or hidden-risk checks. |
| `ai-agent-maintenance` | Updating `AGENTS.md`, `CLAUDE.md`, compatibility shims, skills, source indexes, or cross-tool AI configuration. |

## Maintenance rules

- `SKILL.md` frontmatter must include `name` and `description`.
- `name` must match the parent directory exactly.
- Prefer checklists, gotchas, and repo-specific commands over generic advice.
- Move long reference material to `doc/` and link it instead of bloating skill bodies.
