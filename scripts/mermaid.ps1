# was before i decided on just doxygen+graphviz
param (
    [string]$Path = ".",
    [switch]$NoRecurse,
    [switch]$ShowStdLib
)

function Get-SafeId ($rawName) {
    return $rawName -replace '[^a-zA-Z0-9_]', '_'
}

$childItemParams = @{ Path = $Path; Filter = "*.class"; File = $true }
if (-not $NoRecurse) { $childItemParams["Recurse"] = $true }

try {
    $files = Get-ChildItem @childItemParams -ErrorAction Stop
} catch {
    Write-Error "No .class files found."
    exit
}

if ($files.Count -eq 0) {
    Write-Warning "No *.class files found in $(Resolve-Path $Path)"
    exit
}

Write-Host "Processing $($files.Count) dump files..." -ForegroundColor Cyan

$classes = [System.Collections.Generic.HashSet[string]]::new()
$relationships = [System.Collections.Generic.HashSet[string]]::new()

$reClassStart = '^Class\s+(.+)$'
$reBaseStart  = '^\s+base types:\s*$'
$reInherited  = '^\s{4,}(.+)$'

foreach ($file in $files) {
    $lines = Get-Content $file.FullName
    $currentClass = $null
    $state = "SEARCHING"

    foreach ($line in $lines) {
        if ($line -match $reClassStart) {
            $rawName = $matches[1].Trim().Split(' ')[0]
            
            if ($rawName -notmatch '^__' -and $rawName -notmatch ' vtable ') {
                $currentClass = $rawName
                [void]$classes.Add($currentClass)
                $state = "FOUND_CLASS"
            }
            continue
        }

        if ($state -eq "FOUND_CLASS") {
            if ($line -match $reBaseStart) {
                $state = "READING_BASES"
                continue
            }
            if (-not [string]::IsNullOrWhiteSpace($line) -and -not $line.StartsWith(" ")) {
                $state = "SEARCHING"
            }
        }

        if ($state -eq "READING_BASES") {
            if (-not $line.StartsWith(" ")) {
                $state = "SEARCHING"
                continue
            }
            if ($line -match $reInherited) {
                $baseName = $matches[1].Trim().Split(' ')[0]
                if ($baseName -notmatch '^__') {
                    [void]$relationships.Add("$baseName|$currentClass")
                }
            }
        }
    }
}

Write-Output "classDiagram"

$definedClasses = [System.Collections.Generic.HashSet[string]]::new()

foreach ($rel in $relationships) {
    $parts = $rel.Split('|')
    $baseName = $parts[0]
    $childName = $parts[1]

    if (-not $ShowStdLib -and ($baseName.StartsWith("std::") -or $childName.StartsWith("std::"))) { continue }

    $baseId = Get-SafeId $baseName
    $childId = Get-SafeId $childName

    if (-not $definedClasses.Contains($baseId)) {
        Write-Output "    class $baseId[`"$baseName`"]"
        [void]$definedClasses.Add($baseId)
    }
    if (-not $definedClasses.Contains($childId)) {
        Write-Output "    class $childId[`"$childName`"]"
        [void]$definedClasses.Add($childId)
    }

    Write-Output "    $baseId <|-- $childId"
}

foreach ($cls in $classes) {
    if (-not $ShowStdLib -and $cls.StartsWith("std::")) { continue }

    $clsId = Get-SafeId $cls
    if (-not $definedClasses.Contains($clsId)) {
        Write-Output "    class $clsId[`"$cls`"]"
        [void]$definedClasses.Add($clsId)
    }
}