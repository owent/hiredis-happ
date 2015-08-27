hiredis-happ
======
Redis HA connector

Usage
------

### Linux & Cygwin(hiredis not available in cygwin now)
```bash
git clone https://github.com/owt5008137/hiredis-happ.git;
mkdir -p hiredis-happ/build && cd hiredis-happ/build;
cmake ..; # cmake .. -DCMAKE_INSTALL_PREFIX=/usr;
make;

# for install only
make install
```

### Windows(hiredis do not support MS VC now)
```bat
git clone https://github.com/owt5008137/hiredis-happ.git
mkdir hiredis-happ/build
cd hiredis-happ/build

cmake .. -G "Visual Studio 14 2015 Win64"; # cmake .. -DCMAKE_INSTALL_PREFIX="%ProgramFiles%" -G "Visual Studio 14 2015 Win64"

: using visual studio to build

: or using msbuild when it's in PATH
msbuild hiredis-happ.sln /p:Configuration=Release
```

Document
------
See [doc](doc) 

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

**tools**        -- misc tools