param(
    [Parameter(Mandatory = $true)]
    [string]$InputFile,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z_][A-Za-z0-9_]*$')]
    [string]$SymbolName
)

$resolvedInput = (Resolve-Path -LiteralPath $InputFile).Path
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
[System.IO.Directory]::CreateDirectory($resolvedOutput) | Out-Null

$bytes = [System.IO.File]::ReadAllBytes($resolvedInput)
$headerPath = Join-Path $resolvedOutput "$SymbolName.h"
$sourcePath = Join-Path $resolvedOutput "$SymbolName.c"
$guard = ($SymbolName.ToUpperInvariant() + '_H')

$header = @"
#ifndef $guard
#define $guard

#include <stdint.h>

extern const uint8_t ${SymbolName}_data[];
extern const uint32_t ${SymbolName}_size;

#endif /* $guard */
"@

$builder = [System.Text.StringBuilder]::new()
[void]$builder.AppendLine("#include `"$SymbolName.h`"")
[void]$builder.AppendLine()
[void]$builder.AppendLine("const uint8_t ${SymbolName}_data[] = {")

for ($i = 0; $i -lt $bytes.Length; $i += 12) {
    [void]$builder.Append('    ')
    $lineEnd = [Math]::Min($i + 12, $bytes.Length)

    for ($j = $i; $j -lt $lineEnd; $j++) {
        [void]$builder.Append(('0x{0:X2}U,' -f $bytes[$j]))
        if ($j + 1 -lt $lineEnd) {
            [void]$builder.Append(' ')
        }
    }

    [void]$builder.AppendLine()
}

[void]$builder.AppendLine('};')
[void]$builder.AppendLine()
[void]$builder.AppendLine("const uint32_t ${SymbolName}_size = sizeof(${SymbolName}_data);")

[System.IO.File]::WriteAllText($headerPath, $header, [System.Text.UTF8Encoding]::new($false))
[System.IO.File]::WriteAllText($sourcePath, $builder.ToString(), [System.Text.UTF8Encoding]::new($false))

Write-Output "Generated $sourcePath ($($bytes.Length) audio bytes)"
