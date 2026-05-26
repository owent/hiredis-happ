---
name: code-review
description: "Use when: reviewing or modifying hiredis-happ C++ code, Redis/hiredis async lifecycle, cluster routing, command ownership, tests, or CI risk."
---

# Code review skill

Use this skill for code review, risk triage, and correctness checks in `hiredis-happ`.

## Review process

1. Read `AGENTS.md`, the relevant header in `include/detail/`, and the matching implementation in `src/detail/`.
2. Verify Redis/hiredis claims against official docs or `doc/ai/source-index.md` before making semantic changes.
3. Classify findings by impact: crash/use-after-free/leak/data-loss first, then protocol correctness, then maintainability.
4. Prefer minimal fixes with regression tests. Record unresolved risks in `doc/ROADMAP.md`.

## Async ownership checklist

- Every `cmd_exec` must be callback-called and destroyed exactly once.
- Do not retain `redisReply*` beyond the hiredis callback.
- Guard `reply`, `reply->str`, `reply->element`, and `redisAsyncContext*` before dereferencing.
- If `redisAsyncConnect()` returns a non-null context with `c->err`, free it with `redisAsyncFree(c)`.
- Treat `redisAsyncDisconnect()` and pending callbacks as asynchronous lifecycle boundaries; verify with integration tests before refactoring release paths.
- Any `va_copy()` must have a matching `va_end()`.
- Do not store negative hiredis format lengths in unsigned fields.

## Redis Cluster checklist

- Slot calculation must use Redis hash tags: first valid non-empty `{...}` pair only.
- `MOVED`: update routing and retry; empty endpoint reuses current IP with the redirected port.
- `ASK`: send `ASKING` first and retry only the current command without permanently updating the slot map.
- `TRYAGAIN`: use bounded retry through the existing TTL/timer path.
- `CLUSTER SLOTS`: validate reply type, element pointers, slot range, host strings, and incomplete coverage.
- Multi-key behavior must respect same-slot constraints.

## Tests and CI

- Add unit tests for deterministic parsing and slot logic.
- Add integration tests for hiredis/event-loop lifecycle changes.
- On Windows, include third-party `bin` in `PATH` before tests.
- Run `git diff --check`; the repository has CRLF-aware whitespace rules.

## Output expectations

Report what was verified, what changed, what tests ran, and what remains risky. Avoid vague claims such as "should be safe" without test or source backing.
