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

/* Memória física e tabela de páginas */
signed char memoria[NUM_FRAMES][TAM_PAGINA];
int tabela_paginas[NUM_PAGINAS];   /* página -> frame (-1 = fora da memória) */
int pagina_do_frame[NUM_FRAMES];   /* frame -> página que está nele          */
int tempo_frame[NUM_FRAMES];       /* hora em que a página foi carregada      */
int frames_usados = 0;

int relogio = 0;   /* contador que avança a cada endereço traduzido */

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
/* Memória                                                             */
/* ------------------------------------------------------------------ */

/* Escolhe o frame de menor tempo (FIFO: o carregado há mais tempo). */
int escolher_vitima(void) {
    int vitima = 0;
    for (int f = 1; f < NUM_FRAMES; f++)
        if (tempo_frame[f] < tempo_frame[vitima])
            vitima = f;
    return vitima;
}

/* Trata o page fault: carrega a página do disco num frame e devolve o frame. */
int carregar_pagina(FILE *disco, int pagina) {
    int frame;
    if (frames_usados < NUM_FRAMES) {
        frame = frames_usados++;            /* preenche do 0 ao 127 */
    } else {
        frame = escolher_vitima();          /* memória cheia: substitui */
        tabela_paginas[pagina_do_frame[frame]] = -1;
    }

    fseek(disco, pagina * TAM_PAGINA, SEEK_SET);
    fread(memoria[frame], 1, TAM_PAGINA, disco);

    tabela_paginas[pagina] = frame;
    pagina_do_frame[frame] = pagina;
    tempo_frame[frame] = relogio;
    return frame;
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
    FILE *saida = fopen(ARQ_SAIDA, "w");
    if (saida == NULL) {
        fprintf(stderr, "Erro: não foi possível criar '%s'.\n", ARQ_SAIDA);
        fclose(entrada);
        fclose(disco);
        return 1;
    }

    for (int p = 0; p < NUM_PAGINAS; p++)
        tabela_paginas[p] = -1;

    int endereco, page_faults = 0;

    while (fscanf(entrada, "%d", &endereco) == 1) {
        relogio++;
        int pagina = endereco >> 8;        /* 8 bits mais altos  */
        int offset = endereco & 0xFF;      /* 8 bits mais baixos */

        int frame = tabela_paginas[pagina];
        if (frame == -1) {                 /* page fault */
            page_faults++;
            frame = carregar_pagina(disco, pagina);
        }

        int fisico = frame * TAM_PAGINA + offset;
        fprintf(saida, "Virtual address: %d Physical address: %d Value: %d\n",
                endereco, fisico, memoria[frame][offset]);
    }

    fprintf(saida, "Number of Translated Addresses = %d\n", total);
    fprintf(saida, "Page Faults = %d\n", page_faults);
    fprintf(saida, "Page Fault Rate = %.3f", (double)page_faults / total);

    fclose(entrada);
    fclose(disco);
    fclose(saida);
    return 0;
}
