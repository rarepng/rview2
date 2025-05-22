$loc = (Get-Location).Path
Get-ChildItem -Path $PSScriptRoot -Directory | ForEach-Object{
    Set-Location $_.FullName
    git status
    git pull --recurse-submodules --prune -4
}
Set-Location $loc