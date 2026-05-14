$wshShell = New-Object -ComObject WScript.Shell
$desktop = [Environment]::GetFolderPath('Desktop')
$startMenu = [Environment]::GetFolderPath('Programs')

Write-Host "=== Desktop Shortcuts ==="
Get-ChildItem -Path $desktop -Filter '*Steam*.lnk' -ErrorAction SilentlyContinue | ForEach-Object {
    $shortcut = $wshShell.CreateShortcut($_.FullName)
    Write-Host "Name: $($_.Name)"
    Write-Host "Target: $($shortcut.TargetPath)"
    Write-Host "Args: $($shortcut.Arguments)"
    Write-Host "---"
}

Write-Host ""
Write-Host "=== Start Menu Shortcuts ==="
Get-ChildItem -Path $startMenu -Filter '*Steam*.lnk' -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
    $shortcut = $wshShell.CreateShortcut($_.FullName)
    Write-Host "Name: $($_.Name)"
    Write-Host "Target: $($shortcut.TargetPath)"
    Write-Host "Args: $($shortcut.Arguments)"
    Write-Host "---"
}

Write-Host ""
Write-Host "=== ALL Desktop .lnk files ==="
Get-ChildItem -Path $desktop -Filter '*.lnk' -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host $_.Name
}
