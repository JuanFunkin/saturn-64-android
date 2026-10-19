# Saturn 64 Mobile - preparacion automatica (Windows / PowerShell)
# Descarga SDL2 y el codigo de Saturn, aplica los cambios para Android y deja todo listo
# para abrir en Android Studio. Necesita internet. Tarda unos minutos.

$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$root    = $PSScriptRoot
$tmp     = Join-Path $root "_descargas"
$jni     = Join-Path $root "app\src\main\jni"
$assets  = Join-Path $root "app\src\main\assets"
$javaSdl = Join-Path $root "app\src\main\java\org\libsdl\app"

$SDL_URL    = "https://github.com/libsdl-org/SDL/archive/refs/tags/release-2.28.5.zip"
$SATURN_URL = "https://github.com/Llennpie/Saturn/archive/5008553.zip"

function Descargar($url, $dest) {
    if (-not (Test-Path $dest)) {
        Write-Host "Descargando $url ..."
        Invoke-WebRequest -Uri $url -OutFile $dest -UseBasicParsing
    }
}
function Extraer($zip, $carpeta) {
    if (Test-Path $carpeta) { Remove-Item $carpeta -Recurse -Force }
    Write-Host "Extrayendo $zip ..."
    Expand-Archive -Path $zip -DestinationPath $carpeta -Force
    return (Get-ChildItem $carpeta -Directory | Select-Object -First 1).FullName
}
function Copiar($origen, $destino) {
    if (Test-Path $destino) { Remove-Item $destino -Recurse -Force }
    New-Item -ItemType Directory -Force -Path (Split-Path $destino) | Out-Null
    Copy-Item $origen $destino -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $tmp | Out-Null

# 1) SDL2 ---------------------------------------------------------------
Descargar $SDL_URL (Join-Path $tmp "sdl.zip")
$sdl = Extraer (Join-Path $tmp "sdl.zip") (Join-Path $tmp "sdl")
Copiar (Join-Path $sdl "include")     (Join-Path $jni "SDL2\include")
Copiar (Join-Path $sdl "src")         (Join-Path $jni "SDL2\src")
Copy-Item (Join-Path $sdl "Android.mk") (Join-Path $jni "SDL2\Android.mk") -Force
New-Item -ItemType Directory -Force -Path $javaSdl | Out-Null
Copy-Item (Join-Path $sdl "android-project\app\src\main\java\org\libsdl\app\*.java") $javaSdl -Force
# Cabeceras para poder escribir #include <SDL2/SDL.h> como en el codigo de Saturn
$compat = Join-Path $jni "main\compat\SDL2"
New-Item -ItemType Directory -Force -Path $compat | Out-Null
Copy-Item (Join-Path $sdl "include\*") $compat -Recurse -Force

# 2) Saturn -------------------------------------------------------------
Descargar $SATURN_URL (Join-Path $tmp "saturn.zip")
$sat = Extraer (Join-Path $tmp "saturn.zip") (Join-Path $tmp "saturn")
Copiar $sat (Join-Path $jni "saturn")

# 3) Cambios para Android (archivos modificados + archivos generados) ----
Copy-Item (Join-Path $root "overlay\saturn\*") (Join-Path $jni "saturn") -Recurse -Force

# 4) Archivos que Saturn lee al ejecutarse (van dentro del APK) -----------
if (Test-Path $assets) { Remove-Item $assets -Recurse -Force }
New-Item -ItemType Directory -Force -Path $assets | Out-Null
Copy-Item (Join-Path $sat "dynos") (Join-Path $assets "dynos") -Recurse -Force
Copy-Item (Join-Path $sat "fonts") (Join-Path $assets "fonts") -Recurse -Force
New-Item -ItemType Directory -Force -Path (Join-Path $assets "textures") | Out-Null
if (Test-Path (Join-Path $sat "textures\saturn")) {
    Copy-Item (Join-Path $sat "textures\saturn") (Join-Path $assets "textures\saturn") -Recurse -Force
}
if (Test-Path (Join-Path $sat "imgui.ini")) {
    Copy-Item (Join-Path $sat "imgui.ini") (Join-Path $assets "imgui.ini") -Force
}

Write-Host ""
Write-Host "Listo. Ahora abre esta carpeta en Android Studio (File > Open) y compila."
