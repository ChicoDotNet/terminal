#Requires -Version 7

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'AuditMode')]
    [string]$Configuration = 'Debug',

    [ValidateSet('x64', 'Win32', 'ARM64')]
    [string]$Platform = 'x64',

    [switch]$SkipSubmodules
)

$ErrorActionPreference = 'Stop'

function Set-R09MsbuildDevEnvironment
{
    [CmdletBinding()]
    param()

    try {
        Set-MsbuildDevEnvironment
        return
    }
    catch {
        if ($_.Exception.Message -notmatch 'VSSetup|Find-Package|Find-Module') {
            throw
        }

        Write-Warning 'VSSetup could not be resolved; falling back to the installed Visual Studio DevShell.'
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "VSSetup is unavailable and vswhere was not found at '$vswhere'."
    }

    $installationPath = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($installationPath)) {
        throw 'Unable to locate a Visual Studio installation with the VC++ x64 toolset.'
    }

    switch ($env:PROCESSOR_ARCHITECTURE.ToLowerInvariant()) {
        'amd64' { $arch = 'x64' }
        'x86' { $arch = 'x86' }
        'arm64' { $arch = 'arm64' }
        default { throw "Unknown architecture: $($env:PROCESSOR_ARCHITECTURE)" }
    }

    $devShellModule = Join-Path $installationPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
    if (-not (Test-Path $devShellModule)) {
        throw "Visual Studio DevShell module was not found at '$devShellModule'."
    }

    Import-Module -Global -Name $devShellModule
    Enter-VsDevShell -VsInstallPath $installationPath -SkipAutomaticLocation -DevCmdArguments "-arch=$arch" | Out-Null
    Set-Item -Force -Path 'Env:\Platform' -Value $arch

    Write-Host "Dev environment variables set from $installationPath" -ForegroundColor Green
}

function Get-R09RustNativeStaticLibraries
{
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$Root,

        [Parameter(Mandatory)]
        [string]$TargetTriple,

        [Parameter(Mandatory)]
        [string]$CargoProfile
    )

    $manifest = Join-Path $Root 'rust\terminal-parser-ffi\Cargo.toml'
    $targetDir = Join-Path $Root 'artifacts\cargo-target'
    $cargoArgs = @(
        'rustc',
        '--manifest-path', $manifest,
        '--target', $TargetTriple,
        '--target-dir', $targetDir
    )
    if ($CargoProfile -eq 'release') {
        $cargoArgs += '--release'
    }
    $cargoArgs += @('--', '--print', 'native-static-libs')

    Write-Host 'Querying rustc for the native libraries required by the parser staticlib.'
    $nativeStaticOutput = @(& cargo.exe @cargoArgs 2>&1)
    $cargoExitCode = $LASTEXITCODE
    $nativeStaticOutput | ForEach-Object { Write-Host $_ }
    if ($cargoExitCode -ne 0) {
        throw "Unable to query Rust native static libraries; cargo exited with code $cargoExitCode"
    }

    $nativeStaticMatch = $nativeStaticOutput |
        Select-String -Pattern 'native-static-libs:\s*(?<libraries>.+)$' |
        Select-Object -Last 1
    if (-not $nativeStaticMatch) {
        throw 'rustc did not report native-static-libs for terminal-parser-ffi.'
    }

    $libraries = $nativeStaticMatch.Matches[0].Groups['libraries'].Value -split '\s+' | Where-Object { $_ }
    if (-not $libraries) {
        throw 'rustc reported an empty native-static-libs set for terminal-parser-ffi.'
    }

    return $libraries
}

function Invoke-R09ControlCharacterAbiReplay
{
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$Root,

        [Parameter(Mandatory)]
        [string]$DiagnosticsDir,

        [Parameter(Mandatory)]
        [string]$Configuration,

        [Parameter(Mandatory)]
        [string]$Platform
    )

    $targetTriple = switch ($Platform) {
        'x64' { 'x86_64-pc-windows-msvc' }
        'Win32' { 'i686-pc-windows-msvc' }
        'ARM64' { 'aarch64-pc-windows-msvc' }
        default { throw "Unsupported R09 parser ABI probe platform '$Platform'." }
    }
    $cargoProfile = if ($Configuration -eq 'Debug') { 'debug' } else { 'release' }

    $probeSource = Join-Path $Root 'tools\rust\R09ControlCharacterAbiProbe.cpp'
    $ffiInclude = Join-Path $Root 'rust\terminal-parser-ffi\include'
    $ffiLib = Join-Path $Root "artifacts\cargo-target\$targetTriple\$cargoProfile\terminal_parser_ffi.lib"
    $probeExe = Join-Path $DiagnosticsDir 'r09-control-character-abi-probe.exe'
    $probeObj = Join-Path $DiagnosticsDir 'r09-control-character-abi-probe.obj'

    if (-not (Test-Path $ffiLib)) {
        throw "Rust parser FFI library was not produced at '$ffiLib'."
    }

    $nativeStaticLibraries = Get-R09RustNativeStaticLibraries -Root $Root -TargetTriple $targetTriple -CargoProfile $cargoProfile

    Remove-Item $probeExe, $probeObj -Force -ErrorAction SilentlyContinue

    Write-Host 'Compiling and linking the R09 control-character C ABI replay probe.'
    & cl.exe /nologo /EHsc /std:c++20 "/I$ffiInclude" /c $probeSource "/Fo:$probeObj"
    if ($LASTEXITCODE -ne 0) {
        throw "R09 control-character ABI probe compilation failed with exit code $LASTEXITCODE"
    }

    & cl.exe /nologo $probeObj $ffiLib $nativeStaticLibraries "/Fe:$probeExe"
    if ($LASTEXITCODE -ne 0) {
        throw "R09 control-character ABI probe link failed with exit code $LASTEXITCODE"
    }

    & $probeExe
    if ($LASTEXITCODE -ne 0) {
        throw "R09 control-character ABI replay failed with exit code $LASTEXITCODE"
    }
}

$root = (git rev-parse --show-toplevel 2>$null)
if (-not $root) {
    throw 'Run this script from inside the WindowsRustTerminal checkout.'
}

Push-Location $root
try {
    if (-not $SkipSubmodules) {
        git submodule update --init --recursive
        if ($LASTEXITCODE -ne 0) {
            throw "git submodule update failed with exit code $LASTEXITCODE"
        }
    }

    Import-Module (Join-Path $root 'tools\OpenConsole.psm1') -Force
    Set-R09MsbuildDevEnvironment

    $diagnosticsDir = Join-Path $root 'artifacts'
    New-Item -ItemType Directory -Path $diagnosticsDir -Force | Out-Null
    $textLog = Join-Path $diagnosticsDir 'r09-product-build.log'
    $binaryLog = Join-Path $diagnosticsDir 'r09-product-build.binlog'
    Remove-Item $textLog, $binaryLog -Force -ErrorAction SilentlyContinue

    $msbuildArgs = @(
        "/p:Configuration=$Configuration",
        "/p:Platform=$Platform",
        '/p:AppxSymbolPackageEnabled=false',
        '/t:Terminal\CascadiaPackage',
        '/m',
        '/fl',
        "/flp:logfile=$textLog;verbosity=normal",
        "/bl:$binaryLog"
    )

    Write-Host "Building the canonical Terminal product path: Terminal\CascadiaPackage ($Platform $Configuration)"
    Write-Host "MSBuild diagnostics: $textLog"
    Write-Host "MSBuild binary log: $binaryLog"
    Invoke-OpenConsoleBuild @msbuildArgs
    $buildExitCode = $LASTEXITCODE

    if ($buildExitCode -ne 0) {
        if (Test-Path $textLog) {
            $diagnostics = Select-String -Path $textLog -Pattern '(?i)\b(error (?:C|LNK|MSB|NETSDK|NU|APPX|WMC|XLS|PRI)\d+|fatal error [A-Z]+\d+|: error )' | Select-Object -First 40
            if ($diagnostics) {
                Write-Host 'R09 first actionable MSBuild diagnostics:' -ForegroundColor Red
                foreach ($diagnostic in $diagnostics) {
                    Write-Host $diagnostic.Line
                }
            }
            else {
                Write-Warning "The product build failed, but no standard compiler/linker/MSBuild diagnostic was matched in '$textLog'."
            }
        }
        else {
            Write-Warning "The product build failed before the MSBuild text log was created at '$textLog'."
        }

        throw "Terminal product build failed with exit code $buildExitCode"
    }

    Invoke-R09ControlCharacterAbiReplay -Root $root -DiagnosticsDir $diagnosticsDir -Configuration $Configuration -Platform $Platform
    Write-Host 'Canonical Terminal product build completed successfully.'
}
finally {
    Pop-Location
}
