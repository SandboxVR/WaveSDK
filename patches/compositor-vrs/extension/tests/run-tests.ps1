param([string]$JavaHome = $env:JAVA_HOME)
$ErrorActionPreference = 'Stop'
if (-not $JavaHome) { throw 'Set JAVA_HOME or pass -JavaHome to a JDK.' }
$javac = Join-Path $JavaHome 'bin/javac.exe'
$java = Join-Path $JavaHome 'bin/java.exe'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path ([System.IO.Path]::GetTempPath()) ('wave-vrs-api-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
$sources = @(Get-ChildItem -LiteralPath $PSScriptRoot -Recurse -Filter '*.java' | ForEach-Object FullName)
$source = Join-Path $root 'src/com/sandboxvr/wave/CompositorSettings.java'
& $javac -source 8 -target 8 -d $out $source $sources
if ($LASTEXITCODE -ne 0) { throw 'API test compilation failed.' }
& $java -cp $out 'com.sandboxvr.wave.CompositorSettingsTest'
if ($LASTEXITCODE -ne 0) { throw 'API tests failed.' }
