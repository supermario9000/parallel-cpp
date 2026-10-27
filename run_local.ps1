# Lokalus kompiliavimas ir eksperimentai (LD1)
#   .\run_local.ps1 -DP 5000 -CL 50 -Repeats 3
# Rezultatai: results_local.csv (threads,pm,numDP,numCL,load,matrix,search,total)

param(
    [int]$DP = 5000,
    [int]$CL = 50,
    [int[]]$Threads = @(1, 2, 4),
    [int]$Repeats = 1
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

g++ -O2 -std=c++17 -fopenmp main.cpp -o flp_omp.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

"threads,pm,numDP,numCL,load,matrix,search,total" | Out-File -Encoding utf8 results_local.csv

foreach ($pm in 0, 1) {
    foreach ($t in $Threads) {
        for ($r = 1; $r -le $Repeats; $r++) {
            $out = & .\flp_omp.exe -t $t -dp $DP -cl $CL -pm $pm
            $out | Where-Object { $_ -notlike "CSV,*" } | Write-Host
            $out | Where-Object { $_ -like "CSV,*" } | ForEach-Object { $_.Substring(4) } |
                Out-File -Append -Encoding utf8 results_local.csv
        }
    }
}
