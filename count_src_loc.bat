@echo off
setlocal
pushd "%~dp0"
if not exist "src\" (
  echo src folder not found
  popd
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -NoLogo -Command ^
  "& { $repo = (Get-Location).Path; $ext = @('.cpp','.hpp'); $total = 0; $rows = New-Object System.Collections.Generic.List[object]; Get-ChildItem -LiteralPath 'src' -Recurse -File | Where-Object { $ext -contains $_.Extension.ToLowerInvariant() } | ForEach-Object { $p = $_.FullName; $n = 0; try { $n = (Get-Content -LiteralPath $p -ErrorAction Stop | Where-Object { $_ -match '\S' }).Count } catch { $n = 0 }; [void]$rows.Add([pscustomobject]@{ Lines = $n; P = $p }); $total += $n }; $rows | Sort-Object Lines -Descending | ForEach-Object { $rel = $_.P.Substring($repo.Length).TrimStart('\'); Write-Output ('{0,8}  {1}' -f $_.Lines, $rel) }; Write-Output ''; Write-Output ('Total lines with non-whitespace (src, .cpp/.hpp): {0}' -f $total) }"

popd
endlocal
