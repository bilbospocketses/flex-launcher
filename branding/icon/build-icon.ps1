#Requires -Version 7.0
<#
.SYNOPSIS
    Regenerate every StreamFlex app-icon file from source and install it in the repo.

.DESCRIPTION
    1. build-svg.py writes the two vector masters next to this script:
       streamflex-full.svg (sizes 256-48) and streamflex-small.svg (sizes 40-16).
    2. Inkscape renders each of the ten ICO frames straight from its master (no raster
       resampling) into build/frames/, forced to 8-bit RGBA with date chunks stripped so
       a rebuild is byte-stable.
    3. pack-ico.py packs the frames into build/streamflex.ico.
    4. The outputs are copied to where the build and the docs site use them:
         streamflex-full.svg -> docs/streamflex.svg             (Linux scalable icon)
         frame 48            -> docs/streamflex.png             (Linux 48x48 icon)
         frames 32/24/16     -> docs/streamflex-{32,24,16}.png  (Linux 32/24/16 icons, from the small master)
         frame 32            -> docs/assets/icons/favicon.png   (docs site favicon)
         streamflex.ico      -> config/streamflex.ico           (Windows exe, via streamflex.rc)
    5. The ICO is verified: struct parse, and a pixel round-trip that must be exact (RMSE 0)
       for every frame. On Windows it is also read the way Windows consumers read it: WIC
       (Explorer's decoder), GDI+ (the legacy one) and rc.exe (the Windows build).

    Needs Python 3 with numpy + opencv-python, Inkscape 1.x and ImageMagick 7 on PATH.
    The rc.exe check needs the Windows 10/11 SDK, and is skipped with a notice without it.
    Run from anywhere: pwsh branding/icon/build-icon.ps1
#>
$ErrorActionPreference = 'Stop'

$here   = $PSScriptRoot
$repo   = (Resolve-Path (Join-Path $here '..' '..')).Path
$build  = Join-Path $here 'build'
$frames = Join-Path $build 'frames'
$ico    = Join-Path $build 'streamflex.ico'
New-Item -ItemType Directory -Force $frames | Out-Null

$ink = Get-Command inkscape.com -ErrorAction SilentlyContinue
if (-not $ink) { $ink = Get-Command inkscape -ErrorAction Stop }

'== 1. vector masters'
python (Join-Path $here 'build-svg.py')
if ($LASTEXITCODE -ne 0) { throw 'build-svg.py failed' }

'== 2. frames'
$plan = @( @(256, 'full'), @(128, 'full'), @(96, 'full'), @(64, 'full'), @(48, 'full'),
           @(40, 'small'), @(32, 'small'), @(24, 'small'), @(20, 'small'), @(16, 'small') )
foreach ($p in $plan) {
    $s   = $p[0]
    $svg = Join-Path $here "streamflex-$($p[1]).svg"
    $tmp = Join-Path $frames ('raw_{0:D3}.png' -f $s)
    $out = Join-Path $frames ('ico_{0:D3}.png' -f $s)
    & $ink.Source $svg --export-type=png --export-width=$s --export-height=$s --export-filename=$tmp 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $tmp)) { throw "inkscape failed for $s" }
    & magick $tmp -define png:color-type=6 -define png:exclude-chunks=date,time $out
    if ($LASTEXITCODE -ne 0) { throw "magick failed for $s" }
    Remove-Item $tmp
    & magick identify -format "%f %wx%h %[channels]`n" $out
}

'== 3. pack'
python (Join-Path $here 'pack-ico.py') $frames $ico
if ($LASTEXITCODE -ne 0) { throw 'pack-ico.py failed' }

'== 4. verify'
python (Join-Path $here 'parse-ico.py') $ico
if ($LASTEXITCODE -ne 0) { throw 'parse-ico.py failed' }
$rt = Join-Path $build 'roundtrip'
New-Item -ItemType Directory -Force $rt | Out-Null
Get-ChildItem $rt -Filter 'rt_*.png' | Remove-Item
& magick $ico (Join-Path $rt 'rt_%02d.png')
$sizes = $plan | ForEach-Object { $_[0] }
$bad = 0
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $m = (& magick compare -metric RMSE (Join-Path $frames ('ico_{0:D3}.png' -f $sizes[$i])) (Join-Path $rt ('rt_{0:D2}.png' -f $i)) null: 2>&1) -join ''
    if ($m -notmatch '^0 \(0\)$') { $bad++; "  frame $($sizes[$i]) round-trip: $m" }
}
if ($bad) { throw "$bad frame(s) did not round-trip exactly" }
"round-trip exact for all $($sizes.Count) frames"

if ($IsWindows) {
    # The checks above prove the bytes; these prove the Windows readers accept them.
    Add-Type -AssemblyName PresentationCore
    $dec = [System.Windows.Media.Imaging.BitmapDecoder]::Create([uri]::new($ico),
        [System.Windows.Media.Imaging.BitmapCreateOptions]::PreservePixelFormat,
        [System.Windows.Media.Imaging.BitmapCacheOption]::OnLoad)
    $got  = $dec.Frames | ForEach-Object { '{0}x{1} {2}' -f $_.PixelWidth, $_.PixelHeight, $_.Format }
    $want = $sizes | ForEach-Object { "${_}x${_} Bgra32" }
    if (Compare-Object $want $got) { throw "WIC decode differs from the plan: $($got -join ', ')" }
    "WIC (Explorer): all $($got.Count) frames decode as Bgra32"

    # GDI+ cannot read the PNG-compressed 256 entry and falls back to 128; every other size must be exact.
    Add-Type -AssemblyName System.Drawing
    foreach ($s in $sizes) {
        $icon = [System.Drawing.Icon]::new($ico, [System.Drawing.Size]::new($s, $s))
        $w = $icon.Width
        $icon.Dispose()
        $expect = if ($s -eq 256) { 128 } else { $s }
        if ($w -ne $expect) { throw "GDI+ asked for $s, got $w (expected $expect)" }
    }
    'GDI+ (legacy): every size exact, 256 falls back to 128 as expected'

    $rc = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin\*\x64\rc.exe' -ErrorAction SilentlyContinue |
        Sort-Object { [version]$_.Directory.Parent.Name } | Select-Object -Last 1
    if ($rc) {
        # Compile a copy of the real streamflex.rc against the new icon, as the Windows build will.
        $rcDir = Join-Path $build 'rc'
        New-Item -ItemType Directory -Force $rcDir | Out-Null
        Copy-Item -LiteralPath (Join-Path $repo 'config' 'streamflex.rc'), $ico -Destination $rcDir -Force
        & $rc.FullName /nologo /fo (Join-Path $rcDir 'streamflex.res') (Join-Path $rcDir 'streamflex.rc')
        if ($LASTEXITCODE -ne 0) { throw "rc.exe failed ($LASTEXITCODE)" }
        "rc.exe $($rc.Directory.Parent.Name): compiles streamflex.rc ($((Get-Item (Join-Path $rcDir 'streamflex.res')).Length) B .res)"
    } else {
        'rc.exe: Windows SDK not found, check SKIPPED (CI compiles the .rc in the Windows build)'
    }
}

'== 5. install'
# Pairs, not a hashtable: frame 32 feeds two outputs, and a hashtable keyed by source would drop one.
$install = @(
    @((Join-Path $here 'streamflex-full.svg'), 'docs/streamflex.svg'),
    @((Join-Path $frames 'ico_048.png'),       'docs/streamflex.png'),
    @((Join-Path $frames 'ico_032.png'),       'docs/streamflex-32.png'),
    @((Join-Path $frames 'ico_024.png'),       'docs/streamflex-24.png'),
    @((Join-Path $frames 'ico_016.png'),       'docs/streamflex-16.png'),
    @((Join-Path $frames 'ico_032.png'),       'docs/assets/icons/favicon.png'),
    @($ico,                                    'config/streamflex.ico')
)
foreach ($p in $install) {
    $to = Join-Path $repo $p[1]
    Copy-Item -LiteralPath $p[0] -Destination $to -Force
    '{0,-32} {1}' -f $p[1], (Get-FileHash $to -Algorithm SHA256).Hash.Substring(0, 16)
}
'== changes against the committed icon (empty = the rebuild reproduced it exactly):'
git -C $repo status --short -- branding/icon ($install | ForEach-Object { $_[1] })
