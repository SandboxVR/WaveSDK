param(
    [string]$PatchedUtilsJar = '',
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
if ($LASTEXITCODE -ne 0) { throw 'Donor compilation failed' }
$taskTestSources = Get-ChildItem -LiteralPath "$taskRoot/tests/src" -Filter *.java -Recurse | Select-Object -ExpandProperty FullName
& "$Jdk/javac.exe" --release 8 -Xlint:-options -cp $taskDonor -d $taskTests $taskTestSources
if ($LASTEXITCODE -ne 0) { throw 'Host test compilation failed' }
$taskClasspath = "$taskTests;$taskDonor"
if ($PatchedUtilsJar) {
    $taskResolvedJar = (Resolve-Path -LiteralPath $PatchedUtilsJar).Path
    $taskClasspath = "$taskTests;$taskResolvedJar;$taskDonor"
}
& "$Jdk/java.exe" -cp $taskClasspath DispatchRegression
if ($LASTEXITCODE -ne 0) { throw 'Dispatch regression failed' }
& "$Jdk/java.exe" -cp $taskClasspath com.htc.vr.sdk.overlay.OverlayRegression
if ($LASTEXITCODE -ne 0) { throw 'Overlay regression failed' }
