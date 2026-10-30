# Flash ARIA: asks the board what chip it is, builds for that, and uploads.
#
#   scripts\flash.ps1 -Backup                  save her factory firmware first, once
#   scripts\flash.ps1 -Sketch aria_pin_finder  the pin finder: PROPELLERS OFF
#   scripts\flash.ps1                          her flight firmware
param(
    [string]$Port = "",
    [string]$Sketch = "aria_flight",
    [switch]$Backup
)
$ErrorActionPreference = "Stop"
$env:ARDUINO_DIRECTORIES_USER = "$env:LOCALAPPDATA\Arduino15\user"
$cli = "$env:LOCALAPPDATA\arduino-cli\arduino-cli.exe"
$esptool = (Get-ChildItem "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py\*\esptool.exe" | Select-Object -Last 1).FullName

if (-not $Port) {
    $ports = [System.IO.Ports.SerialPort]::GetPortNames()
    if ($ports.Count -ne 1) { throw "Say which port: -Port COM5 (found: $($ports -join ', '))" }
    $Port = $ports[0]
}

$info = & $esptool --port $Port chip-id 2>&1 | Out-String
Write-Output $info
if ($info -match "ESP32-S3") { $chip = "esp32s3" }
elseif ($info -match "ESP32-D0|ESP32-U4|ESP32-PICO|Chip is ESP32\b|Chip type:\s+ESP32\b") { $chip = "esp32" }
else { throw "Not a chip I know: nothing written" }

if ($Backup) {
    $file = "$PSScriptRoot\..\factory_firmware_$chip.bin"
    if (Test-Path $file) { throw "$file is already there: not overwriting her factory firmware" }
    & $esptool --port $Port read-flash 0 ALL $file
    Write-Output "Saved $file"
    return
}

# An S3 on its own USB port needs USB CDC for serial; one behind a CH340 doesn't
$usb = (Get-CimInstance Win32_SerialPort | Where-Object { $_.DeviceID -eq $Port }).PNPDeviceID
$cdc = if ($chip -eq "esp32s3" -and $usb -match "VID_303A") { ",CDCOnBoot=cdc" } else { "" }
$fqbn = "esp32:esp32:${chip}:PartitionScheme=huge_app$cdc"

python "$PSScriptRoot\embed_web.py" $Sketch
& $cli compile --fqbn $fqbn --upload --port $Port "$PSScriptRoot\$Sketch"
