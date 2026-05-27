#!/bin/bash

cd "$(cd "$(dirname $0)" && pwd)/..";

set -ex ;

PROJECT_ROOT="$PWD" ;
REDIS_FIXTURE_SH="$PROJECT_ROOT/test/redis/redis-fixture.sh" ;
REDIS_FIXTURE_PS1="$PROJECT_ROOT/test/redis/redis-fixture.ps1" ;

is_windows_shell() {
  local system_name="$(uname -s 2>/dev/null || true)" ;
  case "$system_name" in
    MINGW*|MSYS*|CYGWIN*)
      return 0 ;;
  esac

  [[ -n "${MSYSTEM:-}" ]]
}

to_host_path() {
  local path_value="$1" ;
  if command -v cygpath >/dev/null 2>&1; then
    cygpath -aw "$path_value"
  else
    echo "$path_value"
  fi
}

run_redis_fixture_cmd() {
  local command_name="$1" ;

  if is_windows_shell; then
    local ps1_path="$(to_host_path "$REDIS_FIXTURE_PS1")" ;
    if command -v pwsh >/dev/null 2>&1; then
      pwsh -NoLogo -NoProfile -File "$ps1_path" "$command_name"
    else
      powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "$ps1_path" "$command_name"
    fi
  else
    bash "$REDIS_FIXTURE_SH" "$command_name"
  fi
}

export_redis_fixture_env() {
  while IFS='=' read -r KEY VALUE; do
    VALUE="${VALUE%$'\r'}" ;
    export "$KEY=$VALUE" ;
  done < <(run_redis_fixture_cmd print-env)
}

cleanup_redis_fixture() {
  run_redis_fixture_cmd stop-all || true ;
  run_redis_fixture_cmd cleanup || true ;
}

should_run_redis_integration() {
  local with_redis="${HIREDIS_HAPP_TEST_WITH_REDIS:-ON}" ;
  with_redis="$(echo "$with_redis" | tr '[:lower:]' '[:upper:]')" ;

  case "$with_redis" in
    0|OFF|FALSE|NO)
      return 1 ;;
  esac

  return 0 ;
}

run_unit_ctest_suite() {
  ctest . -V -R hiredis-happ-run-test ;
}

run_ctest_suite_with_redis() {
  export HIREDIS_HAPP_TEST_REDIS_BUILD_JOBS="${HIREDIS_HAPP_TEST_REDIS_BUILD_JOBS:-2}" ;
  trap cleanup_redis_fixture EXIT ;
  run_redis_fixture_cmd start-all ;
  export_redis_fixture_env ;
  ctest . -V -R hiredis-happ-run-test ;
  ctest . -V -R hiredis-happ-redis-integration-raw --timeout 120 ;
  ctest . -V -R hiredis-happ-redis-integration-cluster --timeout 180 ;
  trap - EXIT ;
  cleanup_redis_fixture ;
}

run_platform_ctest_suite() {
  if should_run_redis_integration; then
    run_ctest_suite_with_redis ;
  else
    echo "Redis integration is disabled for this run (HIREDIS_HAPP_TEST_WITH_REDIS=${HIREDIS_HAPP_TEST_WITH_REDIS:-ON}). Running unit tests only." ;
    run_unit_ctest_suite ;
  fi
}

if [[ "x$USE_CC" == "xclang-latest" ]]; then
  echo '#include <iostream>
  int main() { std::cout<<"Hello"; }' > test-libc++.cpp
  SELECT_CLANG_VERSION="";
  SELECT_CLANG_HAS_LIBCXX=1;
  clang -x c++ -stdlib=libc++ test-libc++.cpp -lc++ -lc++abi || SELECT_CLANG_HAS_LIBCXX=0;
  if [[ $SELECT_CLANG_HAS_LIBCXX -eq 0 ]]; then
    CURRENT_CLANG_VERSION=$(clang -x c /dev/null -dM -E | grep __clang_major__ | awk '{print $NF}');
    for ((i=$CURRENT_CLANG_VERSION+5;$i>=$CURRENT_CLANG_VERSION;--i)); do
      SELECT_CLANG_HAS_LIBCXX=1;
      SELECT_CLANG_VERSION="-$i";
      clang$SELECT_CLANG_VERSION -x c++ -stdlib=libc++ test-libc++.cpp -lc++ -lc++abi || SELECT_CLANG_HAS_LIBCXX=0;
      if [[ $SELECT_CLANG_HAS_LIBCXX -eq 1 ]]; then
        break;
      fi
    done
  fi
  SELECT_CLANGPP_BIN=clang++$SELECT_CLANG_VERSION;
  LINK_CLANGPP_BIN=0;
  which $SELECT_CLANGPP_BIN || LINK_CLANGPP_BIN=1;
  if [[ $LINK_CLANGPP_BIN -eq 1 ]]; then
    mkdir -p .local/bin ;
    ln -s "$(which "clang$SELECT_CLANG_VERSION")" "$PWD/.local/bin/clang++$SELECT_CLANG_VERSION" ;
    export PATH="$PWD/.local/bin:$PATH";
  fi
  export USE_CC=clang$SELECT_CLANG_VERSION;
elif [[ "x$USE_CC" == "xgcc-latest" ]]; then
  CURRENT_GCC_VERSION=$(gcc -x c /dev/null -dM -E | grep __GNUC__ | awk '{print $NF}');
  echo '#include <iostream>
  int main() { std::cout<<"Hello"; }' > test-gcc-version.cpp ;
  let LAST_GCC_VERSION=$CURRENT_GCC_VERSION+10 ;
  for ((i=$CURRENT_GCC_VERSION;$i<=$LAST_GCC_VERSION;++i)); do
    TEST_GCC_VERSION=1;
    g++-$i -x c++ test-gcc-version.cpp || TEST_GCC_VERSION=0;
    if [[ $TEST_GCC_VERSION -eq 0 ]]; then
      break;
    fi
    CURRENT_GCC_VERSION=$i;
  done
  export USE_CC=gcc-$CURRENT_GCC_VERSION ;
  echo "Using $USE_CC" ;
fi

if [[ "$1" == "format" ]]; then
  python3 -m pip install --user -r ./ci/requirements.txt ;
  export PATH="$HOME/.local/bin:$PATH"
  bash ./ci/format.sh ;
  CHANGED="$(git -c core.autocrlf=true ls-files --modified)" ;
  if [[ ! -z "$CHANGED" ]]; then
    echo "The following files have changes:" ;
    echo "$CHANGED" ;
    git diff ;
    # exit 1 ; # Just warning, some versions of clang-format have different default style for unsupport syntax
  fi
  exit 0 ;
elif [[ "$1" == "ssl.openssl" ]]; then
  CRYPTO_OPTIONS="-DATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_CRYPTO_USE_OPENSSL=ON" ;
  bash cmake_dev.sh -lus -b Debug -r build_jobs_ci -c $USE_CC -- $CRYPTO_OPTIONS \
    "-DATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LOW_MEMORY_MODE=ON" \
    "-DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS:-OFF}"
  cd build_jobs_ci ;
  cmake --build . -j ;
  run_platform_ctest_suite ;
elif [[ "$1" == "codeql.configure" ]]; then
  CRYPTO_OPTIONS="-DATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_CRYPTO_USE_OPENSSL=ON"
  bash cmake_dev.sh -lus -b RelWithDebInfo -r build_jobs_ci -c $USE_CC -- $CRYPTO_OPTIONS \
    "-DATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LOW_MEMORY_MODE=ON"
elif [[ "$1" == "codeql.build" ]]; then
  cd build_jobs_ci
  cmake --build . -j --config RelWithDebInfo || cmake --build . --config RelWithDebInfo
elif [[ "$1" == "gcc.legacy.test" ]]; then
  bash cmake_dev.sh -lus -b Debug -r build_jobs_ci -c $USE_CC ;
  cd build_jobs_ci ;
  cmake --build . -j ;
  run_platform_ctest_suite ;
elif [[ "$1" == "msys2.mingw.test" ]]; then
  pacman -S --needed --noconfirm mingw-w64-x86_64-cmake git m4 curl wget tar autoconf automake  \
    mingw-w64-x86_64-git-lfs mingw-w64-x86_64-toolchain mingw-w64-x86_64-libtool                \
    mingw-w64-x86_64-python mingw-w64-x86_64-python-pip mingw-w64-x86_64-python-setuptools || true ;
  git config --global http.sslBackend openssl ;
  mkdir -p build_jobs_ci ;
  cd build_jobs_ci ;
  cmake .. -G 'MinGW Makefiles' "-DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS" "-DPROJECT_HIREDIS_HAPP_ENABLE_UNITTEST=YES" \
    "-DPROJECT_HIREDIS_HAPP_ENABLE_SAMPLE=YES" "-DATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LOW_MEMORY_MODE=ON";
  cmake --build . -j ;
  for EXT_PATH in $(find ../third_party/install/ -name "*.dll" | xargs dirname | sort -u); do
    export PATH="$PWD/$EXT_PATH:$PATH"
  done
  run_platform_ctest_suite ;
fi
