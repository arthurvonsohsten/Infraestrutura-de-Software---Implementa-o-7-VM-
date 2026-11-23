#!/bin/bash
# Gera o evidencias.log com a execução de todos os fluxos do programa.
# Uso (na pasta do projeto): bash tests/gerar_evidencias.sh > evidencias.log 2>&1
# Precisa de addresses.txt e BACKING_STORE.bin na pasta do projeto.

cd "$(dirname "$0")/.." || exit 1
GABARITO=ref/correct.txt   # correct.txt fornecido pelo professor (lru fifo)

echo "===== Ambiente ====="
date
uname -sr
gcc --version | head -1
echo

echo "===== Compilação ====="
make clean
make
echo

echo "===== Tratamento de erros ====="
bash tests/run_errors.sh

echo "===== Caso limite: endereços 0, 255, 256 e 65535 ====="
./vm tests/limites.txt lru fifo
cat correct.txt; echo
echo

echo "===== Tabela de páginas com 256 frames (compilado com -DNUM_FRAMES=256) ====="
gcc -Wall -Wextra -pthread -DNUM_FRAMES=256 -o vm256 vm.c
./vm256 addresses.txt lru fifo
head -3 correct.txt
tail -5 correct.txt; echo
rm -f vm256
echo

for pagina in fifo lru; do
    for tlb in fifo lru; do
        echo "===== 128 frames: página $pagina, TLB $tlb ====="
        echo "\$ ./vm addresses.txt $pagina $tlb"
        ./vm addresses.txt $pagina $tlb
        head -3 correct.txt
        tail -5 correct.txt; echo
        echo
    done
done

echo "===== Comparação com o correct.txt do professor (lru fifo) ====="
./vm addresses.txt lru fifo
if [ -f "$GABARITO" ]; then
    # ignora BOM e \r que o arquivo do professor pode ter
    if diff <(sed '1s/^\xEF\xBB\xBF//' "$GABARITO" | tr -d '\r') correct.txt > /dev/null; then
        echo "correct.txt IDÊNTICO ao do professor"
    else
        echo "correct.txt DIFERENTE do do professor:"
        diff <(sed '1s/^\xEF\xBB\xBF//' "$GABARITO" | tr -d '\r') correct.txt | head -20
    fi
else
    echo "(gabarito $GABARITO não encontrado)"
fi
echo

echo "===== Busca na TLB com threads (funções pthread usadas pelo vm) ====="
nm -D vm | grep pthread
