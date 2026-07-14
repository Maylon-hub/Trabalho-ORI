#ifndef ARVORE_B_HPP
#define ARVORE_B_HPP

#include <iostream>
#include <cstdio>
#include <cstring>

// Definição da constante ORDEM da Árvore B (m = 5)
const int ORDEM = 5;

// Garante o alinhamento de 1 byte para que a struct tenha exatamente o tamanho calculado de tamanho fixo.
// ALINHAMENTO IDÊNTICO AO ARQUIVO C (btree.h) PARA COMPATIBILIDADE BINÁRIA TOTAL!
#pragma pack(push, 1)
struct NoArvoreB {
    bool eh_folha;               // 1 byte: Verdadeiro se for um nó folha (offset 0)
    int num_chaves;              // 4 bytes: Número atual de chaves no nó (offset 1)
    int chaves[ORDEM - 1];       // 16 bytes: Armazena os IDs dos ativos (patrimonio_id) (offset 5)
    int rrn_dados[ORDEM - 1];    // 16 bytes: RRN correspondente do registro no arquivo de dados (offset 21)
    int rrn_filhos[ORDEM];       // 20 bytes: Ponteiros (RRNs) para os nós filhos no arquivo .btree (offset 37)
};
#pragma pack(pop)

// Validação em tempo de compilação: 1 + 4 + 16 + 16 + 20 = 57 bytes.
static_assert(sizeof(NoArvoreB) == 57, "A struct NoArvoreB deve ter exatamente 57 bytes de tamanho fixo!");

// Lê um nó da Árvore B de um offset específico do arquivo
inline void ler_no(FILE* fp, int rrn, NoArvoreB& no) {
    if (rrn == -1) {
        std::memset(&no, 0, sizeof(NoArvoreB));
        no.eh_folha = true;
        return;
    }
    long offset = sizeof(int) + rrn * sizeof(NoArvoreB);
    fseek(fp, offset, SEEK_SET);
    if (fread(&no, sizeof(NoArvoreB), 1, fp) != 1) {
        // Se a leitura falhar (RRN fora dos limites ou corrompido), inicializa o nó vazio para evitar loops infinitos
        std::memset(&no, 0, sizeof(NoArvoreB));
        no.eh_folha = true;
        no.num_chaves = 0;
        for (int k = 0; k < ORDEM - 1; k++) {
            no.chaves[k] = 0;
            no.rrn_dados[k] = -1;
        }
        for (int k = 0; k < ORDEM; k++) {
            no.rrn_filhos[k] = -1;
        }
    }
}

// Grava um nó da Árvore B em um offset específico do arquivo
inline void gravar_no(FILE* fp, int rrn, const NoArvoreB& no) {
    if (rrn == -1) return;
    long offset = sizeof(int) + rrn * sizeof(NoArvoreB);
    fseek(fp, offset, SEEK_SET);
    fwrite(&no, sizeof(NoArvoreB), 1, fp);
    fflush(fp);
}

// Retorna o próximo RRN livre no final do arquivo da Árvore B
inline int obter_novo_rrn(FILE* fp) {
    fseek(fp, 0, SEEK_END);
    long tamanho_arquivo = ftell(fp);
    return (tamanho_arquivo - sizeof(int)) / sizeof(NoArvoreB);
}

// Inicializa a Árvore B
inline void inicializar_arvore_b(const char* nome_arquivo_btree) {
    FILE* fp = fopen(nome_arquivo_btree, "rb+");
    
    if (!fp) {
        fp = fopen(nome_arquivo_btree, "wb+");
        if (!fp) {
            std::perror("[Erro B-Tree] Falha ao criar o arquivo da Arvore B");
            return;
        }
        
        int raiz_inicial = 0;
        fwrite(&raiz_inicial, sizeof(int), 1, fp);
        
        NoArvoreB raiz;
        raiz.num_chaves = 0;
        raiz.eh_folha = true;
        for (int k = 0; k < ORDEM - 1; k++) {
            raiz.chaves[k] = 0;
            raiz.rrn_dados[k] = -1;
        }
        for (int k = 0; k < ORDEM; k++) {
            raiz.rrn_filhos[k] = -1;
        }
        
        gravar_no(fp, 0, raiz);
        std::cout << "[B-Tree] Arquivo '" << nome_arquivo_btree << "' inicializado (raiz = RRN 0).\n";
    } else {
        std::cout << "[B-Tree] Arquivo '" << nome_arquivo_btree << "' ja existe. Pronto para uso.\n";
        fclose(fp);
    }
}

// Busca na Árvore B com proteção contra loop infinito (limite de chaves do nó é ORDEM-1 = 4)
inline int buscar_na_arvore_b(FILE* fp_btree, int rrn_raiz_atual, int chave_alvo) {
    int rrn_no = rrn_raiz_atual;
    
    while (rrn_no != -1) {
        NoArvoreB no;
        ler_no(fp_btree, rrn_no, no);
        
        int i = 0;
        // Limita a busca ao tamanho real alocado (ORDEM - 1) para evitar estouro em arquivos corrompidos
        while (i < no.num_chaves && i < (ORDEM - 1) && chave_alvo > no.chaves[i]) {
            i++;
        }
        
        if (i < no.num_chaves && i < (ORDEM - 1) && no.chaves[i] == chave_alvo) {
            return no.rrn_dados[i]; 
        }
        
        if (no.eh_folha) {
            return -1;
        }
        
        rrn_no = no.rrn_filhos[i];
    }
    
    return -1;
}

// Localiza onde uma chave está na Árvore B, com proteção contra loops e leituras fora de limites
inline bool localizar_chave(FILE* fp, int rrn_raiz, int chave, int& rrn_no, int& posicao) {
    rrn_no = rrn_raiz;
    while (rrn_no != -1) {
        NoArvoreB no;
        ler_no(fp, rrn_no, no);
        
        int i = 0;
        while (i < no.num_chaves && i < (ORDEM - 1) && chave > no.chaves[i]) {
            i++;
        }
        
        if (i < no.num_chaves && i < (ORDEM - 1) && no.chaves[i] == chave) {
            posicao = i;
            return true;
        }
        
        if (no.eh_folha) return false;
        
        rrn_no = no.rrn_filhos[i];
    }
    return false;
}

// Realiza a exclusão lógica na Árvore B setando rrn_dados[posicao] = -1
inline bool remover_logico_arvore_b(FILE* fp, int chave) {
    fseek(fp, 0, SEEK_SET);
    int rrn_raiz;
    fread(&rrn_raiz, sizeof(int), 1, fp);
    
    int rrn_no;
    int posicao;
    if (!localizar_chave(fp, rrn_raiz, chave, rrn_no, posicao)) {
        return false;
    }
    
    NoArvoreB no;
    ler_no(fp, rrn_no, no);
    no.rrn_dados[posicao] = -1; // Marca de deleção no índice primário
    gravar_no(fp, rrn_no, no);
    return true;
}

// Inserção recursiva com proteção de limites
inline bool inserir_recursivo(FILE* fp, int rrn_atual, int chave, int rrn_dado,
                              int& promovida_chave, int& promovida_rrn_dado, int& novo_filho_rrn) {
    NoArvoreB no;
    ler_no(fp, rrn_atual, no);

    if (no.eh_folha) {
        int temp_chaves[ORDEM];
        int temp_rrn_dados[ORDEM];
        
        int n_chaves_limite = no.num_chaves > (ORDEM - 1) ? (ORDEM - 1) : no.num_chaves;
        for (int k = 0; k < n_chaves_limite; k++) {
            temp_chaves[k] = no.chaves[k];
            temp_rrn_dados[k] = no.rrn_dados[k];
        }
        
        int i = n_chaves_limite - 1;
        while (i >= 0 && temp_chaves[i] > chave) {
            temp_chaves[i + 1] = temp_chaves[i];
            temp_rrn_dados[i + 1] = temp_rrn_dados[i];
            i--;
        }
        temp_chaves[i + 1] = chave;
        temp_rrn_dados[i + 1] = rrn_dado;
        
        int total_chaves = n_chaves_limite + 1;
        
        if (total_chaves >= ORDEM) {
            NoArvoreB novo_no;
            novo_no.eh_folha = true;
            novo_no.num_chaves = 2;
            
            int rrn_novo_no = obter_novo_rrn(fp);
            
            promovida_chave = temp_chaves[2];
            promovida_rrn_dado = temp_rrn_dados[2];
            novo_filho_rrn = rrn_novo_no;
            
            no.num_chaves = 2;
            no.chaves[0] = temp_chaves[0];
            no.chaves[1] = temp_chaves[1];
            no.chaves[2] = 0;
            no.chaves[3] = 0;
            no.rrn_dados[0] = temp_rrn_dados[0];
            no.rrn_dados[1] = temp_rrn_dados[1];
            no.rrn_dados[2] = -1;
            no.rrn_dados[3] = -1;
            
            novo_no.chaves[0] = temp_chaves[3];
            novo_no.chaves[1] = temp_chaves[4];
            novo_no.chaves[2] = 0;
            novo_no.chaves[3] = 0;
            novo_no.rrn_dados[0] = temp_rrn_dados[3];
            novo_no.rrn_dados[1] = temp_rrn_dados[4];
            novo_no.rrn_dados[2] = -1;
            novo_no.rrn_dados[3] = -1;
            
            for (int k = 0; k < ORDEM; k++) {
                no.rrn_filhos[k] = -1;
                novo_no.rrn_filhos[k] = -1;
            }
            
            gravar_no(fp, rrn_atual, no);
            gravar_no(fp, rrn_novo_no, novo_no);
            return true;
        } else {
            no.num_chaves = total_chaves;
            for (int k = 0; k < total_chaves; k++) {
                no.chaves[k] = temp_chaves[k];
                no.rrn_dados[k] = temp_rrn_dados[k];
            }
            for (int k = total_chaves; k < ORDEM - 1; k++) {
                no.chaves[k] = 0;
                no.rrn_dados[k] = -1;
            }
            gravar_no(fp, rrn_atual, no);
            return false;
        }
    } else {
        int i = 0;
        int n_chaves_limite = no.num_chaves > (ORDEM - 1) ? (ORDEM - 1) : no.num_chaves;
        while (i < n_chaves_limite && chave > no.chaves[i]) {
            i++;
        }
        
        int rrn_filho = no.rrn_filhos[i];
        
        int p_chave, p_rrn_dado, p_filho_rrn;
        bool split_ocorreu = inserir_recursivo(fp, rrn_filho, chave, rrn_dado, p_chave, p_rrn_dado, p_filho_rrn);
        
        if (split_ocorreu) {
            int temp_chaves[ORDEM];
            int temp_rrn_dados[ORDEM];
            int temp_rrn_filhos[ORDEM + 1];
            
            for (int k = 0; k < n_chaves_limite; k++) {
                temp_chaves[k] = no.chaves[k];
                temp_rrn_dados[k] = no.rrn_dados[k];
            }
            for (int k = 0; k <= n_chaves_limite; k++) {
                temp_rrn_filhos[k] = no.rrn_filhos[k];
            }
            
            int idx = n_chaves_limite - 1;
            while (idx >= 0 && temp_chaves[idx] > p_chave) {
                temp_chaves[idx + 1] = temp_chaves[idx];
                temp_rrn_dados[idx + 1] = temp_rrn_dados[idx];
                temp_rrn_filhos[idx + 2] = temp_rrn_filhos[idx + 1];
                idx--;
            }
            
            temp_chaves[idx + 1] = p_chave;
            temp_rrn_dados[idx + 1] = p_rrn_dado;
            temp_rrn_filhos[idx + 2] = p_filho_rrn;
            
            int total_chaves = n_chaves_limite + 1;
            
            if (total_chaves >= ORDEM) {
                NoArvoreB novo_no;
                novo_no.eh_folha = false;
                novo_no.num_chaves = 2;
                
                int rrn_novo_no = obter_novo_rrn(fp);
                
                promovida_chave = temp_chaves[2];
                promovida_rrn_dado = temp_rrn_dados[2];
                novo_filho_rrn = rrn_novo_no;
                
                no.num_chaves = 2;
                no.chaves[0] = temp_chaves[0];
                no.chaves[1] = temp_chaves[1];
                no.chaves[2] = 0;
                no.chaves[3] = 0;
                no.rrn_dados[0] = temp_rrn_dados[0];
                no.rrn_dados[1] = temp_rrn_dados[1];
                no.rrn_dados[2] = -1;
                no.rrn_dados[3] = -1;
                
                no.rrn_filhos[0] = temp_rrn_filhos[0];
                no.rrn_filhos[1] = temp_rrn_filhos[1];
                no.rrn_filhos[2] = temp_rrn_filhos[2];
                no.rrn_filhos[3] = -1;
                no.rrn_filhos[4] = -1;
                
                novo_no.chaves[0] = temp_chaves[3];
                novo_no.chaves[1] = temp_chaves[4];
                novo_no.chaves[2] = 0;
                novo_no.chaves[3] = 0;
                novo_no.rrn_dados[0] = temp_rrn_dados[3];
                novo_no.rrn_dados[1] = temp_rrn_dados[4];
                novo_no.rrn_dados[2] = -1;
                novo_no.rrn_dados[3] = -1;
                novo_no.rrn_filhos[0] = temp_rrn_filhos[3];
                novo_no.rrn_filhos[1] = temp_rrn_filhos[4];
                novo_no.rrn_filhos[2] = temp_rrn_filhos[5];
                novo_no.rrn_filhos[3] = -1;
                novo_no.rrn_filhos[4] = -1;
                
                gravar_no(fp, rrn_atual, no);
                gravar_no(fp, rrn_novo_no, novo_no);
                return true;
            } else {
                no.num_chaves = total_chaves;
                for (int k = 0; k < total_chaves; k++) {
                    no.chaves[k] = temp_chaves[k];
                    no.rrn_dados[k] = temp_rrn_dados[k];
                }
                for (int k = total_chaves; k < ORDEM - 1; k++) {
                    no.chaves[k] = 0;
                    no.rrn_dados[k] = -1;
                }
                for (int k = 0; k <= total_chaves; k++) {
                    no.rrn_filhos[k] = temp_rrn_filhos[k];
                }
                for (int k = total_chaves + 1; k < ORDEM; k++) {
                    no.rrn_filhos[k] = -1;
                }
                gravar_no(fp, rrn_atual, no);
                return false;
            }
        }
    }
    return false;
}

// Insere na Árvore B (público)
inline void inserir_na_arvore_b(FILE* fp_btree, int chave, int rrn_dado) {
    fseek(fp_btree, 0, SEEK_SET);
    int rrn_raiz;
    fread(&rrn_raiz, sizeof(int), 1, fp_btree);
    
    int p_chave, p_rrn_dado, p_filho_rrn;
    bool split_raiz = inserir_recursivo(fp_btree, rrn_raiz, chave, rrn_dado, p_chave, p_rrn_dado, p_filho_rrn);
    
    if (split_raiz) {
        NoArvoreB nova_raiz;
        nova_raiz.eh_folha = false;
        nova_raiz.num_chaves = 1;
        nova_raiz.chaves[0] = p_chave;
        nova_raiz.rrn_dados[0] = p_rrn_dado;
        for (int k = 1; k < ORDEM - 1; k++) {
            nova_raiz.chaves[k] = 0;
            nova_raiz.rrn_dados[k] = -1;
        }
        
        nova_raiz.rrn_filhos[0] = rrn_raiz;
        nova_raiz.rrn_filhos[1] = p_filho_rrn;
        
        for (int k = 2; k < ORDEM; k++) {
            nova_raiz.rrn_filhos[k] = -1;
        }
        
        int rrn_nova_raiz = obter_novo_rrn(fp_btree);
        gravar_no(fp_btree, rrn_nova_raiz, nova_raiz);
        
        fseek(fp_btree, 0, SEEK_SET);
        fwrite(&rrn_nova_raiz, sizeof(int), 1, fp_btree);
        fflush(fp_btree);
        
        std::cout << "[B-Tree Raiz] Nova raiz criada no RRN " << rrn_nova_raiz 
                  << " com a chave promovida " << p_chave << ".\n";
    }
}

// Visualização hierárquica
inline void imprimir_arvore_b_recursiva(FILE* fp, int rrn_no, int nivel) {
    if (rrn_no == -1) return;
    
    NoArvoreB no;
    ler_no(fp, rrn_no, no);
    
    for (int j = 0; j < nivel; j++) std::cout << "   |";
    
    int n_chaves_limite = no.num_chaves > (ORDEM - 1) ? (ORDEM - 1) : no.num_chaves;
    std::cout << "--- No RRN [" << rrn_no << "] (Folha: " << (no.eh_folha ? "Sim" : "Nao") 
              << ", Chaves: " << no.num_chaves << ") -> Chaves: [";
    for (int k = 0; k < n_chaves_limite; k++) {
        std::cout << no.chaves[k] << " (Dado RRN: " << no.rrn_dados[k] << ")";
        if (k < n_chaves_limite - 1) std::cout << ", ";
    }
    std::cout << "]";
    
    if (!no.eh_folha) {
        std::cout << " | Filhos RRNs: [";
        for (int k = 0; k <= n_chaves_limite; k++) {
            std::cout << no.rrn_filhos[k];
            if (k < n_chaves_limite) std::cout << ", ";
        }
        std::cout << "]";
    }
    std::cout << "\n";
    
    if (!no.eh_folha) {
        for (int k = 0; k <= n_chaves_limite; k++) {
            imprimir_arvore_b_recursiva(fp, no.rrn_filhos[k], nivel + 1);
        }
    }
}

inline void depurar_arvore_b(const char* nome_arquivo_btree) {
    FILE* fp = fopen(nome_arquivo_btree, "rb");
    if (!fp) {
        std::cout << "[Depuracao B-Tree] Arquivo de indice inexistente.\n";
        return;
    }
    
    int rrn_raiz;
    fread(&rrn_raiz, sizeof(int), 1, fp);
    
    std::cout << "\n========================================================================\n";
    std::cout << "                     ESTRUTURA HIERARQUICA DA ARVORE B                  \n";
    std::cout << "========================================================================\n";
    std::cout << " Raiz da Arvore: RRN " << rrn_raiz << "\n";
    std::cout << " Tamanho do No (NoArvoreB): " << sizeof(NoArvoreB) << " bytes\n";
    std::cout << "------------------------------------------------------------------------\n";
    
    imprimir_arvore_b_recursiva(fp, rrn_raiz, 0);
    
    std::cout << "========================================================================\n\n";
    fclose(fp);
}

#endif // ARVORE_B_HPP
