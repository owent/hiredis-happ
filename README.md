# hiredis-happ

Redis HA connector

Environment  | [Linux+OSX(clang+gcc)][linux-link] | [Windows+MinGW(vc+gcc)][windows-link] 
-------------|------------------------------------|-------------------------------------------
Build Status | ![linux-badge]                     | ![windows-badge]
Compilers | linux-clang-10/11 <br /> linux-gcc-9 <br /> linux-gcc-10 <br /> ~~macOS-clang-12.0~~ <br /> | MSVC 14(Visual Studio 2015) <br /> MSVC 15(Visual Studio 2017) <br /> MSVC 16(Visual Studio 2019) <br /> Mingw32-gcc <br /> Mingw64-gcc

[linux-badge]: https://github.com/owt5008137/hiredis-happ/actions/workflows/main.yml/badge.svg "Github action build status"
[linux-link]:  https://github.com/owt5008137/hiredis-happ/actions/workflows/main.yml "Github action build status"
[windows-badge]: https://ci.appveyor.com/api/projects/status/tp0bkc9ltorakfvs?svg=true "AppVeyor build status"
[windows-link]:  https://ci.appveyor.com/project/owt5008137/hiredis-happ "AppVeyor build status"

## Tips

1. auto reconnect
2. support redis cluster
3. ~~[TODO] support redis sential~~
4. support raw redis connection

## Usage


```bash
git clone https://github.com/owt5008137/hiredis-happ.git;
mkdir -p hiredis-happ/build_jobs_dir && cd hiredis-happ/build_jobs_dir;
# cmake -DDCMAKE_INSTALL_PREFIX=/usr
# cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake .. ;
cmake --build . ;

# for install only, if not installed, all header files are in [BUILDDIR]/include, all libraries files are in [BUILDDIR]/lib
cmake --build . -- install
```

### Custom CMake Options

+ **CMAKE_BUILD_TYPE**: This cmake option will be default set to **RelWithDebInfo**.(Please add -DCMAKE_BUILD_TYPE=RelWithDebInfo when used in product environment)
+ **BUILD_SHARED_LIBS**: Default set to OFF
+ **ENABLE_BOOST_UNIT_TEST**: If using [boost.unittest](http://www.boost.org/libs/test/doc/html/index.html) for test framework(default: OFF)
+ **PROJECT_HIREDIS_HAPP_ENABLE_SAMPLE**: If building samples(default: OFF)
+ **PROJECT_HIREDIS_HAPP_ENABLE_UNITTEST**:  If building unittest(default: OFF)

### Sample

See [sample_cluster_cli](sample/sample_cluster_cli) for redis cluster practice and [sample_raw_cli](sample/sample_raw_cli) for raw redis connection.

Both [happ_cluster](include/detail/happ_cluster.h) and [happ_raw](include/detail/happ_raw.h) support auto reconnecting and retry when cmd failed.

You can also custom how to print log by using *set_log_writer* to help you to find any problem.

## Document

See [doc](doc) 

## Notice

This lib only support Request-Response commands now.(means every request should has only one response).

### Unsupport Commands

+ subscribe
+ unsubscribe
+ monitor

### Directory list

**3rd_party**   -- script for 3rd party  libraries

**doc**         -- document

**include**     -- include files

**project**     -- project configure

**src**         -- source

**sample**      -- samples

**test**        -- unit test

**tools**       -- misc tools