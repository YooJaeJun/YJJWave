# 에디터 타깃을 빌드한다 (에디터를 끈 상태에서). powershell -File Tools/Build.ps1
param(
    [string]$EngineDir = "D:\source\repos\UnrealEngine\Engine"
)

$ProjectDir = Resolve-Path (Join-Path $PSScriptRoot "..")
$Project = Join-Path $ProjectDir "YJJWave.uproject"
$BuildLog = Join-Path $ProjectDir "Saved\Logs\Build.log"
New-Item -ItemType Directory -Force (Split-Path $BuildLog) | Out-Null

& (Join-Path $EngineDir "Build\BatchFiles\Build.bat") YJJWaveEditor Win64 Development "-project=$Project" -WaitMutex *> $BuildLog
$BuildExit = $LASTEXITCODE
Select-String -Path $BuildLog -Encoding UTF8 -Pattern "error |error:|warning C|Result:|Total execution" | Select-Object -First 40 | ForEach-Object { $_.Line }
exit $BuildExit
