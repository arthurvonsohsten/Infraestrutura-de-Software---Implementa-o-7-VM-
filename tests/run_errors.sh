#!/bin/bash
# Roda o vm com entradas inválidas e mostra a mensagem e o código de saída.
# Uso (na pasta do projeto, depois do make): bash tests/run_errors.sh

testar() {
    echo "\$ ./vm $*"
    ./vm "$@"
    echo "-> código de saída: $?"
    echo
}

echo "### (1) Número e tipo dos argumentos"
testar
testar addresses.txt lru
testar addresses.txt lru fifo extra
testar addresses.txt LRU fifo
testar addresses.txt lru abc

echo "### (2) Arquivo de endereços"
testar naoexiste.txt lru fifo
testar tests lru fifo
testar tests/empty.txt lru fifo
testar tests/bad_text.txt lru fifo
testar tests/bad_neg.txt lru fifo
testar tests/bad_range.txt lru fifo
testar tests/bad_two.txt lru fifo
