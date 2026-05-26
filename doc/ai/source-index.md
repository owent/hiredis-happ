# AI configuration source index

This index records the official or attempted sources used to design this repository's AI-agent instructions and skills.

- `last_checked`: 2026-05-26
- `next_review`: Re-check on major VS Code/Copilot, Claude Code, Agent Skills, or MCP changes; otherwise review at least quarterly.
- `update_trigger`: Update this file whenever a new tool-specific instruction path, skill format, MCP policy, or compatibility claim is added to the repository.

## Repository decisions

| Decision | Status | Rationale |
| --- | --- | --- |
| Root `AGENTS.md` is canonical. | Verified | Cross-tool plain Markdown entry point; avoids duplicating always-on rules. |
| Root `CLAUDE.md` stays an import-only shim. | Verified for Claude Code | Claude Code reads `CLAUDE.md`; importing `AGENTS.md` and a small skills index keeps the source of truth in one place without duplicating rules. |
| Project skills live under `.agents/skills/<name>/SKILL.md`. | Verified | Supported by Agent Skills spec and multiple clients; enables progressive disclosure. |
| Prefer `AGENTS.md` directly over a parallel `.github/copilot-instructions.md` copy. | Verified for VS Code/Copilot | VS Code supports `AGENTS.md` directly, so removing a duplicate Copilot-only instruction file reduces context bloat without losing compatibility. |
| Repository skills stay consolidated under `.agents/skills/`. | Verified for VS Code/Copilot | VS Code Agent Skills support `.agents/skills/`, so mirroring the same skills under `.github/copilot/skills/` adds maintenance cost without adding capability. |
| No project MCP server config is added by default. | Security posture | MCP/local tools require explicit trust, least privilege, and visible commands. |

## Verified sources

| Area | URLs checked | Key facts used |
| --- | --- | --- |
| AGENTS.md | <https://agents.md/> | `AGENTS.md` is a plain Markdown README for agents; root and nested files are supported by many tools; explicit user prompts take precedence. |
| Agent Skills core | <https://agentskills.io/>, <https://agentskills.io/specification>, <https://agentskills.io/skill-creation/quickstart>, <https://agentskills.io/skill-creation/best-practices>, <https://agentskills.io/skill-creation/optimizing-descriptions>, <https://agentskills.io/skill-creation/evaluating-skills>, <https://agentskills.io/skill-creation/using-scripts> | Skills are directories containing `SKILL.md`; required frontmatter is `name` and `description`; `name` must match parent directory; descriptions are discovery surfaces; progressive disclosure and concise skill bodies are recommended. |
| VS Code/Copilot customizations | <https://code.visualstudio.com/docs/copilot/customization/custom-instructions>, <https://code.visualstudio.com/docs/copilot/customization/agent-skills>, <https://code.visualstudio.com/docs/copilot/customization/custom-agents>, <https://code.visualstudio.com/docs/copilot/customization/prompt-files> | VS Code supports repository instructions such as `AGENTS.md`, `CLAUDE.md`, and `.github/copilot-instructions.md`; project skill locations include `.github/skills/`, `.claude/skills/`, and `.agents/skills/`; prompt files and custom agents are separate primitives. |
| Claude Code | <https://code.claude.com/docs/en/memory>, <https://code.claude.com/docs/en/skills>, <https://www.anthropic.com/engineering/equipping-agents-for-the-real-world-with-agent-skills> | Claude Code reads `CLAUDE.md`; a thin import-only bridge to `AGENTS.md` plus a small skills index is appropriate; project skills follow the Agent Skills model with Claude-specific extensions; keep memory files concise and move procedures to skills. |
| MCP | <https://modelcontextprotocol.io/docs/getting-started/intro>, <https://modelcontextprotocol.io/specification/2025-11-25/architecture>, <https://modelcontextprotocol.io/specification/2025-11-25/server/tools>, <https://modelcontextprotocol.io/specification/2025-11-25/server/utilities/completion>, <https://modelcontextprotocol.io/specification/2025-11-25/basic/security_best_practices> | MCP uses host/client/server boundaries; tools are model-controlled and should include human-in-loop controls; validate inputs, sanitize outputs, avoid token passthrough, mitigate confused deputy/SSRF/session/local-server risks, and use least privilege. |
| Kilo Code | <https://kilo.ai/docs/>, <https://kilo.ai/docs/code-with-ai>, <https://kilo.ai/docs/customize>, <https://kilo.ai/docs/automate>, <https://kilo.ai/docs/customize/agents-md>, <https://kilo.ai/docs/customize/skills>, <https://kilo.ai/docs/customize/custom-rules>, <https://kilo.ai/docs/customize/custom-instructions>, <https://kilo.ai/docs/customize/custom-modes>, <https://kilo.ai/docs/customize/custom-subagents>, <https://kilo.ai/docs/customize/context/kilocodeignore>, <https://kilo.ai/docs/automate/mcp/using-in-kilo-code> | Kilo supports root and nested `AGENTS.md`, Agent Skills, `.agents/skills`, custom rules/instructions, custom modes/subagents, and MCP configuration; permission settings must be explicit. |
| Roo Code | <https://roocodeinc.github.io/Roo-Code/features/custom-instructions>, <https://roocodeinc.github.io/Roo-Code/features/custom-modes>, <https://roocodeinc.github.io/Roo-Code/features/mcp/overview> | Roo supports `AGENTS.md`/`AGENT.md` by setting, `.roo/rules/`, mode-specific `.roo/rules-{modeSlug}/`, and `.roomodes` with tool groups and file restrictions. |
| OpenCode | <https://opencode.ai/docs/rules/>, <https://opencode.ai/docs/agents/> | OpenCode uses project `AGENTS.md`; `CLAUDE.md` is a fallback when no `AGENTS.md` exists; agents can live in `opencode.json` or `.opencode/agents/*.md`; permissions include skills and tools. |
| Windsurf | <https://docs.windsurf.com/windsurf/cascade/skills> | Windsurf Cascade Skills support Agent Skills and scan `.agents/skills/` for cross-agent compatibility; skills are distinct from rules and workflows. |

## Failed or limited sources

Do not encode strong compatibility claims from these sources until re-verified.

| Source | URLs attempted | Result |
| --- | --- | --- |
| Old Kilo URLs | `https://kilocode.ai/docs/basic-usage/custom-rules`, `https://kilocode.ai/docs/features/skills/overview`, `https://kilocode.ai/docs/advanced-usage/custom-instructions`, redirected `https://kilo.ai/docs/basic-usage/custom-rules`, `https://kilo.ai/docs/features/skills/overview`, `https://kilo.ai/docs/advanced-usage/custom-instructions` | Old paths redirected or returned 404; current `kilo.ai/docs/customize/...` paths were used instead. |
| Zoo/OpenClaw-like docs | `https://zoo.dev/docs/agents/codex/`, `https://docs.zoo.dev/agent/custom-instructions`, `https://docs.zoo.dev/agent/modes`, `https://docs.zoo.dev/agent/skills`, `https://docs.zoo.dev/agent/mcp`, `https://docs.zoo.dev/agent/rooignore`, `https://github.com/zoo-code/zoo-code` | 404 or failed meaningful extraction in this session. |
| Roo skills/auto-approve pages | `https://roocodeinc.github.io/Roo-Code/features/skills/overview`, `https://roocodeinc.github.io/Roo-Code/features/auto-approve` | 404 during this session. |
| Windsurf memories/agents-md pages | `https://docs.windsurf.com/windsurf/memories`, `https://docs.windsurf.com/windsurf/agents/agents-md` | Failed meaningful extraction; only Cascade Skills page was used for Windsurf claims. |
| OpenHands raw docs | `https://raw.githubusercontent.com/All-Hands-AI/OpenHands/main/docs/usage/how-to/custom-agents.md`, `https://raw.githubusercontent.com/All-Hands-AI/OpenHands/main/docs/usage/how-to/customize-agent-skills.md` | 404 during this session. |

## Maintenance checklist

1. Re-fetch official docs before changing compatibility claims.
2. Prefer root `AGENTS.md` and standard `.agents/skills` over tool-specific duplication.
3. Keep tool-specific files as shims unless a verified tool requires richer content.
4. Validate skill frontmatter after any rename or description change.
5. Record failures and uncertainty explicitly instead of guessing.
