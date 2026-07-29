$ErrorActionPreference = "SilentlyContinue"

Write-Host "正在检查 64 位 MySQL ODBC 驱动..." -ForegroundColor Cyan

$drivers = @()
$paths = @(
    "HKLM:\SOFTWARE\ODBC\ODBCINST.INI",
    "HKCU:\SOFTWARE\ODBC\ODBCINST.INI"
)

foreach ($path in $paths) {
    $items = Get-ChildItem $path -ErrorAction SilentlyContinue |
        Where-Object { $_.PSChildName -like "*MySQL*" }
    foreach ($item in $items) {
        $drivers += $item.PSChildName
    }
}

if ($drivers.Count -eq 0) {
    Write-Host "没有找到 MySQL ODBC 驱动。" -ForegroundColor Yellow
    Write-Host "请安装 64 位 MySQL Connector/ODBC 8.0 或 8.4，然后重新运行本脚本。"
    exit 1
}

Write-Host "找到以下 MySQL ODBC 驱动：" -ForegroundColor Green
$drivers | ForEach-Object { Write-Host "  $_" }
Write-Host ""
Write-Host "请把 config/server.example.ini 中的 odbcDriver 改成上面其中一个完整名称。"
