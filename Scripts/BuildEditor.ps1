# Reliable local build for Battlemap_TestCursorEditor (avoids Intermediate file locks).
param(
    [switch]$CleanIntermediate,
    [switch]$AllowEditorRunning,
    [switch]$KillVisualStudio
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$UProject = Join-Path $ProjectRoot "Battlemap_TestCursor.uproject"
$BuildBat = "F:\UE_5.5\Engine\Build\BatchFiles\Build.bat"
$BuildDir = Join-Path $ProjectRoot "Intermediate\Build"
$BuildRulesDir = Join-Path $BuildDir "BuildRules"

function Stop-LockingProcesses {
    if (-not $AllowEditorRunning) {
        $Editor = Get-Process -Name "UnrealEditor" -ErrorAction SilentlyContinue
        if ($Editor) {
            Write-Host "Closing Unreal Editor (DLL link requires editor to be closed)..."
            $Editor | Stop-Process -Force
            Start-Sleep -Seconds 2
        }
    }

    if ($KillVisualStudio) {
        Get-Process -Name "devenv" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 2
    } elseif (Get-Process -Name "devenv" -ErrorAction SilentlyContinue) {
        Write-Host ""
        Write-Host "ERROR: Visual Studio is running and often locks Intermediate\Build\BuildRules\*.pdb"
        Write-Host "  Close Visual Studio, OR rerun with:  .\Scripts\BuildEditor.ps1 -KillVisualStudio"
        Write-Host ""
        exit 1
    }

    $Names = @(
        "UnrealBuildTool", "LiveCodingConsole", "ShaderCompileWorker",
        "MSBuild", "VBCSCompiler", "ServiceHub.Host.dotnet.x64", "ServiceHub.RoslynCodeAnalysisService"
    )
    foreach ($N in $Names) {
        Get-Process -Name $N -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Seconds 1
}

function Clear-BuildArtifacts {
    if ($CleanIntermediate) {
        Write-Host "Removing Intermediate/Build..."
        Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $BuildDir
        return
    }

    Write-Host "Clearing UBT caches (BuildRules, Makefile, SourceFileCache)..."
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $BuildRulesDir
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue (Join-Path $BuildDir "Win64")

    $LooseFiles = @(
        (Join-Path $BuildDir "SourceFileCache.bin"),
        (Join-Path $ProjectRoot "Binaries\Win64\UnrealEditor-Battlemap_TestCursor.dll"),
        (Join-Path $ProjectRoot "Binaries\Win64\UnrealEditor-Battlemap_TestCursor.pdb"),
        (Join-Path $ProjectRoot "Binaries\Win64\UnrealEditor-Battlemap_TestCursor.exp")
    )
    foreach ($F in $LooseFiles) {
        Remove-Item -Force -ErrorAction SilentlyContinue $F
    }
}

function Invoke-EditorBuild {
    & $BuildBat Battlemap_TestCursorEditor Win64 Development `
        -Project="$UProject" `
        -WaitMutex `
        -NoUBA `
        -NoUBALocal `
        -DisableAdaptiveUnity 2>&1 | ForEach-Object { Write-Host $_ }
    if ($null -eq $LASTEXITCODE) {
        return 1
    }
    return [int]$LASTEXITCODE
}

Stop-LockingProcesses
Clear-BuildArtifacts

$MaxAttempts = 3
for ($Attempt = 1; $Attempt -le $MaxAttempts; $Attempt++) {
    if ($Attempt -gt 1) {
        Write-Host "Retry $Attempt/$MaxAttempts after lock failure..."
        Stop-LockingProcesses
        Clear-BuildArtifacts
        Start-Sleep -Seconds 2
    }

    Write-Host "Building Battlemap_TestCursorEditor (attempt $Attempt/$MaxAttempts)..."
    $Code = Invoke-EditorBuild
    if ($Code -eq 0) {
        Write-Host "Build succeeded."
        exit 0
    }

    $LogText = Get-Content "C:\Users\wills\AppData\Local\UnrealBuildTool\Log.txt" -Raw -ErrorAction SilentlyContinue
    $IsLock = $LogText -match "user-mapped section open|LNK1104|ModuleRules\.pdb"
    if (-not $IsLock -or $Attempt -eq $MaxAttempts) {
        Write-Error "Build failed (exit $Code). See Docs/BuildTroubleshooting.md and %LOCALAPPDATA%\UnrealBuildTool\Log.txt"
    }
}

Write-Error "Build failed after $MaxAttempts attempts (file locks). Close VS + UE Editor, then: .\Scripts\BuildEditor.ps1 -KillVisualStudio"
