param([string]$Jdk = 'C:/Program Files/Android/Android Studio/jbr')
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$outputRoot = Join-Path $taskRoot 'out/host-classes'
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$sourceFiles = @(& rg --files (Join-Path $taskRoot 'src') (Join-Path $taskRoot 'compile-stubs') (Join-Path $taskRoot 'tests') -g '*.java')
if ($LASTEXITCODE -ne 0) { throw 'Java source discovery failed' }
$argumentFile = Join-Path $taskRoot 'out/host-sources.txt'
$quotedSources = @($sourceFiles | ForEach-Object { '"' + $_.Replace('\', '/') + '"' })
[IO.File]::WriteAllLines($argumentFile, [string[]]$quotedSources, [Text.UTF8Encoding]::new($false))
& (Join-Path $Jdk 'bin/javac.exe') --release 11 -encoding UTF-8 -d $outputRoot ('@' + $argumentFile)
if ($LASTEXITCODE -ne 0) { throw 'Host compilation failed' }
& (Join-Path $Jdk 'bin/java.exe') -ea -cp $outputRoot com.htc.vr.client.NotifyLifecycleTests
if ($LASTEXITCODE -ne 0) { throw 'Host tests failed' }
