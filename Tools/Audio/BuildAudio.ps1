# 소리(BGM·효과음·보이스)를 합성하고 언리얼 사운드 에셋으로 임포트한다. 에디터를 끈 상태에서.
#   powershell -File Tools/Audio/BuildAudio.ps1 [-SkipImport]
param(
    [string]$EngineDir = "D:\source\repos\UnrealEngine\Engine",
    [switch]$SkipImport
)

$ErrorActionPreference = "Stop"
$ProjectDir = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$Project = Join-Path $ProjectDir "YJJWave.uproject"
$Editor = Join-Path $EngineDir "Binaries\Win64\UnrealEditor-Cmd.exe"

python (Join-Path $PSScriptRoot "generate_audio.py") (Join-Path $PSScriptRoot "Generated")
if ($LASTEXITCODE -ne 0) { throw "generate_audio.py failed" }
if ($SkipImport) { exit 0 }

$Script = (Join-Path $PSScriptRoot "import_audio.py").Replace('\', '/')
$Log = Join-Path $ProjectDir "Saved\Logs\ImportAudio.log"
& $Editor $Project -run=pythonscript -script="$Script" -unattended -nosplash -nop4 -nullrhi -stdout -FullStdOutLogOutput *> $Log
$Code = $LASTEXITCODE
Select-String -Path $Log -Pattern "\[YJJAudio\] done|Error" | Select-Object -First 10 | ForEach-Object { $_.Line }
if ($Code -ne 0) { throw "import_audio.py failed ($Code). Log: $Log" }
