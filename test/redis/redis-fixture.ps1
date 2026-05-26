param(
  [Parameter(Position = 0)]
  [string]$Command = "help"
)

$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent (Split-Path -Parent $scriptDir)
$bashScript = Join-Path $scriptDir 'redis-fixture.sh'

function Show-Usage {
  @(
    'Usage: .\test\redis\redis-fixture.ps1 <command>',
    '',
    'Commands: download, build, prepare, start-single, stop-single, restart-single,',
    '          start-cluster, stop-cluster, restart-cluster, start-all, stop-all,',
    '          cleanup, print-env, status',
    '',
    'This wrapper delegates to WSL because official Redis OSS server binaries are not provided for native Windows.',
    'Set HIREDIS_HAPP_TEST_WSL_DISTRO when you want to target a specific installed distro.'
  ) | Write-Host
}

if (-not (Get-Command wsl -ErrorAction SilentlyContinue)) {
  if ($Command -in @('', 'help', '-h', '--help')) {
    Show-Usage
    Write-Host ''
    Write-Host 'WSL is required to run this fixture on Windows. Install WSL and a Linux distribution, then retry.'
    exit 0
  }

  throw 'WSL is required to run the Redis fixture on Windows. Install WSL and a Linux distribution, then retry.'
}

$wslDistributions = @(& wsl --list --quiet 2>$null | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
if ($LASTEXITCODE -ne 0 -or $wslDistributions.Count -eq 0) {
  if ($Command -in @('', 'help', '-h', '--help')) {
    Show-Usage
    Write-Host ''
    Write-Host 'No WSL Linux distribution is installed yet. Run `wsl --list --online` and `wsl --install <Distro>` first.'
    exit 0
  }

  throw 'No WSL Linux distribution is installed yet. Run `wsl --list --online` and `wsl --install <Distro>` first.'
}

$normalizedDistributions = @($wslDistributions | ForEach-Object { $_.Trim() } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
$requestedDistro = [Environment]::GetEnvironmentVariable('HIREDIS_HAPP_TEST_WSL_DISTRO')
if (-not [string]::IsNullOrWhiteSpace($requestedDistro)) {
  $targetDistro = @($normalizedDistributions | Where-Object { $_ -eq $requestedDistro } | Select-Object -First 1)
  if ($targetDistro.Count -eq 0) {
    throw "Requested WSL distro '$requestedDistro' is not installed. Installed distros: $($normalizedDistributions -join ', ')"
  }

  $targetDistro = $targetDistro[0]
}
else {
  $targetDistro = $normalizedDistributions[0]
}

function Convert-ToWslPath {
  param([Parameter(Mandatory = $true)][string]$WindowsPath)

  if ($WindowsPath -match '^(?<drive>[A-Za-z]):[\\/](?<rest>.*)$') {
    $drive = $Matches['drive'].ToLowerInvariant()
    $rest = ($Matches['rest'] -replace '\\', '/').TrimStart('/')
    return "/mnt/$drive/$rest"
  }

  $normalized = $WindowsPath -replace '\\', '/'
  $escapedPath = $normalized.Replace("'", "'\''")
  $wslPath = & wsl bash -lc "wslpath -a '$escapedPath'"
  if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($wslPath)) {
    throw "Failed to convert Windows path to WSL path: $WindowsPath"
  }

  return $wslPath.Trim()
}

function Convert-EnvValueForWsl {
  param([Parameter(Mandatory = $true)][string]$Name)

  $envValue = [Environment]::GetEnvironmentVariable($Name)
  if ([string]::IsNullOrWhiteSpace($envValue)) {
    return $null
  }

  if ($Name -eq 'HIREDIS_HAPP_TEST_REDIS_ROOT' -and $envValue -match '^[A-Za-z]:\\') {
    return Convert-ToWslPath -WindowsPath $envValue
  }

  return $envValue
}

$wslRepoRoot = Convert-ToWslPath -WindowsPath $repoRoot
$wslBashScript = Convert-ToWslPath -WindowsPath $bashScript

$forwardEnvNames = @(
  'HIREDIS_HAPP_TEST_REDIS_ROOT',
  'HIREDIS_HAPP_TEST_REDIS_DOWNLOAD_URL',
  'HIREDIS_HAPP_TEST_REDIS_BUILD_JOBS',
  'HIREDIS_HAPP_TEST_SINGLE_HOST',
  'HIREDIS_HAPP_TEST_SINGLE_PORT',
  'HIREDIS_HAPP_TEST_CLUSTER_HOST',
  'HIREDIS_HAPP_TEST_CLUSTER_PORT',
  'HIREDIS_HAPP_TEST_CLUSTER_BASE_PORT',
  'HIREDIS_HAPP_TEST_CLUSTER_NODE_COUNT',
  'HIREDIS_HAPP_TEST_CLUSTER_REPLICAS'
)

$envAssignments = foreach ($envName in $forwardEnvNames) {
  $envValue = Convert-EnvValueForWsl -Name $envName
  if (-not [string]::IsNullOrWhiteSpace($envValue)) {
    $escapedValue = $envValue.Replace("'", "'\''")
    "$envName='$escapedValue'"
  }
}

$commandValue = $Command.Replace("'", "'\''")
$envPrefix = if ($envAssignments.Count -gt 0) { ($envAssignments -join ' ') + ' ' } else { '' }
$linuxCommand = "cd '$wslRepoRoot' && ${envPrefix}bash '$wslBashScript' '$commandValue'"

& wsl --distribution $targetDistro --user root -- bash -lc $linuxCommand
$commandExitCode = $LASTEXITCODE

if ($Command -in @('stop-single', 'stop-cluster', 'stop-all', 'cleanup')) {
  & wsl --terminate $targetDistro 2>$null
  $LASTEXITCODE = $commandExitCode
}

if ($commandExitCode -ne 0) {
  exit $commandExitCode
}
