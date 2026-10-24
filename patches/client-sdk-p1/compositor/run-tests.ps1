param([string]$Jdk = 'C:/Program Files/Android/Android Studio/jbr')
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$outputRoot = Join-Path $taskRoot 'out/host-classes'
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$sourceFiles = @(& rg --files (Join-Path $taskRoot 'src') (Join-Path $taskRoot 'donor') (Join-Path $taskRoot 'tests') -g '*.java')
if ($LASTEXITCODE -ne 0) { throw 'Java source discovery failed' }
$argumentFile = Join-Path $taskRoot 'out/host-sources.txt'
$sourceFiles | ForEach-Object { '"' + $_.Replace('\', '/') + '"' } | Set-Content -LiteralPath $argumentFile -Encoding utf8
& (Join-Path $Jdk 'bin/javac.exe') --release 11 -encoding UTF-8 -d $outputRoot ('@' + $argumentFile)
if ($LASTEXITCODE -ne 0) { throw 'Host compilation failed' }
& (Join-Path $Jdk 'bin/java.exe') -ea -cp $outputRoot com.htc.vr.core.CompositorTests
if ($LASTEXITCODE -ne 0) { throw 'Host tests failed' }
