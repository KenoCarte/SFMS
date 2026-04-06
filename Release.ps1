$ErrorActionPreference = "Stop"

$Project = "D:\Program1\SFMS"
$QtRoot  = "F:\Qt\6.11.0\mingw_64"
$MinGW   = "C:\MinGW"

$Build = Join-Path $Project "build-release"
$Dist  = Join-Path $Project "dist"
$ExeInBuild = Join-Path $Build "QtFileManager.exe"
$ExeInDist  = Join-Path $Dist "QtFileManager.exe"
$Windeployqt = Join-Path $QtRoot "bin\windeployqt.exe"
$QtCmake = Join-Path $QtRoot "bin\cmake.exe"
$QtQmake = Join-Path $QtRoot "bin\qmake.exe"
$MingwMake = Join-Path $MinGW "bin\mingw32-make.exe"

function Assert-PathExists {
	param(
		[Parameter(Mandatory = $true)]
		[string]$Path,
		[Parameter(Mandatory = $true)]
		[string]$Message
	)

	if (-not (Test-Path $Path)) {
		throw $Message
	}
}

function Resolve-Tool {
	param(
		[object[]]$Candidates,
		[string]$ToolName,
		[switch]$Optional
	)

	if ($null -eq $Candidates -or $Candidates.Count -eq 0) {
		if ($Optional) {
			return $null
		}
		throw "Cannot find $ToolName. Candidate list is empty."
	}

	foreach ($candidate in $Candidates) {
		if ([string]::IsNullOrWhiteSpace($candidate)) {
			continue
		}
		if (Test-Path $candidate) {
			return $candidate
		}
	}

	if ($Optional) {
		return $null
	}

	throw "Cannot find $ToolName. Check the install path or PATH."
}

Assert-PathExists -Path $QtRoot -Message "Qt directory not found: $QtRoot"
Assert-PathExists -Path $Windeployqt -Message "windeployqt not found: $Windeployqt"

$CmakeCandidates = @()
$FoundCmake = (Get-Command cmake -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -ErrorAction SilentlyContinue)
if (-not [string]::IsNullOrWhiteSpace($FoundCmake)) { $CmakeCandidates += $FoundCmake }
if (Test-Path $QtCmake) { $CmakeCandidates += $QtCmake }
$CmakeExe = Resolve-Tool -ToolName "cmake" -Candidates $CmakeCandidates -Optional

$QmakeCandidates = @()
if (Test-Path $QtQmake) { $QmakeCandidates += $QtQmake }
$FoundQmake = (Get-Command qmake -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -ErrorAction SilentlyContinue)
if (-not [string]::IsNullOrWhiteSpace($FoundQmake)) { $QmakeCandidates += $FoundQmake }
$QmakeExe = Resolve-Tool -ToolName "qmake" -Candidates $QmakeCandidates

$MingwMakeCandidates = @()
if (Test-Path $MingwMake) { $MingwMakeCandidates += $MingwMake }
$FoundMingwMake = (Get-Command mingw32-make -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -ErrorAction SilentlyContinue)
if (-not [string]::IsNullOrWhiteSpace($FoundMingwMake)) { $MingwMakeCandidates += $FoundMingwMake }
$MingwMakeExe = Resolve-Tool -ToolName "mingw32-make" -Candidates $MingwMakeCandidates

Write-Host "[1/5] Cleaning old build folders..."
Remove-Item $Build, $Dist -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $Build, $Dist | Out-Null

if (-not [string]::IsNullOrWhiteSpace($CmakeExe) -and (Test-Path $CmakeExe)) {
	Write-Host "[2/5] Configuring with CMake..."
	& $CmakeExe -S $Project -B $Build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="$QtRoot"

	Write-Host "[3/5] Building Release with CMake..."
	& $CmakeExe --build $Build --config Release -j 4
} else {
	Write-Host "[2/5] cmake not found, falling back to qmake..."
	# Remove stale qmake artifacts in source tree that may reference old Anaconda Qt libs.
	Remove-Item (Join-Path $Project ".qmake.stash") -Force -ErrorAction SilentlyContinue
	Remove-Item (Join-Path $Project "Makefile") -Force -ErrorAction SilentlyContinue
	Remove-Item (Join-Path $Project "Makefile.Debug") -Force -ErrorAction SilentlyContinue
	Remove-Item (Join-Path $Project "Makefile.Release") -Force -ErrorAction SilentlyContinue

	Push-Location $Build
	try {
		# Generate makefiles in build-release only (shadow build), and force MinGW spec.
		& $QmakeExe -r -spec win32-g++ (Join-Path $Project "QtFileManager.pro")
		Write-Host "[3/5] Building Release with qmake..."
		& $MingwMakeExe -j 4
	} finally {
		Pop-Location
	}
}

function Find-BuiltExe {
	param([string]$Root)

	$found = Get-ChildItem -Path $Root -Recurse -Filter "QtFileManager.exe" -ErrorAction SilentlyContinue |
		Select-Object -First 1 -ExpandProperty FullName
	return $found
}

$BuiltExe = Find-BuiltExe -Root $Build
if ([string]::IsNullOrWhiteSpace($BuiltExe)) {
	throw "Built executable not found under: $Build"
}

Write-Host "[4/5] Copying executable to dist..."
Copy-Item $BuiltExe $ExeInDist -Force

Write-Host "[5/5] Deploying Qt runtime dependencies..."
& $Windeployqt --release --compiler-runtime --dir $Dist $ExeInDist

if (Test-Path (Join-Path $MinGW "bin\libgcc_s_seh-1.dll")) {
	Copy-Item (Join-Path $MinGW "bin\libgcc_s_seh-1.dll") $Dist -Force
}
if (Test-Path (Join-Path $MinGW "bin\libstdc++-6.dll")) {
	Copy-Item (Join-Path $MinGW "bin\libstdc++-6.dll") $Dist -Force
}
if (Test-Path (Join-Path $MinGW "bin\libwinpthread-1.dll")) {
	Copy-Item (Join-Path $MinGW "bin\libwinpthread-1.dll") $Dist -Force
}

Write-Host "Done. Launching the app..."
Start-Process -FilePath $ExeInDist -WorkingDirectory $Dist