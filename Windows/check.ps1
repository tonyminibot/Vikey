# Engine lock on Windows: builds the test tools with MSVC (CMake + Ninja) and runs the same checks as
# Tools/autoswitch/check.sh (rule cases, golden snapshots, Telex lock, metrics, driver parity, performance).
# usage: pwsh Windows/check.ps1 [update]
$ErrorActionPreference = 'Stop'
$env:PYTHONUTF8 = '1'          # Python subprocess text I/O must be UTF-8, not the console code page
$env:PYTHONIOENCODING = 'utf-8'
$root = Resolve-Path "$PSScriptRoot/.."
$as = "$root/Tools/autoswitch"
cmake -S "$root/Windows" -B "$root/Windows/build" -G Ninja -DCMAKE_BUILD_TYPE=Release | Out-Null
cmake --build "$root/Windows/build" --config Release
if ($LASTEXITCODE) { exit 1 }
$rc = 0
& "$root/Tools/text_expansion/text_expansion_test.exe"; if ($LASTEXITCODE) { $rc = 1 }
& "$root/Tools/text_expansion/text_expansion_io_test.exe"; if ($LASTEXITCODE) { $rc = 1 }
if (-not $env:TMPDIR) { $env:TMPDIR = $env:TEMP }
python "$as/rules/run_rules.py"; if ($LASTEXITCODE) { $rc = 1 }
$mode = if ($args.Count -gt 0) { $args[0] } else { 'check' }
python "$as/rules/golden.py" $mode; if ($LASTEXITCODE) { $rc = 1 }
$u = Join-Path $env:TMPDIR 'vikey_check_chat.units'
# write the units file from Python itself (a PowerShell pipeline would re-encode the Vietnamese text)
$prep = ($as -replace '\\', '/') + '/sim/prep.py'; $corpus = ($as -replace '\\', '/') + '/sim/corpus/chat_messages.txt'; $uu = $u -replace '\\', '/'
python -c "import runpy, sys, io; sys.stdout = io.TextIOWrapper(open(r'$uu', 'wb'), encoding='utf-8', newline='\n'); sys.argv = ['prep.py', r'$corpus']; runpy.run_path(r'$prep', run_name='__main__'); sys.stdout.flush()"
# driver == simulator (what cmp.sh does, without zsh)
$env:DRV_OUT = Join-Path $env:TMPDIR 'drv.txt'
& "$as/driver/drvlazy.exe" $u --start vi | Out-Null
& "$as/sim/sim.exe" $u --start vi --lazy --dump (Join-Path $env:TMPDIR 'simdump.txt') | Out-Null
$cmp = @"
import os, re
t = os.environ['TMPDIR']
d = open(os.path.join(t, 'drv.txt'), encoding='utf-8').read().split('\n')
s = [l.split('\t')[0] for l in open(os.path.join(t, 'simdump.txt'), encoding='utf-8').read().rstrip('\n').split('\n')]
D = re.sub(r'[\W\d_]+', '', ''.join(d)); S = re.sub(r'[\W\d_]+', '', ''.join(s))
print('driver == simulator:', 'ok' if D == S else 'FAIL', len(D), len(S))
raise SystemExit(0 if D == S else 1)
"@
python -c $cmp; if ($LASTEXITCODE) { $rc = 1 }
& "$as/driver/bench.exe" $u 1000 20000; if ($LASTEXITCODE) { Write-Host 'FAIL performance budget'; $rc = 1 } else { Write-Host 'performance: ok' }
if ($rc) { Write-Host 'CHECK FAILED'; exit 1 } else { Write-Host 'ALL CHECKS PASSED' }
