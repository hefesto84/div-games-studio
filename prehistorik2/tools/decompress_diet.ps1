# Descomprime los ficheros .SQZ comprimidos con DIET (los que estan en
# prehistorik2/raw/) usando el DIET.EXE original (v1.45f, Teddy Matsumoto,
# 1992, freeware) dentro de DOSBox, y copia el resultado a
# prehistorik2/assets/original/decompressed/.
#
# Requisitos: DOSBox (winget install DOSBox.DOSBox) y el propio DIET.EXE en
# tools/diet_tool/extracted/DIET.EXE (extraido de diet145f.lzh, descargado
# de https://ftp.vector.co.jp/00/03/527/diet145f.lzh, el mismo enlace que
# recomienda la documentacion tecnica de pre2.mine.nu).
#
# Nota: DIET solo descomprime los ficheros comprimidos con DIET. Seis
# ficheros del juego usan compresion ZIV en su lugar (CASTLE.SQZ,
# THEEND.SQZ, PRESENT.SQZ, KEYB.SQZ, SAMPLE.SQZ, TITUS.SQZ) y DIET los
# ignora sin avisar (ver docs/format_notes.md); MENU2.SQZ dio "CRC error"
# con DIET (puede que el propio fichero de origen este da�ado) y se ha
# dejado fuera del resultado.

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$dietExe = Join-Path $root "tools\diet_tool\extracted\DIET.EXE"
$rawDir = Join-Path $root "raw"
$outDir = Join-Path $root "assets\original\decompressed"
$workDir = Join-Path $root "tools\diet_tool\_scratch"
$dosboxExe = "C:\Program Files (x86)\DOSBox-0.74-3\DOSBox.exe"

if (-not (Test-Path $dietExe)) { throw "No se encuentra DIET.EXE en $dietExe" }
if (-not (Test-Path $dosboxExe)) { throw "No se encuentra DOSBox en $dosboxExe" }

New-Item -ItemType Directory -Force -Path $workDir | Out-Null
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Copy-Item $dietExe (Join-Path $workDir "DIET.EXE") -Force
Copy-Item (Join-Path $rawDir "*.SQZ") $workDir -Force

$conf = @"
[autoexec]
mount c $workDir
c:
diet.exe -R *.SQZ > result.txt
exit
"@
$confPath = Join-Path $workDir "decompress.conf"
Set-Content -Path $confPath -Value $conf -Encoding ASCII

Start-Process -FilePath $dosboxExe -ArgumentList "-conf `"$confPath`"" -Wait -WindowStyle Minimized

$resultPath = Join-Path $workDir "result.txt"
if (Test-Path $resultPath) {
    Write-Host (Get-Content $resultPath -Raw)
}

Get-ChildItem (Join-Path $workDir "*.SQZ") | ForEach-Object {
    Copy-Item $_.FullName $outDir -Force
}

Remove-Item (Join-Path $outDir "DIET.EXE") -ErrorAction SilentlyContinue
Write-Host "Copiado a $outDir"
Write-Host "Revisa result.txt arriba por si algun fichero dio 'CRC error' (queda igual que el original, sin descomprimir)."
