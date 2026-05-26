---
name: testing
description: "Use when: running hiredis-happ tests, adding regression coverage, debugging CTest failures, or planning Redis/raw/cluster integration tests."
---

# Testing skill

Use this skill for unit tests, CTest execution, deterministic regressions, Redis integration tests, and sanitizer-oriented lifecycle validation.

## Test framework

Tests live under `test/case/` and use the atframe_utils private test framework, not Google Test.

Common macros:

- `CASE_TEST(group, name)`
- `CASE_EXPECT_TRUE`, `CASE_EXPECT_FALSE`
- `CASE_EXPECT_EQ`, `CASE_EXPECT_NE`, `CASE_EXPECT_LT`, `CASE_EXPECT_LE`, `CASE_EXPECT_GT`, `CASE_EXPECT_GE`
- `CASE_MSG_INFO()`, `CASE_MSG_ERROR()`
- `CASE_THREAD_SLEEP_MS`, `CASE_THREAD_YIELD`

## Configure/build before tests

```powershell
cmake -S . -B build_jobs_review -DPROJECT_HIREDIS_HAPP_ENABLE_UNITTEST=ON -DPROJECT_HIREDIS_HAPP_ENABLE_SAMPLE=ON -DATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LOW_MEMORY_MODE=ON
cmake --build build_jobs_review --config RelWithDebInfo
```

## Run tests

Windows requires the third-party `bin` directory in `PATH`. Prefer resolving it from `build_jobs_review/CMakeCache.txt` instead of hardcoding an MSVC version suffix:

```powershell
$thirdPartyInstallDir = Select-String -Path "$PWD\build_jobs_review\CMakeCache.txt" -Pattern '^PROJECT_THIRD_PARTY_INSTALL_DIR:PATH=(.+)$' | ForEach-Object { $_.Matches[0].Groups[1].Value } | Select-Object -First 1
if (-not $thirdPartyInstallDir) { $thirdPartyInstallDir = Get-ChildItem "$PWD\third_party\install" -Directory | Where-Object { $_.Name -like 'windows-*-msvc-*' } | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName }
if (-not $thirdPartyInstallDir) { throw "Could not locate the third-party install directory. Configure the build first." }
$env:PATH = "$thirdPartyInstallDir\bin;$env:PATH"
ctest --test-dir build_jobs_review -V -R hiredis-happ-run-test -C RelWithDebInfo --timeout 120
```

Inspect discovered test names with:

```powershell
ctest --test-dir build_jobs_review -N -C RelWithDebInfo
```

Prefer `ctest` over invoking `hiredis-happ-test.exe` directly so generator-specific paths, environment handling, and future test additions stay consistent.

If `ctest` reports `0xC0000135` or produces no useful test output, first check missing DLLs and `PATH` before debugging test logic.

## Regression focus

- Redis Cluster hash tags and slot routing.
- `MOVED`, `ASK + ASKING`, `TRYAGAIN`, `CLUSTERDOWN`, empty endpoint, and malformed `CLUSTER SLOTS` replies.
- hiredis async lifecycle: connect failure, disconnect, reset while commands are pending, and callbacks with `reply == nullptr`.
- Non request-response commands: `subscribe`, `unsubscribe`, and `monitor` must use raw command handling rather than normal `exec()`.
- Logging with small buffers to catch truncation and null-termination issues.

## Integration test guidance

Use real Redis/Redis Cluster and an event-loop adapter for lifecycle behavior. Prefer sanitizer-backed tests for `connection::release()`, reset, disconnect, retry, pending callbacks, and command destruction. Do not claim lifecycle safety from object-state unit tests alone.

## Validation

After adding tests, run the narrow test first, then full CTest when practical. Document skipped integration tests and environment requirements in `doc/ROADMAP.md` if they remain pending.
