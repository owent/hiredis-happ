$PSDefaultParameterValues['*:Encoding'] = 'UTF-8'

$OutputEncoding = [System.Text.UTF8Encoding]::new()

$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Definition
$WORK_DIR = Get-Location

if ($IsWindows) {
  # See https://docs.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=cmd
  New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" `
    -Name "LongPathsEnabled" -Value 1 -PropertyType DWORD -Force

  if (Test-Path "${Env:USERPROFILE}/scoop/apps/perl/current/perl/bin") {
    $Env:PATH = $Env:PATH + [IO.Path]::PathSeparator + "${Env:USERPROFILE}/scoop/apps/perl/current/perl/bin"
  }

  function Invoke-Environment {
    param
    (
      [Parameter(Mandatory = $true)]
      [string] $Command
    )
    cmd /c "$Command > nul 2>&1 && set" | . { process {
        if ($_ -match '^([^=]+)=(.*)') {
          [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2])
        }
      } }
  }
  $vswhere = "${Env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
  $vsInstallationPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  $winSDKDir = $(Get-ItemPropertyValue -Path "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Microsoft SDKs\Windows\v10.0" -Name "InstallationFolder")
  if ([string]::IsNullOrEmpty($winSDKDir)) {
    $winSDKDir = "${Env:ProgramFiles(x86)}/Windows Kits/10/Include/"
  }
  else {
    $winSDKDir = "$winSDKDir/Include/"
  }
  foreach ($sdk in $(Get-ChildItem $winSDKDir | Sort-Object -Property Name)) {
    if ($sdk.Name -match "[0-9]+\.[0-9]+\.[0-9\.]+") {
      $selectWinSDKVersion = $sdk.Name
    }
  }
  if (!(Test-Path Env:WindowsSDKVersion)) {
    $Env:WindowsSDKVersion = $selectWinSDKVersion
  }
  # Maybe using $selectWinSDKVersion = "10.0.18362.0" for better compatible
  Write-Output "Window SDKs:(Latest: $selectWinSDKVersion)"
  foreach ($sdk in $(Get-ChildItem $winSDKDir | Sort-Object -Property Name)) {
    Write-Output "  - $sdk"
  }
}

Set-Location "$SCRIPT_DIR/.."
$PROJECT_DIR = Split-Path -Parent $SCRIPT_DIR
$RUN_MODE = $args[0]

function Assert-LastNativeExitCode {
  param(
    [Parameter(Mandatory = $true)]
    [string]$StepName
  )

  if ($LASTEXITCODE -ne 0) {
    throw "$StepName failed with exit code $LASTEXITCODE"
  }
}

function Get-RedisFixtureScriptPath {
  Join-Path $PROJECT_DIR 'test\redis\redis-fixture.ps1'
}

function Invoke-RedisFixtureCommand {
  param(
    [Parameter(Mandatory = $true)]
    [string]$CommandName
  )

  $fixtureScript = Get-RedisFixtureScriptPath
  & $fixtureScript $CommandName
  Assert-LastNativeExitCode -StepName "Redis fixture command '$CommandName'"
}

function Import-RedisFixtureEnvironment {
  $fixtureScript = Get-RedisFixtureScriptPath
  $envLines = & $fixtureScript 'print-env'
  Assert-LastNativeExitCode -StepName "Redis fixture command 'print-env'"

  foreach ($line in $envLines) {
    if ($line -match '^(?<name>[^=]+)=(?<value>.*)$') {
      Set-Item -Path ("Env:" + $Matches['name']) -Value $Matches['value']
    }
  }
}

function Cleanup-RedisFixture {
  $fixtureScript = Get-RedisFixtureScriptPath
  foreach ($commandName in @('stop-all', 'cleanup')) {
    try {
      & $fixtureScript $commandName
    }
    catch {
      Write-Warning "Redis fixture cleanup command '$commandName' failed: $($_.Exception.Message)"
    }
  }
}

function Test-ShouldRunRedisIntegration {
  $withRedis = [Environment]::GetEnvironmentVariable('HIREDIS_HAPP_TEST_WITH_REDIS')
  if ([string]::IsNullOrWhiteSpace($withRedis)) {
    return $true
  }

  switch ($withRedis.Trim().ToUpperInvariant()) {
    '0' { return $false }
    'OFF' { return $false }
    'FALSE' { return $false }
    'NO' { return $false }
    default { return $true }
  }
}

function Invoke-UnitCTestSuite {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Configuration
  )

  & ctest . -V -C $Configuration -R hiredis-happ-run-test
  Assert-LastNativeExitCode -StepName 'Unit CTest suite'
}

function Invoke-CTestSuiteWithRedis {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Configuration
  )

  if ([string]::IsNullOrWhiteSpace($Env:HIREDIS_HAPP_TEST_REDIS_BUILD_JOBS)) {
    $Env:HIREDIS_HAPP_TEST_REDIS_BUILD_JOBS = '2'
  }

  try {
    Invoke-RedisFixtureCommand -CommandName 'start-all'
    Import-RedisFixtureEnvironment

    & ctest . -V -C $Configuration -R hiredis-happ-run-test
    Assert-LastNativeExitCode -StepName 'Unit CTest suite'

    & ctest . -V -C $Configuration -R hiredis-happ-redis-integration-raw --timeout 120
    Assert-LastNativeExitCode -StepName 'Raw Redis integration CTest suite'

    & ctest . -V -C $Configuration -R hiredis-happ-redis-integration-cluster --timeout 180
    Assert-LastNativeExitCode -StepName 'Cluster Redis integration CTest suite'
  }
  finally {
    Cleanup-RedisFixture
  }
}

function Invoke-PlatformCTestSuite {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Configuration
  )

  if (Test-ShouldRunRedisIntegration) {
    Invoke-CTestSuiteWithRedis -Configuration $Configuration
    return
  }

  Write-Output "Redis integration is disabled for this run (HIREDIS_HAPP_TEST_WITH_REDIS=$([Environment]::GetEnvironmentVariable('HIREDIS_HAPP_TEST_WITH_REDIS'))). Running unit tests only."
  Invoke-UnitCTestSuite -Configuration $Configuration
}

if ( $RUN_MODE -eq "msvc.modern.test" ) {
  Invoke-Environment "call ""$vsInstallationPath/VC/Auxiliary/Build/vcvars64.bat"""
  New-Item -Path "build_jobs_ci" -ItemType "directory" -Force 
  Set-Location "build_jobs_ci"
  & cmake ".." "-G" "$Env:CMAKE_GENERATOR" "-A" $Env:CMAKE_PLATFORM "-DBUILD_SHARED_LIBS=$Env:BUILD_SHARED_LIBS"  `
    "-DPROJECT_HIREDIS_HAPP_ENABLE_UNITTEST=ON" "-DPROJECT_HIREDIS_HAPP_ENABLE_SAMPLE=ON"                         `
    "-DCMAKE_SYSTEM_VERSION=$selectWinSDKVersion" "-DATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LOW_MEMORY_MODE=ON"
  Assert-LastNativeExitCode -StepName 'CMake configure (msvc.modern.test)'

  $CURRENT_CWD = Get-Location
  $ALL_DLL_FILES = @(Get-ChildItem -Path "$CURRENT_CWD/*.dll" -Recurse) + @(Get-ChildItem -Path "$PROJECT_DIR/third_party/install/*.dll" -Recurse)
  $ALL_DLL_DIRS = $(foreach ($dll_file in $ALL_DLL_FILES) {
      $dll_file.Directory.FullName
    }) | Sort-Object | Get-Unique
  $Env:PATH = ($ALL_DLL_DIRS -Join [IO.Path]::PathSeparator) + [IO.Path]::PathSeparator + $Env:PATH
  Write-Output "PATH=$Env:PATH"

  & cmake --build . --config $Env:CONFIGURATION
  Assert-LastNativeExitCode -StepName 'CMake build (msvc.modern.test)'
  Invoke-PlatformCTestSuite -Configuration $Env:CONFIGURATION
}
elseif ( $RUN_MODE -eq "msvc.2017.test" ) {
  Invoke-Environment "call ""$vsInstallationPath/VC/Auxiliary/Build/vcvars64.bat"""
  New-Item -Path "build_jobs_ci" -ItemType "directory" -Force 
  Set-Location "build_jobs_ci"
  & cmake ".." "-G" "$Env:CMAKE_GENERATOR" "-DBUILD_SHARED_LIBS=$ENV:BUILD_SHARED_LIBS"       `
    "-DPROJECT_HIREDIS_HAPP_ENABLE_UNITTEST=ON" "-DPROJECT_HIREDIS_HAPP_ENABLE_SAMPLE=ON"     `
    "-DCMAKE_SYSTEM_VERSION=$selectWinSDKVersion" "-DATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LOW_MEMORY_MODE=ON"
  Assert-LastNativeExitCode -StepName 'CMake configure (msvc.2017.test)'

  $CURRENT_CWD = Get-Location
  $ALL_DLL_FILES = @(Get-ChildItem -Path "$CURRENT_CWD/*.dll" -Recurse) + @(Get-ChildItem -Path "$PROJECT_DIR/third_party/install/*.dll" -Recurse)
  $ALL_DLL_DIRS = $(foreach ($dll_file in $ALL_DLL_FILES) {
      $dll_file.Directory.FullName
    }) | Sort-Object | Get-Unique
  $Env:PATH = ($ALL_DLL_DIRS -Join [IO.Path]::PathSeparator) + [IO.Path]::PathSeparator + $Env:PATH
  Write-Output "PATH=$Env:PATH"

  & cmake --build . --config $Env:CONFIGURATION
  Assert-LastNativeExitCode -StepName 'CMake build (msvc.2017.test)'
  Invoke-PlatformCTestSuite -Configuration $Env:CONFIGURATION
}

Set-Location $WORK_DIR
