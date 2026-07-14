#ifndef LISTA_INVERTIDA_HPP
#define LISTA_INVERTIDA_HPP

#include <iostream>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

#pragma pack(push, 1)
struct RegistroSecundario {
    char chave[20];       // Valor do campo indexado (ex: "Notebook" ou "TI")
    int primeiro_rrn;    // RRN correspondente na Lista Invertida
};

struct RegistroLista {
    int patrimonio_id;   // Chave primária do Ativo
    int proximo_rrn;     // RRN do próximo ativo com a mesma chave secundária (-1 para fim)
};
#pragma pack(pop)

// Inicializa os arquivos de índice secundário e lista invertida
inline void inicializar_indice_secundario(const char* arq_sec, const char* arq_list) {
    // Tenta abrir o índice secundário
    FILE* fs = fopen(arq_sec, "rb");
    if (!fs) {
        fs = fopen(arq_sec, "wb");
        if (fs) fclose(fs);
    } else {
        fclose(fs);
    }

    // Tenta abrir a lista invertida
    FILE* fl = fopen(arq_list, "rb");
    if (!fl) {
        fl = fopen(arq_list, "wb");
        if (fl) fclose(fl);
    } else {
        fclose(fl);
    }
}

// Lê todas as chaves secundárias ordenadas na memória
inline std::vector<RegistroSecundario> ler_indice_secundario(const char* arq_sec) {
    std::vector<RegistroSecundario> entries;
    FILE* f = fopen(arq_sec, "rb");
    if (!f) return entries;

    RegistroSecundario reg;
    while (fread(&reg, sizeof(RegistroSecundario), 1, f) == 1) {
        entries.push_back(reg);
    }
    fclose(f);
    return entries;
}

// Salva as chaves secundárias de volta no arquivo
inline void salvar_indice_secundario(const char* arq_sec, const std::vector<RegistroSecundario>& entries) {
    FILE* f = fopen(arq_sec, "wb");
    if (!f) return;
    for (const auto& entry : entries) {
        fwrite(&entry, sizeof(RegistroSecundario), 1, f);
    }
    fclose(f);
}

// Insere uma nova associação (chave_secundaria -> patrimonio_id)
inline void inserir_indice_secundario(const char* arq_sec, const char* arq_list, const char* chave, int patrimonio_id) {
    // 1. Ler o índice secundário atual
    auto entries = ler_indice_secundario(arq_sec);
    
    // Limpar e padronizar a chave de busca (tamanho máx 19 + null)
    char chave_padrao[20] = {0};
    std::strncpy(chave_padrao, chave, 19);

    // 2. Gravar o novo registro na lista invertida
    FILE* fl = fopen(arq_list, "rb+");
    if (!fl) {
        fl = fopen(arq_list, "wb+");
        if (!fl) return;
    }
    fseek(fl, 0, SEEK_END);
    long size_list = ftell(fl);
    int novo_rrn_lista = size_list / sizeof(RegistroLista);

    // Procurar se a chave já existe no índice secundário
    int idx_chave = -1;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (std::strcmp(entries[i].chave, chave_padrao) == 0) {
            idx_chave = i;
            break;
        }
    }

    RegistroLista novo_nodo;
    novo_nodo.patrimonio_id = patrimonio_id;

    if (idx_chave != -1) {
        // A chave existe: o novo nodo aponta para o atual início da lista
        novo_nodo.proximo_rrn = entries[idx_chave].primeiro_rrn;
        // O início da lista passa a ser o novo RRN
        entries[idx_chave].primeiro_rrn = novo_rrn_lista;
    } else {
        // Chave não existe: o novo nodo é o único da lista
        novo_nodo.proximo_rrn = -1;
        
        // Adiciona a nova chave no índice secundário
        RegistroSecundario nova_entrada;
        std::strcpy(nova_entrada.chave, chave_padrao);
        nova_entrada.primeiro_rrn = novo_rrn_lista;
        entries.push_back(nova_entrada);

        // Ordena as chaves alfabeticamente
        std::sort(entries.begin(), entries.end(), [](const RegistroSecundario& a, const RegistroSecundario& b) {
            return std::strcmp(a.chave, b.chave) < 0;
        });
    }

    // Escreve o nodo na lista invertida
    fseek(fl, (long)novo_rrn_lista * sizeof(RegistroLista), SEEK_SET);
    fwrite(&novo_nodo, sizeof(RegistroLista), 1, fl);
    fclose(fl);

    // Salva o índice secundário ordenado
    salvar_indice_secundario(arq_sec, entries);
}

// Busca todos os IDs associados a uma chave secundária
inline std::vector<int> buscar_indice_secundario(const char* arq_sec, const char* arq_list, const char* chave) {
    std::vector<int> resultados;
    auto entries = ler_indice_secundario(arq_sec);

    char chave_padrao[20] = {0};
    std::strncpy(chave_padrao, chave, 19);

    int primeiro_rrn = -1;
    for (const auto& entry : entries) {
        if (std::strcmp(entry.chave, chave_padrao) == 0) {
            primeiro_rrn = entry.primeiro_rrn;
            break;
        }
    }

    if (primeiro_rrn == -1) {
        return resultados; // Nenhuma chave correspondente encontrada
    }

    FILE* fl = fopen(arq_list, "rb");
    if (!fl) return resultados;

    int rrn_atual = primeiro_rrn;
    while (rrn_atual != -1) {
        fseek(fl, (long)rrn_atual * sizeof(RegistroLista), SEEK_SET);
        RegistroLista nodo;
        if (fread(&nodo, sizeof(RegistroLista), 1, fl) != 1) {
            break; // Fim do arquivo ou erro de leitura
        }
        resultados.push_back(nodo.patrimonio_id);
        rrn_atual = nodo.proximo_rrn;
    }
    fclose(fl);
    return resultados;
}

// Remove a associação (chave -> patrimonio_id) na lista invertida
inline bool remover_indice_secundario(const char* arq_sec, const char* arq_list, const char* chave, int patrimonio_id) {
    auto entries = ler_indice_secundario(arq_sec);

    char chave_padrao[20] = {0};
    std::strncpy(chave_padrao, chave, 19);

    int idx_chave = -1;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (std::strcmp(entries[i].chave, chave_padrao) == 0) {
            idx_chave = i;
            break;
        }
    }

    if (idx_chave == -1) return false; // Chave não existe no índice

    FILE* fl = fopen(arq_list, "rb+");
    if (!fl) return false;

    int rrn_anterior = -1;
    int rrn_atual = entries[idx_chave].primeiro_rrn;
    bool removido = false;

    while (rrn_atual != -1) {
        fseek(fl, (long)rrn_atual * sizeof(RegistroLista), SEEK_SET);
        RegistroLista nodo;
        if (fread(&nodo, sizeof(RegistroLista), 1, fl) != 1) break;

        if (nodo.patrimonio_id == patrimonio_id) {
            if (rrn_anterior == -1) {
                // É o primeiro elemento da lista
                entries[idx_chave].primeiro_rrn = nodo.proximo_rrn;
            } else {
                // É um elemento do meio/fim, atualiza o anterior para apontar para o próximo
                fseek(fl, (long)rrn_anterior * sizeof(RegistroLista), SEEK_SET);
                RegistroLista nodo_anterior;
                if (fread(&nodo_anterior, sizeof(RegistroLista), 1, fl) == 1) {
                    nodo_anterior.proximo_rrn = nodo.proximo_rrn;
                    fseek(fl, (long)rrn_anterior * sizeof(RegistroLista), SEEK_SET);
                    fwrite(&nodo_anterior, sizeof(RegistroLista), 1, fl);
                }
            }
            removido = true;
            break;
        }

        rrn_anterior = rrn_atual;
        rrn_atual = nodo.proximo_rrn;
    }
    fclose(fl);

    // Se a lista ficou vazia, remove a chave do índice secundário
    if (removido) {
        if (entries[idx_chave].primeiro_rrn == -1) {
            entries.erase(entries.begin() + idx_chave);
        }
        salvar_indice_secundario(arq_sec, entries);
        return true;
    }

    return false;
}

#endif // LISTA_INVERTIDA_HPP
