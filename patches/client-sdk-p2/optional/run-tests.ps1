param(
    [Parameter(Mandatory=$true)][string]$PatchedImeJar,
    [Parameter(Mandatory=$true)][string]$PatchedPermissionJar,
    [string]$UtilsJar = "$PSScriptRoot/../../client-sdk-p1/.build/utils-patched.jar",
    [string]$Jdk = 'C:/Program Files/Android/Android Studio/jbr/bin'
)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$taskOutput = Join-Path $taskRoot 'tests/build/classes'
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
$taskIme = (Resolve-Path -LiteralPath $PatchedImeJar).Path
$taskPermission = (Resolve-Path -LiteralPath $PatchedPermissionJar).Path
$taskUtils = (Resolve-Path -LiteralPath $UtilsJar).Path
$taskSources = Get-ChildItem -LiteralPath "$taskRoot/tests/src" -Recurse -Filter *.java | Select-Object -ExpandProperty FullName
& "$Jdk/javac.exe" --release 8 -Xlint:-options -cp "$taskIme;$taskPermission;$taskUtils" -d $taskOutput $taskSources
if ($LASTEXITCODE -ne 0) { throw 'Optional lifecycle host compilation failed' }
& "$Jdk/java.exe" -Xverify:all -cp "$taskOutput;$taskIme;$taskPermission;$taskUtils" OptionalLifecycleRegression
if ($LASTEXITCODE -ne 0) { throw 'Optional lifecycle regression failed' }
