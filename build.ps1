[CmdletBinding()]
param(
    [switch]$Tests,
    [switch]$Clean,
    [switch]$NoInstall
)

$ErrorActionPreference = "Stop"

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ScriptDir "Build\CMake"
$Preset = "dev"
$RunTests = $false

if ($Tests) {
    $Preset = "dev-tests"
    $RunTests = $true
}

Write-Host "Configuring..."
Push-Location $ScriptDir
try {
    Invoke-Checked cmake --preset $Preset
}
finally {
    Pop-Location
}

Write-Host "Building..."
$BuildArgs = @("--build", $BuildDir, "--parallel",
    [Environment]::ProcessorCount.ToString())
if ($Clean) {
    $BuildArgs += "--clean-first"
}
Invoke-Checked cmake @BuildArgs

$Binary = Join-Path $BuildDir "uni-plan.exe"
if (!(Test-Path $Binary)) {
    throw "Expected binary was not produced: $Binary"
}

Write-Host ""
Write-Host "Built: $Binary"

if (!$NoInstall) {
    $InstallBinary = Join-Path $HOME "bin\uni-plan.exe"
    if (Test-Path $InstallBinary) {
        $Item = Get-Item $InstallBinary -Force
        if (($Item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            Remove-Item $InstallBinary -Force
        }
    }
    Invoke-Checked cmake --install $BuildDir --prefix $HOME --component runtime
    Write-Host "Installed: $InstallBinary"
}

if ($RunTests) {
    Write-Host ""
    Write-Host "Running tests..."
    Invoke-Checked (Join-Path $BuildDir "uni-plan-tests.exe")
}

Write-Host "Run: uni-plan.exe --version"
