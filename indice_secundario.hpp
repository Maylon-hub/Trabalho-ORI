#ifndef INDICE_SECUNDARIO_HPP
#define INDICE_SECUNDARIO_HPP

#include <iostream>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

// Garante o alinhamento de 1 byte para evitar padding extra do compilador.
#pragma pack(push, 1)

// Registro do índice secundário que mapeia uma chave (tipo_equipamento) para a cabeça da lista invertida
struct IndiceSecundario {
    char tipo_equipamento[20]; // 20 bytes (Chave Secundária)
    int rrn_lista;             // 4 bytes: RRN inicial do nó da lista invertida no arquivo 'tipo.inv'
};

// Registro da lista invertida armazenada em disco (tipo.inv)
struct NoListaInvertida {
    int patrimonio_id;         // 4 bytes (Chave Primária do ativo, que será resolvida pela Árvore B)
    int proximo_rrn;           // 4 bytes: RRN do próximo nó na lista invertida, ou -1 se for o fim
};

#pragma pack(pop)

// Validações de tamanho físico em tempo de compilação
static_assert(sizeof(IndiceSecundario) == 24, "A struct IndiceSecundario deve ter exatamente 24 bytes!");
static_assert(sizeof(NoListaInvertida) == 8, "A struct NoListaInvertida deve ter exatamente 8 bytes!");

/**
 * 1. Inicializar Arquivo de Lista Invertida
 * Cria o arquivo 'tipo.inv' se ele não existir. Como gravamos os nós diretamente 
 * a partir do offset 0 (sem cabeçalho de metadados), apenas criamos o arquivo vazio.
 */
inline void inicializar_lista_invertida(const char* nome_inv) {
    FILE* fp = fopen(nome_inv, "rb");
    if (!fp) {
        // Se não existe, cria o arquivo vazio
        fp = fopen(nome_inv, "wb");
        if (!fp) {
            std::perror("[Erro Indice Secundario] Falha ao criar arquivo de lista invertida");
            return;
        }
        std::cout << "[Secundario] Arquivo de lista invertida '" << nome_inv << "' criado (vazio).\n";
    } else {
        std::cout << "[Secundario] Arquivo de lista invertida '" << nome_inv << "' ja existe.\n";
    }
    fclose(fp);
}

/**
 * 2. Carregar Índice Secundário na RAM
 * Lê as entradas ordenadas do arquivo 'tipo.sec' e carrega na memória RAM.
 */
inline bool carregar_indice_secundario(const char* nome_sec, std::vector<IndiceSecundario>& idx_sec) {
    idx_sec.clear();
    FILE* fp = fopen(nome_sec, "rb");
    if (!fp) {
        std::cout << "[Secundario] Arquivo '" << nome_sec << "' nao encontrado. Iniciando indice vazio na RAM.\n";
        return false;
    }
    
    IndiceSecundario entrada;
    // Lê registro por registro sequencialmente de 24 em 24 bytes
    while (fread(&entrada, sizeof(IndiceSecundario), 1, fp) == 1) {
        idx_sec.push_back(entrada);
    }
    
    fclose(fp);
    std::cout << "[Secundario] Carregadas " << idx_sec.size() << " chaves secundarias de '" << nome_sec << "' para a RAM.\n";
    return true;
}

/**
 * 3. Salvar Índice Secundário no Disco (no Fechamento)
 * Ordena as entradas na RAM (caso já não estejam) e as persiste de volta no disco em 'tipo.sec'.
 */
inline bool salvar_indice_secundario(const char* nome_sec, std::vector<IndiceSecundario>& idx_sec) {
    // Garante que o vetor esteja devidamente ordenado pela chave tipo_equipamento
    std::sort(idx_sec.begin(), idx_sec.end(), [](const IndiceSecundario& a, const IndiceSecundario& b) {
        return std::strcmp(a.tipo_equipamento, b.tipo_equipamento) < 0;
    });
    
    FILE* fp = fopen(nome_sec, "wb");
    if (!fp) {
        std::perror("[Erro Indice Secundario] Falha ao abrir 'tipo.sec' para gravacao");
        return false;
    }
    
    // Grava todo o vetor ordenado em disco de uma só vez
    if (!idx_sec.empty()) {
        fwrite(idx_sec.data(), sizeof(IndiceSecundario), idx_sec.size(), fp);
    }
    
    fclose(fp);
    std::cout << "[Secundario] Indice secundario '" << nome_sec << "' salvo com sucesso no disco (" 
              << idx_sec.size() << " chaves ordenadas).\n";
    return true;
}

/**
 * 4. Inserir na Lista Invertida e Índice Secundário
 * Insere um novo par (tipo, patrimonio_id) na estrutura.
 * - O nó físico é gravado no final do arquivo 'tipo.inv' (Lógica de Append: O(1)).
 * - O novo nó aponta para a cabeça anterior da lista invertida daquele tipo (Lógica de Pilha: Prepend).
 * - A RAM é atualizada com a nova cabeça e a ordenação é mantida em RAM.
 */
inline void inserir_indice_secundario(std::vector<IndiceSecundario>& idx_sec, FILE* fp_inv, 
                                      const char* tipo_equipamento, int patrimonio_id) {
    // Busca binária na RAM usando std::lower_bound para encontrar onde a chave está (ou deveria estar)
    auto it = std::lower_bound(idx_sec.begin(), idx_sec.end(), tipo_equipamento, [](const IndiceSecundario& a, const char* val) {
        return std::strcmp(a.tipo_equipamento, val) < 0;
    });
    
    int antigo_rrn_lista = -1;
    bool existe = (it != idx_sec.end() && std::strcmp(it->tipo_equipamento, tipo_equipamento) == 0);
    
    if (existe) {
        // Se a chave já existe, recuperamos o RRN da cabeça atual da lista invertida
        antigo_rrn_lista = it->rrn_lista;
    }
    
    // Posiciona o ponteiro de arquivo no final de 'tipo.inv' para descobrir o próximo RRN livre
    fseek(fp_inv, 0, SEEK_END);
    long offset_fim = ftell(fp_inv);
    
    // Matemática do Offset da Lista Invertida (sem cabeçalho):
    // Offset = RRN * sizeof(NoListaInvertida) -> RRN = Offset / 8
    int novo_rrn_no = offset_fim / sizeof(NoListaInvertida);
    
    // Preenche o novo nó da lista invertida
    NoListaInvertida novo_no;
    novo_no.patrimonio_id = patrimonio_id;
    novo_no.proximo_rrn = antigo_rrn_lista; // Faz o prepend: aponta para a antiga cabeça (ou -1 se era vazia)
    
    // Escreve o nó no final do arquivo tipo.inv
    fwrite(&novo_no, sizeof(NoListaInvertida), 1, fp_inv);
    fflush(fp_inv);
    
    std::cout << "[Secundario] Gravado no '" << tipo_equipamento << "' no RRN " << novo_rrn_no 
              << " de 'tipo.inv' (patrimonio_id: " << patrimonio_id 
              << ", proximo_rrn: " << antigo_rrn_lista << ").\n";
              
    // Atualiza o índice secundário na RAM
    if (existe) {
        // Apenas atualiza a cabeça da lista para apontar para o novo nó
        it->rrn_lista = novo_rrn_no;
    } else {
        // Cria a nova entrada de índice secundário e insere no ponto correto para manter ordenado
        IndiceSecundario novo_idx;
        std::strncpy(novo_idx.tipo_equipamento, tipo_equipamento, sizeof(novo_idx.tipo_equipamento) - 1);
        novo_idx.tipo_equipamento[sizeof(novo_idx.tipo_equipamento) - 1] = '\0';
        novo_idx.rrn_lista = novo_rrn_no;
        
        idx_sec.insert(it, novo_idx);
        std::cout << "[Secundario] Nova chave '" << tipo_equipamento << "' adicionada a RAM apontando para RRN " << novo_rrn_no << ".\n";
    }
}

/**
 * 5. Buscar na Lista Invertida
 * Efetua a busca por uma chave secundária (tipo_equipamento).
 * - Busca binária na RAM para localizar a cabeça da lista.
 * - Varredura em disco navegando pelas referências de RRN no arquivo 'tipo.inv' para coletar todos os IDs.
 * Retorna true se encontrou pelo menos um ID correspondente.
 */
inline bool buscar_indice_secundario(const std::vector<IndiceSecundario>& idx_sec, FILE* fp_inv, 
                                      const char* tipo_equipamento, std::vector<int>& ids_encontrados) {
    ids_encontrados.clear();
    
    // Busca binária em RAM
    auto it = std::lower_bound(idx_sec.begin(), idx_sec.end(), tipo_equipamento, [](const IndiceSecundario& a, const char* val) {
        return std::strcmp(a.tipo_equipamento, val) < 0;
    });
    
    if (it == idx_sec.end() || std::strcmp(it->tipo_equipamento, tipo_equipamento) != 0) {
        std::cout << "[Secundario] Busca por '" << tipo_equipamento << "' -> Chave nao encontrada na RAM.\n";
        return false;
    }
    
    int rrn_atual = it->rrn_lista;
    std::cout << "[Secundario] Busca por '" << tipo_equipamento << "' -> Iniciando travessia em 'tipo.inv' a partir do RRN " << rrn_atual << "...\n";
    
    // Percorre a lista invertida no disco navegando de nó em nó
    while (rrn_atual != -1) {
        long offset = rrn_atual * sizeof(NoListaInvertida);
        fseek(fp_inv, offset, SEEK_SET);
        
        NoListaInvertida no;
        if (fread(&no, sizeof(NoListaInvertida), 1, fp_inv) != 1) {
            std::cerr << "   [Erro] Falha ao ler no da lista invertida no RRN " << rrn_atual << "\n";
            break;
        }
        
        std::cout << "   -> Lido RRN " << rrn_atual << " | ID Patrimonio: " << no.patrimonio_id 
                  << " | Proximo RRN: " << no.proximo_rrn << "\n";
                  
        ids_encontrados.push_back(no.patrimonio_id);
        rrn_atual = no.proximo_rrn; // Vai para o próximo elemento encadeado
    }
    
    return !ids_encontrados.empty();
}

/**
 * Função Auxiliar de Depuração
 * Varre o arquivo 'tipo.inv' e lista todas as chaves em RAM para fins didáticos (fator de domínio).
 */
inline void depurar_indice_secundario(const char* nome_sec, const char* nome_inv, const std::vector<IndiceSecundario>& idx_sec) {
    std::cout << "\n========================================================================\n";
    std::cout << "               INDICE SECUNDARIO (RAM) E LISTA INVERTIDA (DISCO)        \n";
    std::cout << "========================================================================\n";
    std::cout << " Chaves na RAM (Tamanho: " << idx_sec.size() << "):\n";
    for (size_t i = 0; i < idx_sec.size(); i++) {
        std::cout << " [" << i << "] Tipo: " << idx_sec[i].tipo_equipamento << " | Head RRN da lista no disco: " << idx_sec[i].rrn_lista << "\n";
    }
    std::cout << "------------------------------------------------------------------------\n";
    
    FILE* fp = fopen(nome_inv, "rb");
    if (!fp) {
        std::cout << " [Lista Invertida] Arquivo '" << nome_inv << "' nao pôde ser lido.\n";
        std::cout << "========================================================================\n\n";
        return;
    }
    
    std::cout << " Conteúdo físico do arquivo '" << nome_inv << "' (tipo.inv) no disco:\n";
    NoListaInvertida no;
    int rrn = 0;
    while (fread(&no, sizeof(NoListaInvertida), 1, fp) == 1) {
        long offset = rrn * sizeof(NoListaInvertida);
        std::cout << "   RRN [" << rrn << "] | Offset: " << offset << " bytes | Patrimonio ID: " << no.patrimonio_id << " | Proximo RRN: " << no.proximo_rrn << "\n";
        rrn++;
    }
    fclose(fp);
    std::cout << "========================================================================\n\n";
}

#endif // INDICE_SECUNDARIO_HPP
