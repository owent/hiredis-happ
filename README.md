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
|Linux|GCC|Unit + Redis integration|
|Linux|Clang|With libc++ + Redis integration|
|Windows|Visual Studio 2022|Static linking + WSL Redis integration|
|Windows|Visual Studio 2022|Dynamic linking + WSL Redis integration|
|macOS|AppleClang|With libc++ + Redis integration|

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

### Unit tests

```powershell
ctest --test-dir build_jobs_review -V -R hiredis-happ-run-test -C RelWithDebInfo --timeout 120
```

`hiredis-happ-run-test` covers the pure unit/regression groups: `happ_cmd`, `happ_connection`, `happ_cluster`, and `happ_raw`.

### Redis fixture scripts and integration tests

The Redis-backed integration targets are split from the unit target:

- `hiredis-happ-redis-integration-raw`
- `hiredis-happ-redis-integration-cluster`

The repository-owned end-to-end test flows now run all three CTest entries in one pass and clean temporary Redis processes automatically at the end:

- Linux/macOS: `bash ci/do_ci.sh ssl.openssl`
- Legacy GCC flow: `bash ci/do_ci.sh gcc.legacy.test`
- Windows MSVC: `pwsh ci/do_ci.ps1 msvc.modern.test`

Use the direct fixture commands below when you want to run only the Redis-backed tests or inspect the temporary Redis instances manually.

The fixture scripts under `test/redis/` download the official `redis-stable.tar.gz`, build `redis-server` / `redis-cli`, start a standalone Redis on `127.0.0.1:6390`, and create a temporary 6-node cluster with seed node `127.0.0.1:7300`.

On Linux, macOS, or WSL:

```bash
bash ./test/redis/redis-fixture.sh start-all
while IFS='=' read -r key value; do export "$key=$value"; done < <(bash ./test/redis/redis-fixture.sh print-env)
ctest --test-dir build_jobs_review -V -R hiredis-happ-redis-integration-raw --timeout 120
ctest --test-dir build_jobs_review -V -R hiredis-happ-redis-integration-cluster --timeout 180
bash ./test/redis/redis-fixture.sh cleanup
```

On Windows, use the PowerShell wrapper. It requires WSL plus an installed Linux distribution because official Redis OSS server binaries are not provided for native Windows:

```powershell
.\test\redis\redis-fixture.ps1 start-all
$envLines = .\test\redis\redis-fixture.ps1 print-env
foreach ($line in $envLines) {
  if ($line -match '^([^=]+)=(.*)$') {
    Set-Item -Path ("Env:" + $Matches[1]) -Value $Matches[2]
  }
}
ctest --test-dir build_jobs_review -V -R hiredis-happ-redis-integration-raw -C RelWithDebInfo --timeout 120
ctest --test-dir build_jobs_review -V -R hiredis-happ-redis-integration-cluster -C RelWithDebInfo --timeout 180
.\test\redis\redis-fixture.ps1 cleanup
```

If you have multiple WSL distros installed, set `HIREDIS_HAPP_TEST_WSL_DISTRO` before running the wrapper to pin a specific distro. The wrapper terminates that distro after `stop-*` / `cleanup` so temporary Redis processes do not linger after the test flow finishes.

The fixture scripts honor `HIREDIS_HAPP_TEST_SINGLE_HOST`, `HIREDIS_HAPP_TEST_SINGLE_PORT`, `HIREDIS_HAPP_TEST_CLUSTER_HOST`, `HIREDIS_HAPP_TEST_CLUSTER_PORT`, and related `HIREDIS_HAPP_TEST_*` environment overrides printed by `print-env`.

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

### Redis integration fixtures do not start on Windows

The PowerShell wrapper under `test/redis/redis-fixture.ps1` requires:

- WSL enabled.
- At least one installed Linux distribution (for example Ubuntu).
- A working `wsl` command from the current shell.

The GitHub Actions Windows job bootstraps an Ubuntu distro before invoking the main MSVC test flow so Redis integration coverage stays inside the normal Windows build/test matrix.

If the wrapper tells you no distribution is installed, run `wsl --list --online` and then `wsl --install <Distro>` first.

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
