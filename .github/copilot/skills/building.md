# hiredis-happ - Build System

## CMake Build System

This project uses **CMake** (minimum version 3.24.0).

Prefer out-of-source builds. A clean checkout should not be left dirty by generated headers or build artifacts; if a build change creates source-tree output, call it out in review and roadmap.

### Build Commands

```bash
# Clone and configure
git clone --single-branch --depth=1 -b main https://github.com/owent/hiredis-happ.git
mkdir hiredis-happ/build_jobs_dir && cd hiredis-happ/build_jobs_dir

# Configure with unit tests enabled
cmake .. -DPROJECT_HIREDIS_HAPP_ENABLE_SAMPLE=YES -DPROJECT_HIREDIS_HAPP_ENABLE_UNITTEST=YES

# Build
cmake --build .                          # Linux/macOS (GCC/Clang)
cmake --build . --config RelWithDebInfo  # Windows (MSVC)

# Run tests via CTest
ctest . -V
```

On Windows, prepend `<repo>\third_party\install\windows-amd64-msvc-19\bin` to `PATH` before running tests or samples so the `hiredis.dll` runtime dependency is found.

### Key CMake Options

| Option                                 | Default        | Description                     |
| -------------------------------------- | -------------- | ------------------------------- |
| `BUILD_SHARED_LIBS`                    | OFF            | Build dynamic library           |
| `CMAKE_BUILD_TYPE`                     | RelWithDebInfo | Build type (Debug/Release/etc.) |
| `PROJECT_HIREDIS_HAPP_ENABLE_SAMPLE`   | OFF            | Build sample applications       |
| `PROJECT_HIREDIS_HAPP_ENABLE_UNITTEST` | OFF            | Build unit tests                |

## Compiler Support

| Compiler    | Minimum Version |
| ----------- | --------------- |
| GCC         | 4.8+            |
| Clang       | 3.4+            |
| Apple Clang | 6.0+            |
| MSVC        | VS2022+         |

## Dependencies

- **hiredis** - Required, the underlying Redis client library

## CI Matrix Notes

- Linux GCC, Linux Clang/libc++, macOS AppleClang/libc++, and Windows VS2022 shared/static are expected paths.
- `.github/workflows/main.yml` also runs format and CodeQL jobs.
- When editing workflow expressions, validate `${{ ... }}` syntax carefully; cache keys and matrix variables are easy to break silently.
- For Windows test jobs, ensure runtime DLL directories are added to `PATH`, not only link directories.

## Packaging Review Checklist

- Public headers include `detail/hiredis_happ_config.h`; if generation changes, ensure install-tree consumers can still include `<hiredis_happ.h>`.
- Keep exported target namespace as `hiredis::` unless intentionally changing the public CMake API.
- Future package work should add `hiredis-happ-config.cmake`, a version file, and an install-tree smoke test.
