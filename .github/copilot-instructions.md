# hiredis-happ - Copilot Instructions

## Project Overview

**hiredis-happ** is a C++ library providing high-availability Redis connectors with automatic reconnection and cluster support. It wraps [hiredis](https://github.com/redis/hiredis) to provide:

- Auto reconnect functionality
- Redis cluster support
- Raw redis connection support

- **Repository**: https://github.com/owent/hiredis-happ
- **License**: MIT
- **Languages**: C++ (C++17 minimum)

## Directory Structure

```
hiredis-happ/
├── include/             # Public headers
│   ├── hiredis_happ.h   # Main include file
│   └── detail/          # Implementation details
│       ├── happ_cluster.h      # Cluster connector
│       ├── happ_raw.h          # Raw connection
│       ├── happ_connection.h   # Connection base
│       ├── happ_cmd.h          # Command wrapper
│       └── crc16.h             # CRC16 for slot calculation
├── src/                 # Implementation files
│   ├── hiredis_happ.cpp
│   └── detail/
├── test/                # Unit tests
│   └── case/            # Test case implementations
├── sample/              # Sample applications
│   ├── sample_cluster_cli/  # Redis cluster sample
│   └── sample_raw_cli/      # Raw connection sample
├── project/             # Project configuration
│   └── cmake/
├── doc/                 # Documentation
└── third_party/         # Third-party dependencies
```

## Key Components

### Main Header (`include/hiredis_happ.h`)

Include this header to use the library:

```cpp
#include <hiredis_happ.h>
```

### Cluster Connector (`include/detail/happ_cluster.h`)

- `hiredis::happ::cluster` - Redis cluster connector with auto-reconnect
- Supports slot-based routing
- Automatic MOVED/ASK redirection handling

### Raw Connector (`include/detail/happ_raw.h`)

- `hiredis::happ::raw` - Single Redis connection with auto-reconnect
- Simpler API for non-cluster setups

### Connection (`include/detail/happ_connection.h`)

- `hiredis::happ::connection` - Base connection class
- Authentication support via `set_auth_password()` or `set_auth_fn()`

### Command (`include/detail/happ_cmd.h`)

- `hiredis::happ::cmd_exec` - Command wrapper
- Callback-based async execution

## Coding Conventions

1. **Namespace**: All code is in `hiredis::happ` namespace
2. **Include guards**: Use `#ifndef` guards with `#pragma once`
3. **C++ Standard**: C++17 minimum
4. **Naming**:
   - Classes/structs: `snake_case`
   - Functions: `snake_case`
   - Constants: `UPPER_SNAKE_CASE`
   - Member variables: `snake_case_`
5. **API Macros**: Use `HIREDIS_HAPP_API` for public API functions

## Unsupported Commands

This library only supports Request-Response commands. The following are NOT supported:

- `subscribe`
- `unsubscribe`
- `monitor`

These commands don't follow the request-response pattern and require special handling through `connection::redis_raw_cmd`.
