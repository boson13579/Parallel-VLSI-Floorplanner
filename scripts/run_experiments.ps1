param(
    [string]$Input = "testcase\\case1.block",
    [string]$OutputDir = "results",
    [int[]]$Threads = @(1,2,4,8)
)

$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$strategies = @(
    "MultiStart_Coarse",
    "ParallelTempering_Medium",
    "ParallelMoves_Fine"
)

foreach ($p in $Threads) {
    $env:OMP_NUM_THREADS = "$p"
    foreach ($s in $strategies) {
        Write-Host "Running strategy=$s, threads=$p" -ForegroundColor Cyan
        $out = Join-Path $OutputDir ("${s}_p${p}.out.block")
        & .\floorplanner -i $Input -o $out 2>&1 | Tee-Object -FilePath (Join-Path $OutputDir ("${s}_p${p}.log"))
        Copy-Item convergence_log.csv (Join-Path $OutputDir ("convergence_${s}_p${p}.csv")) -Force
    }
}
