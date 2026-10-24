param(
    [string]$Jdk = 'C:/Program Files/Android/Android Studio/jbr',
    [string]$AndroidSdk = 'C:/Users/kevin.mok/AppData/Local/Android/Sdk'
)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$outputRoot = [IO.Path]::GetFullPath((Join-Path $taskRoot 'out/donor'))
$classes = [IO.Path]::GetFullPath((Join-Path $outputRoot 'classes'))
$android = Join-Path $AndroidSdk 'platforms/android-36.1/android.jar'
$d8 = Join-Path $AndroidSdk 'build-tools/36.1.0/lib/d8.jar'
foreach ($required in @((Join-Path $Jdk 'bin/javac.exe'), $android, $d8)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing $required" }
}
New-Item -ItemType Directory -Path $classes -Force | Out-Null
if (-not $classes.StartsWith($outputRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Generated class directory escaped donor output'
}
Get-ChildItem -LiteralPath $classes -Recurse -File -Filter '*.class' | ForEach-Object {
    Remove-Item -LiteralPath $_.FullName
}
$sources = @(& rg --files (Join-Path $taskRoot 'src') (Join-Path $taskRoot 'compile-stubs') -g '*.java')
if ($LASTEXITCODE -ne 0) { throw 'Donor source discovery failed' }
$argumentFile = Join-Path $outputRoot 'sources.txt'
$quotedSources = @($sources | ForEach-Object { '"' + $_.Replace('\', '/') + '"' })
[IO.File]::WriteAllLines($argumentFile, [string[]]$quotedSources, [Text.UTF8Encoding]::new($false))
& (Join-Path $Jdk 'bin/javac.exe') --release 8 -encoding UTF-8 -cp $android -d $classes ('@' + $argumentFile)
if ($LASTEXITCODE -ne 0) { throw 'Donor compilation failed' }
$donorJar = Join-Path $outputRoot 'donor.jar'
& (Join-Path $Jdk 'bin/jar.exe') --create --file $donorJar -C $classes .
if ($LASTEXITCODE -ne 0) { throw 'Donor JAR creation failed' }
$dex = Join-Path $outputRoot 'dex'
New-Item -ItemType Directory -Path $dex -Force | Out-Null
& (Join-Path $Jdk 'bin/java.exe') -cp $d8 com.android.tools.r8.D8 --release --min-api 29 --lib $android --output $dex $donorJar
if ($LASTEXITCODE -ne 0) { throw 'Donor D8 compilation failed' }
Write-Output (Join-Path $dex 'classes.dex')
