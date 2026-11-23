# Gerenciador de Memória Virtual

Implementação em C do projeto *Designing a Virtual Memory Manager* (Silberschatz et al., *Operating System Concepts*, 10ª ed., P-51), com as modificações pedidas na disciplina de Infraestrutura de Software:

- memória física com **128 frames**, preenchidos do 0 ao 127;
- substituição de página **FIFO** ou **LRU**;
- TLB de 16 entradas com substituição **FIFO** ou **LRU**;
- busca na TLB com **uma thread para cada entrada**.

## Compilação

```bash
make          # gera o executável vm
make clean    # apaga o executável e o correct.txt
```

## Execução

```bash
./vm <arquivo_enderecos> <alg_pagina> <alg_tlb>
```

| Argumento | Valores | Descrição |
|-----------|---------|-----------|
| `arquivo_enderecos` | ex.: `address.txt` | um endereço lógico (0 a 65535) por linha |
| `alg_pagina` | `fifo` ou `lru` | substituição de página na memória física |
| `alg_tlb` | `fifo` ou `lru` | substituição de entrada na TLB |

Exemplo (página LRU, TLB FIFO):

```bash
./vm address.txt lru fifo
```

O arquivo `BACKING_STORE.bin` precisa estar na mesma pasta. O resultado é gravado em `correct.txt`:

```
Virtual address: 16916 TLB: 0 Physical address: 20 Value: 0
...
Number of Translated Addresses = 1000
Page Faults = 539
Page Fault Rate = 0.539
TLB Hits = 54
TLB Hit Rate = 0.054
```

`TLB` é a posição da TLB onde a página foi encontrada (hit) ou inserida (miss).

## Como funciona

- **Tradução:** página = `endereço >> 8` e offset = `endereço & 0xFF`; endereço físico = `frame * 256 + offset`.
- **Fluxo:** TLB → tabela de páginas → page fault (lê a página do `BACKING_STORE.bin`).
- **FIFO e LRU:** cada frame e cada entrada da TLB guarda um `tempo`; a vítima é a de menor tempo. No FIFO o tempo é marcado só quando a página entra; no LRU ele é atualizado a cada acesso (inclusive quando há TLB hit).
- Quando uma página sai da memória, sua entrada na TLB é invalidada.
- **Threads:** a cada busca, são criadas 16 threads; a thread *i* compara a entrada *i* da TLB com a página procurada.

## Tratamento de erros

O programa mostra uma mensagem e encerra com código 1 quando:

1. o número de argumentos não é 3, ou algum algoritmo não é `fifo`/`lru`;
2. o arquivo de endereços não existe, não pode ser aberto, está vazio ou tem alguma linha que não seja um único inteiro entre 0 e 65535.

## Testes e evidências

```bash
bash tests/run_errors.sh                              # casos de erro
bash tests/gerar_evidencias.sh > evidencias.log 2>&1  # todos os fluxos
```

Para testar a memória com 256 frames (sem substituição):

```bash
gcc -Wall -pthread -DNUM_FRAMES=256 -o vm vm.c
```

## Arquivos

| Arquivo | Conteúdo |
|---------|----------|
| `vm.c` | código-fonte |
| `Makefile` | compilação (`make`) e limpeza (`make clean`) |
| `evidencias.log` | saída de todos os fluxos e testes |
| `tests/` | entradas inválidas e scripts de teste |
| `relatorio.pdf` | relatório |
