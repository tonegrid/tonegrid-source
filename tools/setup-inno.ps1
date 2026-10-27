param([Parameter(Mandatory=$true)][string]$ToolDirectory)
$ErrorActionPreference = 'Stop'
$taskToolDirectory = [System.IO.Path]::GetFullPath($ToolDirectory)
New-Item -ItemType Directory -Path $taskToolDirectory -Force | Out-Null
$taskDownload = Join-Path $taskToolDirectory 'innosetup-7.1.0-x64.exe'
$taskCompilerRoot = Join-Path $taskToolDirectory 'InnoSetup'
$taskExpectedHash = '0362a383ed217d4c4239b5933866dd96d3eb2102737da92f80f6057a4b40df2f'
Invoke-WebRequest -Uri 'https://github.com/jrsoftware/issrc/releases/download/is-7_1_0/innosetup-7.1.0-x64.exe' -OutFile $taskDownload
if ((Get-FileHash -LiteralPath $taskDownload -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskExpectedHash) { throw 'Inno Setup package checksum mismatch.' }
$taskSignature = Get-AuthenticodeSignature -LiteralPath $taskDownload
if ($taskSignature.Status -ne 'Valid' -or $taskSignature.SignerCertificate.Subject -notmatch 'Pyrsys') { throw 'Official Inno Setup publisher signature is not valid.' }
$taskCompilerProcess = Start-Process -FilePath $taskDownload -ArgumentList @('/CURRENTUSER','/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/NOICONS','/TASKS="!fileassoc"', ('/DIR="'+$taskCompilerRoot+'"'), ('/LOG="'+(Join-Path $taskToolDirectory 'install.log')+'"')) -WindowStyle Hidden -Wait -PassThru
if ($taskCompilerProcess.ExitCode -ne 0) { throw 'Inno compiler installation failed.' }
Write-Output (Join-Path $taskCompilerRoot 'ISCC.exe')
