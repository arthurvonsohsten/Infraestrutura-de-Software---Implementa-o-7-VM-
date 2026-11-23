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
int tempo_frame[NUM_FRAMES];       /* FIFO: hora da carga | LRU: último uso   */
int frames_usados = 0;

/* TLB */
typedef struct {
    int pagina;   /* -1 = entrada vazia */
    int frame;
    int tempo;    /* FIFO: hora da inserção | LRU: último uso */
} EntradaTLB;

EntradaTLB tlb[TAM_TLB];

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
/* TLB                                                                 */
/* ------------------------------------------------------------------ */

/* Retorna o índice da página na TLB ou -1 (TLB miss). */
int buscar_tlb(int pagina) {
    for (int i = 0; i < TAM_TLB; i++)
        if (tlb[i].pagina == pagina)
            return i;
    return -1;
}

/* Insere a página na TLB: usa uma entrada vazia ou substitui a de menor
 * tempo (a mais antiga no FIFO, a menos usada recentemente no LRU). */
int inserir_tlb(int pagina, int frame) {
    int escolhida = 0;
    for (int i = 0; i < TAM_TLB; i++) {
        if (tlb[i].pagina == -1) { escolhida = i; break; }
        if (tlb[i].tempo < tlb[escolhida].tempo) escolhida = i;
    }
    tlb[escolhida].pagina = pagina;
    tlb[escolhida].frame = frame;
    tlb[escolhida].tempo = relogio;
    return escolhida;
}

/* Remove da TLB a página que saiu da memória. */
void remover_tlb(int pagina) {
    for (int i = 0; i < TAM_TLB; i++)
        if (tlb[i].pagina == pagina)
            tlb[i].pagina = -1;
}

/* ------------------------------------------------------------------ */
/* Memória                                                             */
/* ------------------------------------------------------------------ */

/* Escolhe o frame de menor tempo (FIFO: o carregado há mais tempo;
 * LRU: o usado há mais tempo). */
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
        int antiga = pagina_do_frame[frame];
        tabela_paginas[antiga] = -1;
        remover_tlb(antiga);
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
    for (int i = 0; i < TAM_TLB; i++)
        tlb[i].pagina = -1;

    int endereco, page_faults = 0, tlb_hits = 0;

    while (fscanf(entrada, "%d", &endereco) == 1) {
        relogio++;
        int pagina = endereco >> 8;        /* 8 bits mais altos  */
        int offset = endereco & 0xFF;      /* 8 bits mais baixos */
        int frame;

        int indice = buscar_tlb(pagina);
        if (indice != -1) {                /* TLB hit */
            tlb_hits++;
            frame = tlb[indice].frame;
            if (alg_tlb_lru)
                tlb[indice].tempo = relogio;
        } else {                           /* TLB miss: consulta a tabela */
            frame = tabela_paginas[pagina];
            if (frame == -1) {             /* page fault */
                page_faults++;
                frame = carregar_pagina(disco, pagina);
            }
            indice = inserir_tlb(pagina, frame);
        }

        if (alg_pagina_lru)                /* LRU: marca o último uso */
            tempo_frame[frame] = relogio;

        int fisico = frame * TAM_PAGINA + offset;
        fprintf(saida, "Virtual address: %d TLB: %d Physical address: %d Value: %d\n",
                endereco, indice, fisico, memoria[frame][offset]);
    }

    fprintf(saida, "Number of Translated Addresses = %d\n", total);
    fprintf(saida, "Page Faults = %d\n", page_faults);
    fprintf(saida, "Page Fault Rate = %.3f\n", (double)page_faults / total);
    fprintf(saida, "TLB Hits = %d\n", tlb_hits);
    fprintf(saida, "TLB Hit Rate = %.3f", (double)tlb_hits / total);

    fclose(entrada);
    fclose(disco);
    fclose(saida);
    return 0;
}
