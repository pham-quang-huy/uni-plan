[CmdletBinding()]
param(
    [switch]$Tests,
    [switch]$Clean
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
$BuildDir = Join-Path $ScriptDir "Build\CMakeWin"
$Preset = "dev-win"
$RunTests = $false

if ($Tests) {
    $Preset = "dev-win-tests"
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

Write-Host ""
Write-Host "Verifying build binary..."
Invoke-Checked $Binary --version

if ($RunTests) {
    Write-Host ""
    Write-Host "Running tests..."
    Invoke-Checked (Join-Path $BuildDir "uni-plan-tests.exe")
}

Write-Host "Run: $Binary --version"
