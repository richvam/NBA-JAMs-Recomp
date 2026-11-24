# test_installer.ps1 <setup.exe> - the R3.1 check, run on a Windows CI runner.
#
# 1. silent per-user install into a test folder: the program, mods and README are there,
#    portable.txt is not, the Start menu entry exists, no administrator rights were needed
# 2. the installed program answers --version
# 3. a pretend user file in %APPDATA%\NBAJamRecomp; silent uninstall: the program folder is
#    gone, the user's file is still there
# 4. install again, uninstall with /PURGE: the data folder is gone too
param([Parameter(Mandatory = $true)][string]$Setup)
$ErrorActionPreference = 'Stop'
$failed = 0
function Check([bool]$ok, [string]$what) {
	if ($ok) { Write-Host "PASS  $what" } else { Write-Host "FAIL  $what"; $script:failed++ }
}

$dir = Join-Path $env:RUNNER_TEMP 'NBAJamRecomp-install'
$data = Join-Path $env:APPDATA 'NBAJamRecomp'
$menu = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\NBA JAM Recomp.lnk'
if (Test-Path $data) { Remove-Item -Recurse -Force $data }

function Install {
	$p = Start-Process -FilePath $Setup -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/CURRENTUSER', "/DIR=$dir", "/LOG=$env:RUNNER_TEMP\setup.log" -Wait -PassThru
	return $p.ExitCode
}
function Uninstall([string[]]$extra) {
	$u = Join-Path $dir 'unins000.exe'
	$p = Start-Process -FilePath $u -ArgumentList (@('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART') + $extra) -Wait -PassThru
	# the uninstaller copies itself and returns at once: wait for the folder to go
	for ($i = 0; $i -lt 60 -and (Test-Path (Join-Path $dir 'NBAJamRecomp.exe')); $i++) { Start-Sleep -Milliseconds 500 }
	Start-Sleep -Seconds 2
	return $p.ExitCode
}

# 1
Check ((Install) -eq 0) 'silent per-user install exits 0'
Check (Test-Path (Join-Path $dir 'NBAJamRecomp.exe')) 'NBAJamRecomp.exe installed'
Check (Test-Path (Join-Path $dir 'README.txt')) 'README.txt installed'
Check ((Get-ChildItem -Recurse (Join-Path $dir 'mods') -Filter *.lua).Count -gt 10) 'the mods are installed'
Check (-not (Test-Path (Join-Path $dir 'portable.txt'))) 'no portable.txt: the program uses the per-user folder'
Check (Test-Path $menu) 'Start menu shortcut'
Check (Test-Path (Join-Path $data 'roms')) 'the ROMs folder is made in %APPDATA%\NBAJamRecomp'

# 2
$v = Start-Process -FilePath (Join-Path $dir 'NBAJamRecomp.exe') -ArgumentList '--version' -Wait -PassThru -NoNewWindow
Check ($v.ExitCode -eq 0) 'the installed program runs (--version)'

# 3
Set-Content -Path (Join-Path $data 'nbajam_recomp.ini') -Value 'volume=0.5'
Check ((Uninstall @()) -eq 0) 'silent uninstall exits 0'
Check (-not (Test-Path (Join-Path $dir 'NBAJamRecomp.exe'))) 'the program is removed'
Check (-not (Test-Path $menu)) 'the Start menu shortcut is removed'
Check (Test-Path (Join-Path $data 'nbajam_recomp.ini')) "the user's settings are kept"
Check (Test-Path (Join-Path $data 'roms')) "the user's ROMs folder is kept"

# 4
Check ((Install) -eq 0) 'install again'
Check ((Uninstall @('/PURGE')) -eq 0) 'silent uninstall with /PURGE exits 0'
Check (-not (Test-Path $data)) 'with /PURGE the data folder is removed'

Write-Host "installer: $failed failed"
if ($failed -gt 0) { Get-Content "$env:RUNNER_TEMP\setup.log" -ErrorAction SilentlyContinue | Select-Object -Last 40; exit 1 }
