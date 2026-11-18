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
#include <string.h>

/* Parâmetros do sistema */
#define TAM_PAGINA    256   /* bytes por página (= bytes por frame) */
#define NUM_PAGINAS   256   /* páginas no espaço lógico (2^8)       */
#define TAM_TLB       16    /* entradas da TLB                      */
#ifndef NUM_FRAMES
#define NUM_FRAMES    128   /* frames da memória física             */
#endif

#define ARQ_DISCO     "BACKING_STORE.bin"
#define ARQ_SAIDA     "correct.txt"

int alg_pagina_lru, alg_tlb_lru;   /* 1 = lru, 0 = fifo */

/* ------------------------------------------------------------------ */
/* Validação da entrada                                                */
/* ------------------------------------------------------------------ */

void mostrar_uso(void) {
    fprintf(stderr, "Uso: ./vm <arquivo_enderecos> <fifo|lru> <fifo|lru>\n");
    fprintf(stderr, "Exemplo: ./vm address.txt lru fifo\n");
}

/* Retorna 1 se for lru, 0 se for fifo e -1 se for inválido. */
int ler_algoritmo(const char *nome) {
    if (strcmp(nome, "lru") == 0) return 1;
    if (strcmp(nome, "fifo") == 0) return 0;
    return -1;
}

/*
 * Confere se cada linha do arquivo tem apenas um número entre 0 e 65535.
 * Linhas vazias são ignoradas. Retorna a quantidade de endereços ou -1.
 */
int validar_arquivo(FILE *arq, const char *nome) {
    char linha[64];
    int num_linha = 0, total = 0;

    while (fgets(linha, sizeof linha, arq) != NULL) {
        num_linha++;
        linha[strcspn(linha, "\r\n")] = '\0';   /* tira a quebra de linha */

        if (linha[0] == '\0')
            continue;

        int tam = strlen(linha);
        for (int i = 0; i < tam; i++) {
            if (linha[i] < '0' || linha[i] > '9') {
                fprintf(stderr, "Erro: '%s' linha %d: \"%s\" não é um endereço válido.\n",
                        nome, num_linha, linha);
                return -1;
            }
        }
        if (tam > 5 || atoi(linha) > 65535) {
            fprintf(stderr, "Erro: '%s' linha %d: endereço %s fora do intervalo 0-65535.\n",
                    nome, num_linha, linha);
            return -1;
        }
        total++;
    }

    if (ferror(arq)) {
        fprintf(stderr, "Erro: não foi possível ler '%s'.\n", nome);
        return -1;
    }
    if (total == 0) {
        fprintf(stderr, "Erro: '%s' não contém endereços.\n", nome);
        return -1;
    }
    return total;
}

/* ------------------------------------------------------------------ */

int main(int argc, char *argv[]) {
    /* (1) número e tipo dos argumentos */
    if (argc != 4) {
        fprintf(stderr, "Erro: são necessários 3 argumentos.\n");
        mostrar_uso();
        return 1;
    }
    alg_pagina_lru = ler_algoritmo(argv[2]);
    alg_tlb_lru = ler_algoritmo(argv[3]);
    if (alg_pagina_lru == -1 || alg_tlb_lru == -1) {
        fprintf(stderr, "Erro: os algoritmos devem ser fifo ou lru.\n");
        mostrar_uso();
        return 1;
    }

    /* (2) arquivo de endereços */
    FILE *entrada = fopen(argv[1], "r");
    if (entrada == NULL) {
        fprintf(stderr, "Erro: não foi possível abrir '%s'.\n", argv[1]);
        return 1;
    }
    int total = validar_arquivo(entrada, argv[1]);
    if (total == -1) {
        fclose(entrada);
        return 1;
    }
    rewind(entrada);

    FILE *disco = fopen(ARQ_DISCO, "rb");
    if (disco == NULL) {
        fprintf(stderr, "Erro: não foi possível abrir '%s'.\n", ARQ_DISCO);
        fclose(entrada);
        return 1;
    }

    fclose(entrada);
    fclose(disco);
    return 0;
}
