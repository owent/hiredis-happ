# hiredis-happ - Code Review Skill

Use this checklist when reviewing or changing `hiredis-happ` code.

## Review inputs

- Read the relevant header in `include/detail/` and matching implementation in `src/detail/` before editing.
- For Redis behavior, verify against official Redis Cluster/RESP semantics instead of guessing.
- For hiredis behavior, verify async callback, reply lifetime, and context ownership semantics.
- Treat `atframework/` and `cmake-toolset/` as dependencies unless the change explicitly targets them.

## C++ correctness checklist

- `cmd_exec` ownership must be single-source: every command wrapper is either queued, callback-called, or destroyed exactly once.
- Any `va_copy()` must have a matching `va_end()`.
- Do not store negative hiredis format lengths in unsigned fields.
- Guard all hiredis pointers from external replies: `redisAsyncContext*`, `redisReply*`, `reply->str`, `reply->element`.
- Async reply objects are valid only during hiredis callbacks; never retain `redisReply*` beyond callback return.
- `redisAsyncConnect()` returning a non-null context with `c->err` must be followed by `redisAsyncFree(c)`.
- `raw`, `cluster`, `connection`, and `redisAsyncContext` are not thread-safe unless a future change documents and tests otherwise.

## Redis Cluster checklist

- Slot calculation must implement Redis hash tags: hash the substring inside the first valid `{...}` pair.
- `MOVED` means update routing and retry; empty endpoint means reuse the current endpoint with the provided port.
- `ASK` means send `ASKING` first and retry only the current command without permanently changing the slot map.
- `TRYAGAIN` should enter bounded retry using TTL/timer flow.
- `CLUSTER SLOTS` can be incomplete or malformed; validate ranges and reply element types.
- Multi-key commands are valid only when all keys map to the same slot.

## Test expectations

- Add unit tests for deterministic parsing/slot logic.
- Add integration tests for lifecycle changes that depend on hiredis event-loop callbacks.
- Prefer sanitizer-backed tests for connection release, reset, disconnect, and retry behavior.

## CI/deploy checklist

- Keep `.github/workflows/main.yml` expressions syntactically valid.
- Verify Linux, macOS, Windows static/shared build paths when changing CMake or dependencies.
- Do not let generated headers or build artifacts dirty a clean checkout unless explicitly documented.
- On Windows, test executables may need the third-party install `bin` directory in `PATH`; missing DLLs can surface as `0xC0000135` before test output appears.
- Run `git diff --check`; this repository declares CRLF-aware whitespace rules in `.gitattributes`.
- For package changes, add an install-tree consumer smoke test before considering the change complete.