param(
    [string] $RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'

$iPlugRoot = Join-Path $RepositoryRoot 'vendor\iPlug2'
$dependencyRoot = Join-Path $iPlugRoot 'Dependencies\IPlug'
$vst3 = Join-Path $dependencyRoot 'VST3_SDK'
$clap = Join-Path $dependencyRoot 'CLAP_SDK'
$clapHelpers = Join-Path $dependencyRoot 'CLAP_HELPERS'

if (-not (Test-Path -LiteralPath (Join-Path $iPlugRoot 'iPlug2.cmake') -PathType Leaf)) {
    throw 'iPlug2 is missing. Run git submodule update --init --recursive first.'
}

function Install-GitDependency {
    param(
        [string] $Path,
        [string] $Url,
        [string] $Revision,
        [string] $PlaceholderName,
        [switch] $Recursive
    )

    if (Test-Path -LiteralPath (Join-Path $Path '.git')) {
        $current = (& git -C $Path rev-parse HEAD).Trim()
        if ($current -eq $Revision) {
            Write-Host "Already pinned: $Path"
            return
        }
        throw "Unexpected dependency revision at $Path ($current). Remove it manually before bootstrapping."
    }

    $placeholderBytes = $null
    if (Test-Path -LiteralPath $Path) {
        $entries = @(Get-ChildItem -LiteralPath $Path -Force)
        if ($entries.Count -eq 1 -and $entries[0].Name -eq $PlaceholderName) {
            $placeholderBytes = [System.IO.File]::ReadAllBytes($entries[0].FullName)
        } elseif ($entries.Count -gt 0) {
            throw "Dependency placeholder is not empty: $Path"
        }
        Remove-Item -LiteralPath $Path -Recurse -Force
    }

    $arguments = @('clone', '--no-checkout', $Url, $Path)
    & git @arguments
    if ($LASTEXITCODE -ne 0) { throw "git clone failed for $Url" }
    & git -C $Path checkout --detach $Revision
    if ($LASTEXITCODE -ne 0) { throw "git checkout failed for $Revision" }
    if ($Recursive) {
        & git -C $Path submodule update --init --recursive
        if ($LASTEXITCODE -ne 0) { throw "git submodule update failed for $Path" }
    }
    if ($null -ne $placeholderBytes) {
        [System.IO.File]::WriteAllBytes((Join-Path $Path $PlaceholderName), $placeholderBytes)
    }
}

Install-GitDependency -Path $vst3 -Url 'https://github.com/steinbergmedia/vst3sdk.git' `
    -Revision '3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96' -PlaceholderName 'README.md' -Recursive
Install-GitDependency -Path $clap -Url 'https://github.com/free-audio/clap.git' `
    -Revision 'a47f6badb49d948fd009998f28309cdab78979c9' -PlaceholderName 'readme.txt'
Install-GitDependency -Path $clapHelpers -Url 'https://github.com/free-audio/clap-helpers.git' `
    -Revision 'c35dd4906bd8efbb900cb2b89e680fed463cc8b1' -PlaceholderName 'readme.txt'

Write-Host 'iPlug2 format SDKs are ready.'
