# AGENTS.md

This file is the canonical, cross-tool guide for AI agents working in `hiredis-happ`. Keep it concise and source-backed. If a tool-specific file conflicts with this file, follow the user's explicit request first, then this file, then the tool-specific shim.

## Project snapshot

- `hiredis-happ` is a C++ library that wraps hiredis async APIs for Redis raw and cluster high-availability connectors.
- Minimum language level: C++17.
- Build system: CMake, with third-party dependencies prepared under `third_party/install/*`.
- Main source layout:
  - `include/` public headers.
  - `src/` implementations.
  - `test/case/` unit tests using the atframe_utils private test framework.
  - `sample/` raw and cluster CLI examples.
  - `doc/` design notes, review reports, roadmap, and AI source index.
- `atframework/atframe_utils` and `atframework/cmake-toolset` are dependency submodules. Treat them as external unless a task explicitly targets them or they block this repository's build. Nested `AGENTS.md` files inside submodules override this root guide for those paths.

## Working principles

1. Research before changing behavior. Verify Redis Cluster, RESP, hiredis async, CMake packaging, and AI-tool compatibility against official or source-indexed documentation.
2. Prefer small, reviewable patches. Do not reformat unrelated files or rewrite public APIs unless the task requires it.
3. Read the relevant header and implementation before editing C++ code. For example, read `include/detail/happ_cluster.h` and `src/detail/happ_cluster.cpp` together.
4. Maintain docs and playbooks when a behavior, workflow, build command, or guardrail changes.
5. Do not print secrets or whole secret files. If credentials are required, ask the user to enter them directly into the terminal.
6. Do not auto-approve unknown MCP/local-server commands. Local tools run with user privileges; require visible commands, least privilege, and explicit consent for sensitive operations.

## C++ and Redis guardrails

- `cmd_exec` ownership is critical: each command must be callback-called and destroyed exactly once.
- `redisReply*` is callback-scoped; never retain it after a hiredis callback returns.
- If `redisAsyncConnect()` returns a non-null context with `c->err`, call `redisAsyncFree(c)`.
- Treat `raw`, `cluster`, `connection`, and `redisAsyncContext` as not thread-safe unless future tests and docs explicitly say otherwise.
- Guard nullable hiredis fields: `reply`, `reply->str`, `reply->element`, `redisAsyncContext*`.
- Redis Cluster slot calculation must implement hash tags. Handle `MOVED`, `ASK` + `ASKING`, `TRYAGAIN`, empty redirection endpoints, and incomplete/malformed `CLUSTER SLOTS` defensively.
- This library supports request-response commands. `subscribe`, `unsubscribe`, and `monitor` require raw command handling and must not be routed through the normal `exec()` lifecycle.

## Build and test commands

Use out-of-source builds. A typical local validation build is:

```powershell
cmake -S . -B build_jobs_review -DPROJECT_HIREDIS_HAPP_ENABLE_UNITTEST=ON -DPROJECT_HIREDIS_HAPP_ENABLE_SAMPLE=ON -DATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LOW_MEMORY_MODE=ON
cmake --build build_jobs_review --config RelWithDebInfo
```

On Windows, prepend the third-party `bin` directory before running tests or samples so `hiredis.dll` is discoverable. Prefer resolving `PROJECT_THIRD_PARTY_INSTALL_DIR` from the build cache instead of hardcoding a specific MSVC triplet suffix:

```powershell
$thirdPartyInstallDir = Select-String -Path "$PWD\build_jobs_review\CMakeCache.txt" -Pattern '^PROJECT_THIRD_PARTY_INSTALL_DIR:PATH=(.+)$' | ForEach-Object { $_.Matches[0].Groups[1].Value } | Select-Object -First 1
if (-not $thirdPartyInstallDir) { $thirdPartyInstallDir = Get-ChildItem "$PWD\third_party\install" -Directory | Where-Object { $_.Name -like 'windows-*-msvc-*' } | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName }
if (-not $thirdPartyInstallDir) { throw "Could not locate the third-party install directory. Configure the build first." }
$env:PATH = "$thirdPartyInstallDir\bin;$env:PATH"
ctest --test-dir build_jobs_review -V -R hiredis-happ-run-test -C RelWithDebInfo --timeout 120
```

For docs-only or AI-config-only changes, at minimum run `git diff --check` and validate skill frontmatter. Full CMake builds are optional unless instructions, commands, or generated files changed.

## Standard Agent Skills

Use `.agents/skills/<name>/SKILL.md` as the project-standard skill location. Current skills:

- `build`: CMake, dependency, install, CI, and packaging workflow.
- `testing`: unit/integration tests, Windows DLL lookup, and sanitizer guidance.
- `code-review`: Redis/hiredis/C++ review checklist and risk triage.
- `ai-agent-maintenance`: updates to `AGENTS.md`, `CLAUDE.md`, tool shims, skills, and source indexes.

Keep skill frontmatter valid: `name` must match the parent directory, use lowercase letters/digits/hyphens, and include a concise `description` explaining when to use the skill.

## Documentation map

- `doc/ROADMAP.md`: active execution plan and open risks.
- `doc/code-review-2026-05-26.md`: latest comprehensive review report.
- `doc/ai/source-index.md`: verified AI-tool documentation sources, review triggers, and unverified sources.

When adding a new official source or relying on a tool behavior, update `doc/ai/source-index.md` with the URL, date checked, and confidence level.
