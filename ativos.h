#ifndef ATIVOS_H
#define ATIVOS_H

#include <stdio.h>

#define DADOS_FILE "ativos_inventario.bin"

#pragma pack(push, 1)
typedef struct {
    int patrimonio_id;         // 4 bytes (Chave Primária se >= 0, ou Ponteiro da LED se < 0)
    char tipo_equipamento[20]; // 20 bytes (ex: "Notebook", "Monitor")
    char setor_alocacao[20];   // 20 bytes (ex: "TI", "Financeiro")
    char marca_modelo[40];     // 40 bytes (ex: "Dell Latitude 3420")
    float valor_compra;        // 4 bytes (ex: 4500.50)
} Ativo;
#pragma pack(pop)

static inline void lerAtivo(FILE *fp, int rrn, Ativo *ativo) {
    long offset = sizeof(int) + rrn * sizeof(Ativo);
    fseek(fp, offset, SEEK_SET);
    fread(ativo, sizeof(Ativo), 1, fp);
}

static inline void escreverAtivo(FILE *fp, int rrn, Ativo *ativo) {
    long offset = sizeof(int) + rrn * sizeof(Ativo);
    fseek(fp, offset, SEEK_SET);
    fwrite(ativo, sizeof(Ativo), 1, fp);
}

#endif // ATIVOS_H
