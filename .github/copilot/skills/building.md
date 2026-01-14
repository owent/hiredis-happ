# hiredis-happ - Build System

## CMake Build System

This project uses **CMake** (minimum version 3.24.0).

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
