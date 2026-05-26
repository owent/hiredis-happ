# hiredis-happ

Asynchronous hiredis-based Redis high-availability connector for raw single-node connections and Redis Cluster routing.

[![GitHub Actions status][ci-badge]][ci-link]

[ci-badge]: https://github.com/owent/hiredis-happ/actions/workflows/main.yml/badge.svg "GitHub Actions build status"
[ci-link]: https://github.com/owent/hiredis-happ/actions/workflows/main.yml "GitHub Actions build status"

## Highlights

- C++17 library wrapping hiredis async APIs.
- Supports raw single-node Redis and Redis Cluster connectors.
- Handles reconnect, retry, and Cluster hash-tag-aware slot routing.
- Provides sample CLIs for raw and cluster workflows.
- Uses a request-response `exec()` lifecycle for normal commands.
- Sentinel remains design-only for now; it is not implemented in this repository.

## CI job matrix

|Target system|Toolchain|Note|
|---|---|---|
|Linux|GCC||
|Linux|Clang|With libc++|
|Windows|Visual Studio 2022|Static linking|
|Windows|Visual Studio 2022|Dynamic linking|
|macOS|AppleClang|With libc++|

## Build

### Configure and build

```powershell
cmake -S . -B build_jobs_review -DPROJECT_HIREDIS_HAPP_ENABLE_UNITTEST=ON -DPROJECT_HIREDIS_HAPP_ENABLE_SAMPLE=ON -DATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LOW_MEMORY_MODE=ON
cmake --build build_jobs_review --config RelWithDebInfo
```

For single-config generators on Linux/macOS, add `-DCMAKE_BUILD_TYPE=RelWithDebInfo` during configure.

Install into the default prefix or your custom `CMAKE_INSTALL_PREFIX` with:

```powershell
cmake --install build_jobs_review --config RelWithDebInfo
```

### Common CMake options

- `BUILD_SHARED_LIBS`: Build `hiredis-happ` as a shared library instead of a static library.
- `PROJECT_HIREDIS_HAPP_ENABLE_SAMPLE`: Build the sample CLIs under `sample/`.
- `PROJECT_HIREDIS_HAPP_ENABLE_UNITTEST`: Build the unit tests under `test/`.
- `ATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LOW_MEMORY_MODE`: Reduce third-party build concurrency for lower-memory environments.

### Windows runtime note

Before running tests or sample executables on Windows, prepend the third-party `bin` directory so `hiredis.dll` is discoverable. The snippet below first reads `PROJECT_THIRD_PARTY_INSTALL_DIR` from `build_jobs_review/CMakeCache.txt`; if that cache is unavailable, it falls back to scanning `third_party/install/` for the current Windows MSVC triplet directory.

```powershell
$thirdPartyInstallDir = Select-String -Path "$PWD\build_jobs_review\CMakeCache.txt" -Pattern '^PROJECT_THIRD_PARTY_INSTALL_DIR:PATH=(.+)$' |
  ForEach-Object { $_.Matches[0].Groups[1].Value } |
  Select-Object -First 1

if (-not $thirdPartyInstallDir) {
  $thirdPartyInstallDir = Get-ChildItem "$PWD\third_party\install" -Directory |
    Where-Object { $_.Name -like 'windows-*-msvc-*' } |
    Sort-Object Name -Descending |
    Select-Object -First 1 -ExpandProperty FullName
}

if (-not $thirdPartyInstallDir) {
  throw "Could not locate the third-party install directory. Configure the build first."
}

$env:PATH = "$thirdPartyInstallDir\bin;$env:PATH"
```

## Run the sample CLIs

Sample executables are built only when libuv or libevent is available. They use the hiredis async adapter from libuv when available; otherwise they fall back to libevent.

```powershell
.\build_jobs_review\sample\RelWithDebInfo\hiredis-happ-sample_raw_cli.exe 127.0.0.1 6379
.\build_jobs_review\sample\RelWithDebInfo\hiredis-happ-sample_cluster_cli.exe 127.0.0.1 7000
```

Both CLIs accept an optional password as the third argument. For single-config generators, drop the `RelWithDebInfo` directory component from the executable path.

Runnable reference implementations:

- [`sample/sample_raw_cli/main.cpp`](sample/sample_raw_cli/main.cpp)
- [`sample/sample_cluster_cli/main.cpp`](sample/sample_cluster_cli/main.cpp)

## Run tests

On Windows, run the auto-detection snippet from the Windows runtime note first so `hiredis.dll` is discoverable.

```powershell
ctest --test-dir build_jobs_review -V -R hiredis-happ-run-test -C RelWithDebInfo --timeout 120
```

For single-config generators, omit `-C RelWithDebInfo`.

To inspect the discovered test names before running them:

```powershell
ctest --test-dir build_jobs_review -N -C RelWithDebInfo
```

Prefer running tests through `ctest` instead of invoking `hiredis-happ-test.exe` directly, so generator-specific paths and future test additions stay consistent.

If `ctest` reports `0xC0000135` or shows no useful test output, re-check `PATH` first before chasing test logic.

## Troubleshooting

### The sample executable is missing

- Make sure you configured with `-DPROJECT_HIREDIS_HAPP_ENABLE_SAMPLE=ON`.
- Sample targets are added only when libuv or libevent is available. If configure succeeds but no sample target appears, check whether one of those event-loop dependencies was found.
- On multi-config generators such as Visual Studio, sample binaries are typically under `build_jobs_review/sample/RelWithDebInfo/`. On single-config generators, they are usually under `build_jobs_review/sample/`.

### Windows exits before `main()` or returns `0xC0000135`

That usually means `hiredis.dll` is not on `PATH` yet.

Reuse the auto-detection snippet from the Windows runtime note above, then rerun the sample or test executable in the same shell.

### The sample prints `connected failed` or `connect to redis failed`

- Verify the IP and port first.
- If the server requires authentication, pass the password as the third CLI argument.
- For Redis Cluster, the seed endpoint must be a reachable cluster node; the connector will then load slot information from that node.
- Keep the sample's console logging enabled. The sample CLIs already route `set_log_writer()` output to stdout, which is the fastest way to see hiredis and connector-side errors.

### My own application never receives replies

If you are integrating the library into your own event loop instead of using the sample CLIs, check all of the following:

- Call `init()` before `start()`.
- Register `set_on_connect()`, `set_on_connected()`, and `set_on_disconnected()` callbacks.
- In `set_on_connect()`, attach `conn->get_context()` to your libuv or libevent loop.
- Drive `proc(sec, usec)` from a timer callback or equivalent event-loop tick.
- Keep `raw`, `cluster`, `connection`, and the underlying `redisAsyncContext` on one event-loop thread unless you add external synchronization.

If one of these pieces is missing, the connection can exist on paper while no actual async progress happens in practice—classic distributed systems slapstick.

### Cluster commands route unexpectedly or keep redirecting

- Pass the real routing key to `cluster::exec(key, key_len, ...)`.
- Use Redis hash tags when multiple keys must hit the same slot, for example `{user:42}:profile` and `{user:42}:settings`.
- For commands without a natural key, pass `nullptr, 0` to request random routing, matching the sample CLI behavior.
- If a command still behaves unexpectedly, verify that the command really belongs in the normal request-response flow and that the selected key is the same key Redis Cluster will hash.

### `SUBSCRIBE`, `PSUBSCRIBE`, `UNSUBSCRIBE`, `PUNSUBSCRIBE`, or `MONITOR` behaves strangely

These commands do not fit the library's normal request-response `exec()` lifecycle.

- Do **not** send them through `exec()`.
- Wait until the connection is available.
- Use `connection::redis_raw_cmd()` instead.

The sample CLIs show this split explicitly: normal commands go through `exec()`, while subscribe/monitor-style commands go through the raw path.

## Minimal command examples

Before calling `exec()`, initialize the connector with `init()`, register `set_on_connect()` / `set_on_connected()` / `set_on_disconnected()` callbacks, attach the hiredis async context to your event loop in `set_on_connect()`, call `start()`, and drive `proc()` from your timer callback or event-loop tick. The sample CLIs above are the runnable libuv/libevent examples.

### Raw connector (`hiredis::happ::raw`)

```cpp
const char *argv[] = {"SET", "user:42", "hello"};
size_t argvlen[] = {3, 7, 5};

raw_client.exec(on_reply, nullptr, 3, argv, argvlen);
```

### Cluster connector (`hiredis::happ::cluster`)

```cpp
const char *argv[] = {"SET", "{user:42}:profile", "hello"};
size_t argvlen[] = {3, 17, 5};

cluster_client.exec(argv[1], argvlen[1], on_reply, nullptr, 3, argv, argvlen);
```

Pass `nullptr, 0` as the cluster key for commands that should be routed randomly or do not have a natural key, matching the behavior in [`sample/sample_cluster_cli/main.cpp`](sample/sample_cluster_cli/main.cpp).

### Subscribe-family and monitor commands

`SUBSCRIBE`, `PSUBSCRIBE`, `UNSUBSCRIBE`, `PUNSUBSCRIBE`, and `MONITOR` are not request-response commands, so they must not go through the normal `exec()` lifecycle. Use `connection::redis_raw_cmd()` after the connection is available instead.

```cpp
conn->redis_raw_cmd(subscribe_callback, user_data, "SUBSCRIBE %s", "demo-channel");
```

## Documentation

- [Code review report - 2026-05-26](doc/code-review-2026-05-26.md)
- [Roadmap & playbook](doc/ROADMAP.md)
- [AI configuration source index](doc/ai/source-index.md)
- [Historical design draft](doc/Redis全异步(HA)Driver设计稿.md)

## Behavior and limits

- The normal `exec()` lifecycle supports request-response commands only.
- Treat `hiredis::happ::raw`, `hiredis::happ::cluster`, `hiredis::happ::connection`, and the underlying hiredis `redisAsyncContext` as single-event-loop objects unless you add external synchronization.
- Cluster routing supports Redis hash tags and common redirections such as `MOVED`, `ASK`, and `TRYAGAIN`.
- Sentinel is still documented as a design idea only.

## Repository layout

| Path | Purpose |
| --- | --- |
| `include/` | Public headers, including `hiredis_happ.h` and connector internals. |
| `src/` | Library implementation files. |
| `sample/` | Raw and cluster CLI samples. |
| `test/` | Unit tests and regression coverage. |
| `doc/` | Roadmap, review notes, design draft, and AI source index. |
| `project/` | Project-specific CMake helpers. |
| `third_party/` | Third-party dependency bootstrap/install support. |
| `ci/` | CI entry scripts and helper utilities. |
| `.agents/skills/` | Project-specific agent skills and maintenance notes. |
