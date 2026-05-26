---
name: build
description: "Use when: configuring, building, packaging, installing, or debugging CMake/CI/dependency/runtime-path issues in hiredis-happ."
---

# Build skill

Use this skill for CMake configuration, local builds, packaging changes, CI workflow edits, and dependency/runtime lookup problems.

## Start with context

1. Check `AGENTS.md` for global guardrails.
2. Inspect `CMakeLists.txt`, `project/cmake/`, and relevant CI files before changing build logic.
3. Treat `atframework/` and `cmake-toolset/` as dependency submodules unless the task explicitly targets them or they block this repository's build.

## Local configure/build

Preferred validation build:

```powershell
cmake -S . -B build_jobs_review -DPROJECT_HIREDIS_HAPP_ENABLE_UNITTEST=ON -DPROJECT_HIREDIS_HAPP_ENABLE_SAMPLE=ON -DATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LOW_MEMORY_MODE=ON
cmake --build build_jobs_review --config RelWithDebInfo
```

For Linux/macOS single-config generators, use `-DCMAKE_BUILD_TYPE=RelWithDebInfo` and omit `--config` if appropriate.

## Windows runtime path

Before running tests or samples on Windows, prepend the third-party install `bin` directory. Prefer resolving it from `build_jobs_review/CMakeCache.txt` instead of hardcoding an MSVC version suffix:

```powershell
$thirdPartyInstallDir = Select-String -Path "$PWD\build_jobs_review\CMakeCache.txt" -Pattern '^PROJECT_THIRD_PARTY_INSTALL_DIR:PATH=(.+)$' | ForEach-Object { $_.Matches[0].Groups[1].Value } | Select-Object -First 1
if (-not $thirdPartyInstallDir) { $thirdPartyInstallDir = Get-ChildItem "$PWD\third_party\install" -Directory | Where-Object { $_.Name -like 'windows-*-msvc-*' } | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName }
if (-not $thirdPartyInstallDir) { throw "Could not locate the third-party install directory. Configure the build first." }
$env:PATH = "$thirdPartyInstallDir\bin;$env:PATH"
```

If this is missing, `hiredis-happ-test.exe` can fail before `main()` with `0xC0000135` because `hiredis.dll` is not discoverable.

## CI and workflow edits

- Keep GitHub Actions expressions syntactically valid: use `${{ ... }}` exactly.
- Validate cache keys, matrix variables, Windows `PATH`, and static/shared build differences.
- If CI behavior changes, update `doc/ROADMAP.md` or the relevant doc/playbook.

## Packaging checklist

- Public includes must continue to work through `<hiredis_happ.h>`.
- Avoid generating committed headers or build artifacts into the source tree unless intentionally documented.
- Preserve the public CMake target namespace unless the task is an API-breaking packaging change.
- Future package work should include a config/version file and an install-tree consumer smoke test.

## Validation

- Run `git diff --check` for docs/build-only changes.
- Run configure/build for CMake, dependency, or workflow command changes.
- If generated files or submodules change, explain why in the final summary.
