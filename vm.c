/*
 * vm.c - Gerenciador de Memória Virtual
 * Baseado no projeto "Designing a Virtual Memory Manager"
 * (Silberschatz et al., Operating System Concepts, 10a ed., P-51).
 *
 * Uso: ./vm <arquivo_enderecos> <fifo|lru> <fifo|lru>
 *        argv[2] -> algoritmo de substituição de página
 *        argv[3] -> algoritmo de substituição da TLB
 */

#include <stdio.h>
#include <stdlib.h>

/* Parâmetros do sistema */
#define PAGE_SIZE        256   /* bytes por página = bytes por frame  */
#define PAGE_TABLE_SIZE  256   /* 2^8 páginas no espaço lógico        */
#define NUM_FRAMES       128   /* frames da memória física            */
#define TLB_SIZE         16    /* entradas da TLB                     */

#define BACKING_STORE    "BACKING_STORE.bin"
#define OUTPUT_FILE      "correct.txt"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    return EXIT_SUCCESS;
}
