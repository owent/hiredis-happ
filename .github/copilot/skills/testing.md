# hiredis-happ - Unit Testing Framework

This project uses **atframe_utils** private unit testing framework (not Google Test). The framework provides similar macros.

## Test Framework Macros

```cpp
// Define a test case
CASE_TEST(test_group_name, test_case_name) {
    // Test implementation
}

// Assertions
CASE_EXPECT_TRUE(condition)
CASE_EXPECT_FALSE(condition)
CASE_EXPECT_EQ(expected, actual)
CASE_EXPECT_NE(val1, val2)
CASE_EXPECT_LT(val1, val2)
CASE_EXPECT_LE(val1, val2)
CASE_EXPECT_GT(val1, val2)
CASE_EXPECT_GE(val1, val2)
CASE_EXPECT_ERROR(message)

// Logging during tests
CASE_MSG_INFO() << "Info message";
CASE_MSG_ERROR() << "Error message";

// Test utilities
CASE_THREAD_SLEEP_MS(milliseconds)
CASE_THREAD_YIELD()
```

## Regression Focus Areas

- Redis Cluster hash tags and slot routing.
- `MOVED`, `ASK + ASKING`, `TRYAGAIN`, `CLUSTERDOWN`, and malformed `CLUSTER SLOTS` replies.
- hiredis async lifecycle: connect failure, disconnect, reset while commands are pending, and callbacks with `reply == nullptr`.
- Non request-response commands must use `connection::redis_raw_cmd()` instead of normal `exec()`.
- Logging paths should be tested with small buffers to catch truncation/termination issues.

## Running Tests

The test executable is `hiredis-happ-test`.

```bash
# Run all tests
./hiredis-happ-test

# List all test cases
./hiredis-happ-test -l
./hiredis-happ-test --list-tests

# Run specific test group(s) or case(s)
./hiredis-happ-test -r <test_group_name>
./hiredis-happ-test -r <test_group_name>.<test_case_name>

# Run with filter pattern (supports wildcards)
./hiredis-happ-test -f "cluster*"
./hiredis-happ-test --filter "happ*"

# Show help
./hiredis-happ-test -h
./hiredis-happ-test --help

# Show version
./hiredis-happ-test -v
./hiredis-happ-test --version
```

## Windows: DLL lookup via PATH

On Windows, running `hiredis-happ-test.exe` (or samples) from a build directory may fail if dependent DLLs are not discoverable. The easiest fix is to **prepend those folders to `PATH`** for the current session.

Example (PowerShell):

```powershell
$repoRoot = "<REPO_ROOT>"  # e.g. D:\workspace\git\github\owent\hiredis-happ
$buildDir = "$repoRoot\build_jobs_dir"
$cfg = "RelWithDebInfo"

$env:PATH = "$repoRoot\third_party\install\windows-amd64-msvc-19\bin;" + $env:PATH
Set-Location "$buildDir\test"
.\$cfg\hiredis-happ-test.exe -r happ_cluster
```

For CTest on Windows, set the same `PATH` first; otherwise `hiredis-happ-test.exe` can fail before `main()` with `0xC0000135` because `hiredis.dll` is not discoverable.

## Writing Test Cases

Test files are located in `test/case/`. Example:

```cpp
#include "frame/test_macros.h"
#include "hiredis_happ.h"

CASE_TEST(happ_cluster, connection_callback) {
    hiredis::happ::cluster clu;

    clu.init("127.0.0.1", 6370);
    clu.set_timeout(57);
    clu.start();

    CASE_EXPECT_EQ(static_cast<size_t>(1), clu.get_timer_actions().timer_conns.size());

    clu.reset();
}
```

For lifecycle behavior that depends on hiredis callbacks, prefer integration tests with a real event loop and ASAN/UBSAN over pure object-state tests.
