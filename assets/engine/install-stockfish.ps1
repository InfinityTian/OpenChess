param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$work = $null
try {
    if (Test-Path (Join-Path $Destination 'stockfish.exe')) { exit 0 }
    $arch = $env:PROCESSOR_ARCHITEW6432
    if (!$arch) { $arch = $env:PROCESSOR_ARCHITECTURE }
    if ($arch -eq 'ARM64') {
        $name = 'stockfish-windows-arm64-universal'
        $hash = '8372ad3f0d7276deb2c70f801f541ec7db463219fc6d9c7592864e542aa4f401'
    } elseif ($arch -eq 'AMD64') {
        $name = 'stockfish-windows-x86-64-universal'
        $hash = '3c8bf1f9ea66a09350a40df4f632288285ac206d99f33ab5842c408fc30b48a7'
    } else { throw 'Unsupported Stockfish platform' }
    $parent = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    $work = Join-Path $parent ('.stockfish-' + [Guid]::NewGuid().ToString())
    New-Item -ItemType Directory -Path $work | Out-Null
    $archive = Join-Path $work 'archive.zip'
    Invoke-WebRequest -UseBasicParsing -TimeoutSec 180 -Uri "https://github.com/official-stockfish/Stockfish/releases/download/sf_19/$name.zip" -OutFile $archive
    if ((Get-FileHash -Algorithm SHA256 $archive).Hash.ToLowerInvariant() -ne $hash) { throw 'Stockfish checksum mismatch' }
    Expand-Archive -Path $archive -DestinationPath (Join-Path $work 'unpacked')
    $binary = Get-ChildItem (Join-Path $work 'unpacked') -Recurse -File -Filter "$name.exe" | Select-Object -First 1
    if (!$binary) { throw 'Stockfish executable missing' }
    Rename-Item $binary.FullName 'stockfish.exe'
    if (Test-Path $Destination) { throw 'Stockfish destination already exists' }
    Move-Item $binary.DirectoryName $Destination
    exit 0
} catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
} finally {
    if ($work -and (Test-Path $work)) { Remove-Item -Recurse -Force $work }
}
