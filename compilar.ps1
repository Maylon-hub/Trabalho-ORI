# Script de compilação e execução para o teste do índice secundário e lista invertida
Write-Host "Compilando teste_secundario.cpp..." -ForegroundColor Cyan
g++ -O3 -std=c++17 teste_secundario.cpp -o teste_secundario.exe

if ($LASTEXITCODE -eq 0) {
    Write-Host "[Sucesso] Compilado com sucesso!" -ForegroundColor Green
    Write-Host "Executando o teste do indice secundario..." -ForegroundColor Yellow
    Write-Host "---------------------------------------------------------"
    .\teste_secundario.exe
} else {
    Write-Host "[Erro] Falha ao compilar teste_secundario.cpp. Verifique se o g++ esta no PATH." -ForegroundColor Red
}
