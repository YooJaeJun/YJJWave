# 게임을 창 모드로 띄워 오토파일럿(또는 벤치마크)을 돌리고, 로그 요약과 스크린샷을 남긴다.
#   powershell -File Tools/RunGame.ps1                      # 봇이 90초(게임 시간) 플레이
#   powershell -File Tools/RunGame.ps1 -Bench 5000 -Time 30 # 편마다 5000마리 깔고 30초 맞붙임 (성능 측정)
# 스크린샷: Saved/Screenshots/Play_*.png, 로그: Saved/Logs/RunGame.log
param(
    [string]$EngineDir = "D:\source\repos\UnrealEngine\Engine",
    [int]$Bench = 0,
    [double]$Time = 90,
    [double]$Speed = 1,
    [double]$ShotInterval = 15,
    [int]$ResX = 1920,
    [int]$ResY = 1080,
    [string]$Lang = "",
    [string]$Tag = "",
    # 추가 콘솔 명령 (쉼표로). 예: -Exec "stat RHI, stat unit"
    [string]$Exec = ""
)

$ProjectDir = Resolve-Path (Join-Path $PSScriptRoot "..")
$Project = Join-Path $ProjectDir "YJJWave.uproject"
$Editor = Join-Path $EngineDir "Binaries\Win64\UnrealEditor.exe"
$Log = Join-Path $ProjectDir "Saved\Logs\RunGame$Tag.log"

Remove-Item (Join-Path $ProjectDir "Saved\Screenshots\Play_*.png") -ErrorAction SilentlyContinue

$AllExec = "yjj.AutoPlay 1, yjj.AutoPlayTime $Time, yjj.AutoPlaySpeed $Speed, yjj.ShotInterval $ShotInterval, yjj.Bench $Bench, t.MaxFPS 0, r.VSync 0"
if ($Exec) { $AllExec += ", $Exec" }
$GameArgs = @(
    "`"$Project`"", "-game", "-windowed", "-ResX=$ResX", "-ResY=$ResY", "-WinX=0", "-WinY=0", "-nosplash", "-unattended", "-NoSound",
    "-ExecCmds=`"$AllExec`"", "-log", "-abslog=`"$Log`""
)
if ($Lang) { $GameArgs += "-yjjlang=$Lang" }

$Process = Start-Process -FilePath $Editor -ArgumentList $GameArgs -PassThru
$Limit = [int](($Time / [Math]::Max($Speed, 0.1)) * 3 + 180)
if (-not $Process.WaitForExit($Limit * 1000)) {
    $Process.Kill()
    Write-Host "Timed out"
}

Select-String -Path $Log -Encoding UTF8 -Pattern "\[AutoPilot\]|\[Bench\]|\[Run\] 시작|\[Data\]|\[Art\]|Error:|Ensure condition failed|Fatal|\[Battle\] 성벽" | ForEach-Object { $_.Line }
Get-ChildItem (Join-Path $ProjectDir "Saved\Screenshots\Play_*.png") -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName }
