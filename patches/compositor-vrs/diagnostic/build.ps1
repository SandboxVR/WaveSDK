param([switch]$NativeOnly)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$taskJdk = 'C:/Program Files/Android/Android Studio/jbr'
$taskSdk = Join-Path $env:LOCALAPPDATA 'Android/Sdk'
if (-not (Test-Path -LiteralPath "$taskJdk/bin/java.exe")) { throw "JDK missing: $taskJdk" }
if (-not (Test-Path -LiteralPath "$taskSdk/ndk/25.1.8937393")) { throw 'NDK 25.1.8937393 missing' }
$env:JAVA_HOME = $taskJdk
$env:ANDROID_HOME = $taskSdk
$taskKey = Join-Path $taskRoot '.local/vrs-diagnostic-debug.keystore'
if (-not (Test-Path -LiteralPath $taskKey)) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $taskKey) | Out-Null
    & "$taskJdk/bin/keytool.exe" -genkeypair -keystore $taskKey -storepass android -keypass android -alias vrs-diagnostic -dname 'CN=Wave VRS Diagnostic, O=Sandbox VR Diagnostic' -keyalg RSA -keysize 2048 -validity 3650
    if ($LASTEXITCODE -ne 0) { throw 'Diagnostic signing key generation failed' }
}
Push-Location -LiteralPath $taskRoot
try {
    $taskTarget = if ($NativeOnly) { ':app:externalNativeBuildDebug' } else { ':app:assembleDebug' }
    & "$taskRoot/gradlew.bat" --offline --no-daemon $taskTarget
    if ($LASTEXITCODE -ne 0) { throw "Gradle failed: $LASTEXITCODE" }
    if (-not $NativeOnly) {
        Get-FileHash -Algorithm SHA256 -LiteralPath "$taskRoot/app/build/outputs/apk/debug/app-debug.apk"
        Get-Content -LiteralPath "$taskRoot/app/build/sdk-identity.txt"
    }
} finally {
    Pop-Location
}
