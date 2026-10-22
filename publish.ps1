[CmdletBinding()]
param(
    [switch]$Publish,
    [switch]$DryRun,
    [string]$Version = '1.0.3'
)

$ErrorActionPreference = 'Stop'
$Repository = 'CookieCums/cummy'
$RemoteUrl = 'https://github.com/CookieCums/cummy.git'
if ($Version -notmatch '^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$') {
    throw "Version '$Version' is invalid. Use semantic version format, for example 1.0.1."
}
$ReleaseTag = "v$Version"
$ReleaseTitle = "CummY $Version"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$OutputDirectory = Join-Path $Root 'output'
$ChangelogPath = Join-Path $Root 'CHANGELOG.md'

if (-not (Test-Path -LiteralPath $ChangelogPath -PathType Leaf)) {
    throw "Changelog not found: $ChangelogPath"
}
$Changelog = Get-Content -LiteralPath $ChangelogPath -Raw -Encoding UTF8
$VersionPattern = "(?ms)^## \[$([regex]::Escape($Version))\][^\r\n]*\r?\n(.*?)(?=^## |\z)"
$VersionMatch = [regex]::Match($Changelog, $VersionPattern)
if (-not $VersionMatch.Success -or [string]::IsNullOrWhiteSpace($VersionMatch.Groups[1].Value)) {
    throw "CHANGELOG.md must contain a non-empty section headed '## [$Version]'."
}
$ReleaseNotes = $VersionMatch.Groups[1].Value.Trim()

$SourceFiles = @(
    'README.md',
    'CHANGELOG.md',
    '.gitignore',
    'publish.ps1',
    'cummy.cpp',
    'project_scanner.hpp',
    'cummy_icon.rc',
    'cummy_setup.iss',
    'assets/cummY-github.png',
    'assets/cummY-app.png',
    'assets/cummY-app.ico'
)
$ReleaseAssets = @(
    'cummy.exe',
    "cummy-setup-$Version.exe"
)
$ReleaseAssetPaths = @($ReleaseAssets | ForEach-Object { Join-Path $OutputDirectory $_ })
$RequiredFiles = $SourceFiles + @(
    'output\cummy.exe',
    "output\cummy-setup-$Version.exe"
)
$MissingFiles = @($RequiredFiles | Where-Object { -not (Test-Path -LiteralPath (Join-Path $Root $_) -PathType Leaf) })

if ($MissingFiles.Count -gt 0) {
    throw "Required publish files are missing: $($MissingFiles -join ', ')"
}

$LatestDirectories = @()
if (Test-Path -LiteralPath $OutputDirectory -PathType Container) {
    $LatestDirectories = @(Get-ChildItem -LiteralPath $OutputDirectory -Directory |
        Where-Object { $_.Name -match '^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)? \(latest\)$' })
}
if ($LatestDirectories.Count -gt 1) {
    throw "Expected at most one '(latest)' release folder in $OutputDirectory."
}
$CurrentLatestDirectory = if ($LatestDirectories.Count -eq 1) { $LatestDirectories[0] } else { $null }
$TargetDirectory = Join-Path $OutputDirectory "$Version (latest)"
$VersionDirectory = Join-Path $OutputDirectory $Version
if ((Test-Path -LiteralPath $VersionDirectory) -and
    (-not $CurrentLatestDirectory -or $CurrentLatestDirectory.FullName -ne $TargetDirectory)) {
    throw "Release folder already exists: $VersionDirectory"
}
if ($CurrentLatestDirectory -and $CurrentLatestDirectory.FullName -ne $TargetDirectory) {
    $CurrentVersionName = $CurrentLatestDirectory.Name -replace ' \(latest\)$', ''
    $FormerVersionDirectory = Join-Path $OutputDirectory $CurrentVersionName
    if (Test-Path -LiteralPath $FormerVersionDirectory) {
        throw "Cannot archive latest folder because its destination already exists: $FormerVersionDirectory"
    }
}
$StagingDirectory = Join-Path $OutputDirectory ".staging-$Version"
if (Test-Path -LiteralPath $StagingDirectory) {
    throw "Release staging folder already exists: $StagingDirectory. Inspect or move it before retrying."
}

$TotalBytes = [long]0
foreach ($File in $RequiredFiles) {
    $TotalBytes += (Get-Item -LiteralPath (Join-Path $Root $File)).Length
}

Write-Output "Repository: https://github.com/$Repository"
Write-Output "Visibility: public"
Write-Output "Release: $ReleaseTag"
Write-Output 'Release notes:'
Write-Output $ReleaseNotes
Write-Output 'Files to commit:'
foreach ($File in $SourceFiles) {
    Write-Output "  $File"
}
Write-Output 'Release assets:'
foreach ($Asset in $ReleaseAssets) {
    $Info = Get-Item -LiteralPath (Join-Path $OutputDirectory $Asset)
    Write-Output ("  {0} ({1:N2} MB)" -f $Asset, ($Info.Length / 1MB))
}
Write-Output "Versioned output folder: output\$Version (latest)"
Write-Output ("Approximate total upload: {0:N2} MB" -f ($TotalBytes / 1MB))

if (-not $Publish -or $DryRun) {
    if ($Publish -and $DryRun) {
        Write-Output 'Dry run: -Publish was received, but no changes or network operations were performed.'
        Write-Output "Run .\publish.ps1 -Publish -Version $Version when you are ready to publish."
    } else {
        Write-Output 'Preview only. Nothing was initialized, committed, tagged, pushed, or uploaded.'
        Write-Output "Run .\publish.ps1 -Publish -Version $Version to publish $ReleaseTag."
    }
    return
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw 'Git was not found in PATH. Reopen the terminal after adding Git for Windows to PATH.'
}
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    throw 'GitHub CLI (gh) was not found in PATH.'
}

& gh auth status
if ($LASTEXITCODE -ne 0) {
    throw 'GitHub CLI is not authenticated. Run gh auth login first.'
}
& gh auth setup-git
if ($LASTEXITCODE -ne 0) {
    throw 'Could not configure Git to use the authenticated GitHub CLI credentials.'
}

$RepositoryJson = & gh repo view $Repository --json url,isPrivate
if ($LASTEXITCODE -ne 0) {
    throw "GitHub repository $Repository could not be accessed."
}
$RepositoryInfo = ($RepositoryJson -join "`n") | ConvertFrom-Json
if ($RepositoryInfo.isPrivate) {
    throw 'The repository is private, but this release plan expects a public repository.'
}

$UserJson = & gh api user
if ($LASTEXITCODE -ne 0) {
    throw 'Could not read the authenticated GitHub account.'
}
$GitHubUser = ($UserJson -join "`n") | ConvertFrom-Json
if ($GitHubUser.login -ine 'CookieCums') {
    throw "Expected to publish as CookieCums, but gh is authenticated as $($GitHubUser.login)."
}

Push-Location $Root
try {
    if (-not (Test-Path -LiteralPath '.git' -PathType Container)) {
        & git init --initial-branch=main
        if ($LASTEXITCODE -ne 0) { throw 'git init failed.' }
    }

    $Branch = & git branch --show-current
    if ($LASTEXITCODE -ne 0) { throw 'Could not read the current Git branch.' }
    if ($Branch -and $Branch -ne 'main') {
        throw "Current branch is '$Branch'; switch to main before publishing."
    }

    $Remotes = @(& git remote)
    if ($LASTEXITCODE -ne 0) { throw 'Could not read Git remotes.' }
    if ($Remotes -contains 'origin') {
        $CurrentRemote = (& git remote get-url origin).Trim()
        if ($LASTEXITCODE -ne 0) { throw 'Could not read the origin URL.' }
        if ($CurrentRemote -notin @($RemoteUrl, 'https://github.com/CookieCums/cummy', 'git@github.com:CookieCums/cummy.git')) {
            throw "Origin points to '$CurrentRemote', not $RemoteUrl."
        }
    } else {
        & git remote add origin $RemoteUrl
        if ($LASTEXITCODE -ne 0) { throw 'Could not configure origin.' }
    }

    & git config user.name $GitHubUser.login
    if ($LASTEXITCODE -ne 0) { throw 'Could not configure the local Git author name.' }
    $GitHubEmail = "$($GitHubUser.id)+$($GitHubUser.login)@users.noreply.github.com"
    & git config user.email $GitHubEmail
    if ($LASTEXITCODE -ne 0) { throw 'Could not configure the local Git author email.' }

    & git add -f -- $SourceFiles
    if ($LASTEXITCODE -ne 0) { throw 'Could not stage the project files.' }

    & git diff --cached --quiet
    $StagedDiff = $LASTEXITCODE
    $Head = & git rev-parse --verify --quiet HEAD
    $HasHead = $LASTEXITCODE -eq 0
    if (-not $HasHead -or $StagedDiff -eq 1) {
        if ($StagedDiff -ne 1) { throw 'There are no staged files to commit.' }
        & git commit -m "Prepare CummY release $ReleaseTag" -m 'Pushed-by: Cumming...'
        if ($LASTEXITCODE -ne 0) { throw 'git commit failed.' }
    } elseif ($StagedDiff -ne 0) {
        throw 'Could not inspect staged changes.'
    }

    $Head = (& git rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Could not read the commit to release.' }
    & git show-ref --verify --quiet "refs/tags/$ReleaseTag"
    $TagExists = $LASTEXITCODE -eq 0
    if ($TagExists) {
        $TaggedCommit = (& git rev-list -n 1 $ReleaseTag).Trim()
        if ($TaggedCommit -ne $Head) {
            throw "Tag $ReleaseTag already points to a different commit."
        }
    } else {
        & git tag -a $ReleaseTag -m $ReleaseTitle
        if ($LASTEXITCODE -ne 0) { throw "Could not create tag $ReleaseTag." }
    }

    & git push --set-upstream origin main
    if ($LASTEXITCODE -ne 0) { throw 'Could not push the main branch.' }
    & git push origin $ReleaseTag
    if ($LASTEXITCODE -ne 0) { throw "Could not push tag $ReleaseTag." }

    $PreviousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & gh release view $ReleaseTag --repo $Repository *> $null
    $ReleaseViewExitCode = $LASTEXITCODE
    $ErrorActionPreference = $PreviousErrorActionPreference
    $ReleaseExists = $ReleaseViewExitCode -eq 0
    New-Item -ItemType Directory -Path $StagingDirectory | Out-Null
    foreach ($Asset in $ReleaseAssets) {
        Copy-Item -LiteralPath (Join-Path $OutputDirectory $Asset) -Destination $StagingDirectory
    }
    Set-Content -LiteralPath (Join-Path $StagingDirectory 'CHANGELOG.md') `
        -Value "## [$Version]`r`n`r`n$ReleaseNotes" -Encoding UTF8
    $ReleaseNotesPath = Join-Path $StagingDirectory 'release-notes.md'
    Set-Content -LiteralPath $ReleaseNotesPath -Value $ReleaseNotes -Encoding UTF8
    $ReleaseAssetPaths = @($ReleaseAssets | ForEach-Object { Join-Path $StagingDirectory $_ })

    if ($ReleaseExists) {
        $UploadArguments = @('release', 'upload', $ReleaseTag) + $ReleaseAssetPaths + @('--repo', $Repository, '--clobber')
        & gh @UploadArguments
        if ($LASTEXITCODE -ne 0) { throw 'GitHub release asset upload failed.' }
        & gh release edit $ReleaseTag --repo $Repository --title $ReleaseTitle --notes-file $ReleaseNotesPath
    } else {
        $CreateArguments = @('release', 'create', $ReleaseTag) + $ReleaseAssetPaths + @(
            '--repo', $Repository,
            '--title', $ReleaseTitle,
            '--notes-file', $ReleaseNotesPath
        )
        & gh @CreateArguments
    }
    if ($LASTEXITCODE -ne 0) { throw 'GitHub release publication failed.' }
    Remove-Item -LiteralPath $ReleaseNotesPath

    if ($CurrentLatestDirectory -and $CurrentLatestDirectory.FullName -eq $TargetDirectory) {
        foreach ($Asset in $ReleaseAssets + @('CHANGELOG.md')) {
            Move-Item -LiteralPath (Join-Path $StagingDirectory $Asset) `
                -Destination $TargetDirectory -Force
        }
        Remove-Item -LiteralPath $StagingDirectory
    } else {
        $FormerVersionDirectory = $null
        if ($CurrentLatestDirectory) {
            $CurrentVersionName = $CurrentLatestDirectory.Name -replace ' \(latest\)$', ''
            $FormerVersionDirectory = Join-Path $OutputDirectory $CurrentVersionName
            Move-Item -LiteralPath $CurrentLatestDirectory.FullName -Destination $FormerVersionDirectory
        }
        try {
            Move-Item -LiteralPath $StagingDirectory -Destination $TargetDirectory
        } catch {
            if ($FormerVersionDirectory -and (Test-Path -LiteralPath $FormerVersionDirectory)) {
                Move-Item -LiteralPath $FormerVersionDirectory -Destination $CurrentLatestDirectory.FullName
            }
            throw
        }
    }

    Write-Output "Published $ReleaseTag to https://github.com/$Repository/releases/tag/$ReleaseTag"
    Write-Output "Release files saved to output\$Version (latest)."
} finally {
    Pop-Location
}
