$loc = (Get-Location).Path
Get-ChildItem -Path $PSScriptRoot -Directory | ForEach-Object{
    cd $_.FullName
    git status
    git pull --recurse-submodules --prune -4
}
cd $loc