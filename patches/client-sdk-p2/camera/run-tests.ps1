param(
    [string]$Jdk = 'C:/Program Files/Android/Android Studio/jbr/bin',
    [string]$AndroidJar = 'C:/Users/kevin.mok/AppData/Local/Android/Sdk/platforms/android-36.1/android.jar'
)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$taskDonor = Join-Path $taskRoot 'build/donor-classes'
$taskTests = Join-Path $taskRoot 'build/test-classes'
New-Item -ItemType Directory -Force -Path $taskDonor, $taskTests | Out-Null
$taskSources = Get-ChildItem -LiteralPath "$taskRoot/src", "$taskRoot/compile-stubs" -Filter *.java -Recurse | Select-Object -ExpandProperty FullName
& "$Jdk/javac.exe" --release 8 -Xlint:-options -cp $AndroidJar -d $taskDonor $taskSources
if ($LASTEXITCODE -ne 0) { throw 'Camera donor compilation failed' }
$taskTestSources = Get-ChildItem -LiteralPath "$taskRoot/tests/src" -Filter *.java -Recurse | Select-Object -ExpandProperty FullName
& "$Jdk/javac.exe" --release 8 -Xlint:-options -cp "$taskDonor;$AndroidJar" -d $taskTests $taskTestSources
if ($LASTEXITCODE -ne 0) { throw 'Camera host test compilation failed' }
& "$Jdk/java.exe" -cp "$taskTests;$taskDonor" com.htc.vr.sdk.CameraRegression
if ($LASTEXITCODE -ne 0) { throw 'Camera regression failed' }
