hiredis-happ
======
Redis HA connector

Function
------
1. auto reconnect
2. support redis cluster
3. [TODO] support redis sential
4. support raw redis connection

Usage
------

### Linux & Cygwin(hiredis not available in cygwin now)
```bash
git clone https://github.com/owt5008137/hiredis-happ.git;
mkdir -p hiredis-happ/build && cd hiredis-happ/build;
# cmake -DDCMAKE_INSTALL_PREFIX=/usr
# cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake ..;
make;

# for install only, if not installed, all header files are in [BUILDDIR]/include, all libraries files are in [BUILDDIR]/lib
make install
```

### Windows + MSVC(hiredis do not support MS VC now, so it's untested)
```bat
git clone https://github.com/owt5008137/hiredis-happ.git
mkdir hiredis-happ/build
cd hiredis-happ/build

:: cmake .. -DLIBHIREDIS_ROOT=[hiredis install prefix] -G "Visual Studio 14 2015 Win64"
:: cmake .. -DCMAKE_INSTALL_PREFIX="%ProgramFiles%" -G "Visual Studio 14 2015 Win64"
:: cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo -G "Visual Studio 14 2015 Win64"
cmake .. -G "Visual Studio 14 2015 Win64";

:: using visual studio to build

:: or using msbuild when it's in PATH
msbuild hiredis-happ.sln /p:Configuration=RelWithDebInfo
```

### Custom CMake Options
+ **CMAKE_BUILD_TYPE**: This cmake option will be default set to **Debug**.(Please add -DCMAKE_BUILD_TYPE=RelWithDebInfo when used in product environment)
+ **LIBHIREDIS_ROOT**: Where to find hiredis headers and libraries
+ **HIREDIS_VERSION**: Hiredis version to download when can not find a available hiredis
+ **BUILD_SHARED_LIBS**: Default set to OFF
+ **ENABLE_BOOST_UNIT_TEST**: If using [boost.unittest](http://www.boost.org/libs/test/doc/html/index.html) for test framework(default: OFF)
+ **PROJECT_ENABLE_SAMPLE**: If building samples(default: OFF)
+ **PROJECT_ENABLE_UNITTEST**:  If building unittest(default: OFF)
+ **LIBHIREDIS_INCLUDE_DIRS** and **LIBHIREDIS_LIBRARIES**: Where to find hiredis libraries and include directory, these two option should be set both.
+ **LIBHIREDIS_USING_SRC**: If **LIBHIREDIS_INCLUDE_DIRS** is the source directory of hiredis

Document
------
See [doc](doc) 

Notice
------
This lib only support Request-Response commands now.(means every request should has only one response).

### Unsupport Commands
+ subscribe
+ unsubscribe
+ monitor

Environment  | Linux (GCC)         | Windows 
-------------|---------------------|---------
Build Status | [![Build Status](https://travis-ci.org/owt5008137/hiredis-happ.svg)](https://travis-ci.org/owt5008137/hiredis-happ) | [![Build status](https://ci.appveyor.com/api/projects/status/tp0bkc9ltorakfvs?svg=true)](https://ci.appveyor.com/project/owt5008137/hiredis-happ)


### Directory list

**3rd_party**   -- script for 3rd party  libraries

**doc**         -- document

**include**     -- include files

**project**     -- project configure

**src**         -- source

**sample**      -- samples

**test**        -- unit test

**tools**       -- misc tools