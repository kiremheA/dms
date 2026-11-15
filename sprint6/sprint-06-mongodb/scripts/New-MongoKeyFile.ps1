[CmdletBinding()]
param(
    [string]$OutputPath = (Join-Path $PSScriptRoot "..\secrets\mongo-keyfile")
)

$resolvedOutputPath = [System.IO.Path]::GetFullPath($OutputPath)

if (Test-Path -LiteralPath $resolvedOutputPath) {
    throw "Keyfile already exists: $resolvedOutputPath"
}

$outputDirectory = [System.IO.Path]::GetDirectoryName($resolvedOutputPath)
[System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null

$keyBytes = New-Object byte[] 756
$randomGenerator = [System.Security.Cryptography.RandomNumberGenerator]::Create()

try {
    $randomGenerator.GetBytes($keyBytes)
}
finally {
    $randomGenerator.Dispose()
}

$keyContent = [System.Convert]::ToBase64String($keyBytes)
$utf8WithoutBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($resolvedOutputPath, $keyContent, $utf8WithoutBom)

Write-Host "Created MongoDB keyfile: $resolvedOutputPath"
