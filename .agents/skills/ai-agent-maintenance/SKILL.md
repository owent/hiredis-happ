---
name: ai-agent-maintenance
description: "Use when: updating AGENTS.md, CLAUDE.md, compatibility shims, Agent Skills, source indexes, or cross-tool AI configuration for hiredis-happ."
---

# AI agent maintenance skill

Use this skill for repository AI instructions, skills, prompt files, compatibility shims, and source-index maintenance.

## Design policy

- `AGENTS.md` is the canonical root instruction file.
- `CLAUDE.md` is a thin compatibility shim importing `AGENTS.md` and, when helpful, `.agents/skills/README.md`.
- `.agents/skills/<name>/SKILL.md` is the standard project skill location.
- Tool-specific files may exist only as thin shims or when an official source shows a concrete benefit.
- When `AGENTS.md` and `.agents/skills/` already cover the same rules, remove duplicate `.github/copilot-instructions.md` and `.github/copilot/skills/` copies instead of maintaining parallel text.
- Do not copy long initialization prompts wholesale. Distill repo-specific rules, commands, gotchas, and source-backed decisions.

## Source-backed workflow

1. Inventory relevant files, if present: `AGENTS.md`, `CLAUDE.md`, `.agents/skills/`, `.agents/skills/README.md`, `.github/copilot-instructions.md`, `.github/copilot/skills/`, `.github/instructions/`, `.github/prompts/`, `.github/agents/`.
2. Check `doc/ai/source-index.md` before making tool-compatibility claims.
3. Fetch official docs for any new URL or tool behavior before relying on it.
4. Mark failed or ambiguous sources as unverified. Do not encode unsupported claims into always-on instructions.
5. Update `last_checked`, `next_review`, and update triggers in the source index when sources are revisited.

## Skill frontmatter rules

- Frontmatter is YAML between `---` markers.
- Required fields: `name`, `description`.
- `name` must match the parent directory exactly.
- Use lowercase letters, digits, and hyphens; no spaces, underscores, leading/trailing hyphens, or consecutive hyphens.
- Quote descriptions containing colons.
- Descriptions should use trigger language such as `Use when:` and stay under 1024 characters.

## Progressive disclosure

- Keep `AGENTS.md` short enough to be always loaded.
- Put multi-step workflows in skills.
- Put long references in `doc/` and link them.
- Avoid duplicating identical instructions across `AGENTS.md`, `CLAUDE.md`, compatibility shims, and skills.

## Validation

- Run `git diff --check`.
- Manually verify every `SKILL.md` frontmatter block.
- For docs-only changes, do not run a full CMake build unless build/test commands or generated files changed.
- Summarize any tool-specific support as verified, compatibility shim, or unverified.
