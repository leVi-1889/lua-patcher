$steamPath = "C:\Program Files (x86)\Steam"

if (-not (Test-Path $steamPath)) {
    Write-Host "Steam folder not found at $steamPath" -ForegroundColor Red
    exit
}

$watcher = New-Object System.IO.FileSystemWatcher
$watcher.Path = $steamPath
$watcher.IncludeSubdirectories = $true
$watcher.EnableRaisingEvents = $true

$action = {
    $path = $Event.SourceEventArgs.FullPath
    $changeType = $Event.SourceEventArgs.ChangeType
    # Ignore log files and cache changes which happen constantly
    if ($path -notmatch "\.log$" -and $path -notmatch "\\appcache\\" -and $path -notmatch "\\logs\\") {
        Write-Host "[$changeType] $path" -ForegroundColor Cyan
    }
}

$created = Register-ObjectEvent $watcher "Created" -Action $action
$changed = Register-ObjectEvent $watcher "Changed" -Action $action
$deleted = Register-ObjectEvent $watcher "Deleted" -Action $action
$renamed = Register-ObjectEvent $watcher "Renamed" -Action $action

Write-Host "=================================================="
Write-Host "Monitoring Steam folder for changes..."
Write-Host "Path: $steamPath"
Write-Host "NOW: Install Steam Tools and watch this console."
Write-Host "Press ENTER to stop monitoring."
Write-Host "=================================================="

Read-Host

Unregister-Event -SourceIdentifier $created.Name
Unregister-Event -SourceIdentifier $changed.Name
Unregister-Event -SourceIdentifier $deleted.Name
Unregister-Event -SourceIdentifier $renamed.Name
$watcher.Dispose()
Write-Host "Monitoring stopped."
