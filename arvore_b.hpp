#ifndef ARVORE_B_HPP
#define ARVORE_B_HPP

#include <iostream>
#include <cstdio>
#include <cstring>

// Definição da constante ORDEM da Árvore B (m = 5)
// Para ORDEM = 5, o número máximo de chaves em um nó é ORDEM - 1 = 4.
// O número máximo de filhos é ORDEM = 5.
// A cisão (split) ocorre quando um nó atinge ORDEM (5) chaves.
const int ORDEM = 5;

// Garante o alinhamento de 1 byte para que a struct tenha exatamente o tamanho calculado de tamanho fixo.
#pragma pack(push, 1)
struct NoArvoreB {
    int num_chaves;              // 4 bytes: Número atual de chaves no nó
    bool eh_folha;               // 1 byte: Verdadeiro se for um nó folha
    int chaves[ORDEM - 1];       // 16 bytes: Armazena os IDs dos ativos (patrimonio_id)
    int rrn_dados[ORDEM - 1];    // 16 bytes: RRN correspondente do registro no arquivo de dados
    int rrn_filhos[ORDEM];       // 20 bytes: Ponteiros (RRNs) para os nós filhos no arquivo .btree
};
#pragma pack(pop)

// Validação em tempo de compilação: 4 + 1 + 16 + 16 + 20 = 57 bytes.
static_assert(sizeof(NoArvoreB) == 57, "A struct NoArvoreB deve ter exatamente 57 bytes de tamanho fixo!");

/**
 * Explicação matemática dos Offsets:
 * - O arquivo da Árvore B possui um cabeçalho inicial de 4 bytes (int) contendo o RRN da Raiz.
 * - Assim, o byte inicial do nó com RRN N é calculado pela fórmula:
 * 
 *   Offset = sizeof(int) + (RRN_No * sizeof(NoArvoreB))
 *          = 4 + (RRN_No * 57)
 */

// Lê um nó da Árvore B de um offset específico do arquivo
inline void ler_no(FILE* fp, int rrn, NoArvoreB& no) {
    if (rrn == -1) return;
    long offset = sizeof(int) + rrn * sizeof(NoArvoreB);
    fseek(fp, offset, SEEK_SET);
    fread(&no, sizeof(NoArvoreB), 1, fp);
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

/**
 * 1. Inicializar Árvore B
 * Cria o arquivo, reservando os 4 primeiros bytes para guardar o RRN do nó Raiz.
 * Inicializa a primeira raiz vazia no RRN 0.
 */
inline void inicializar_arvore_b(const char* nome_arquivo_btree) {
    FILE* fp = fopen(nome_arquivo_btree, "rb+");
    
    if (!fp) {
        // Se o arquivo não existir, cria um novo
        fp = fopen(nome_arquivo_btree, "wb+");
        if (!fp) {
            std::perror("[Erro B-Tree] Falha ao criar o arquivo da Arvore B");
            return;
        }
        
        // RRN do nó raiz inicial é 0
        int raiz_inicial = 0;
        fwrite(&raiz_inicial, sizeof(int), 1, fp);
        
        // Cria o nó raiz inicial (vazio e folha)
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
        
        // Grava a raiz vazia no RRN 0 (offset 4)
        gravar_no(fp, 0, raiz);
        
        std::cout << "[B-Tree] Arquivo '" << nome_arquivo_btree << "' inicializado.\n";
        std::cout << "         -> Cabecalho gravado no offset 0 (raiz = RRN 0).\n";
        std::cout << "         -> No raiz vazio gravado no RRN 0 (offset 4 bytes).\n";
    } else {
        std::cout << "[B-Tree] Arquivo '" << nome_arquivo_btree << "' ja existe. Pronto para uso.\n";
    }
    
    fclose(fp);
}

/**
 * 2. Buscar na Árvore B
 * Navega pelos nós lendo do disco até encontrar a chave, retornando o 'rrn_dados' (ou -1 se não achar).
 * Implementação iterativa para maior desempenho e economia de memória em disco.
 */
inline int buscar_na_arvore_b(FILE* fp_btree, int rrn_raiz_atual, int chave_alvo) {
    int rrn_no = rrn_raiz_atual;
    
    while (rrn_no != -1) {
        NoArvoreB no;
        ler_no(fp_btree, rrn_no, no);
        
        // Realiza busca linear dentro do nó carregado na memória ram
        int i = 0;
        while (i < no.num_chaves && chave_alvo > no.chaves[i]) {
            i++;
        }
        
        // Se encontramos a chave
        if (i < no.num_chaves && no.chaves[i] == chave_alvo) {
            return no.rrn_dados[i]; // Retorna o RRN correspondente do registro de dados
        }
        
        // Se for nó folha e não encontramos a chave, ela não existe
        if (no.eh_folha) {
            return -1;
        }
        
        // Caso contrário, desce para o nó filho apropriado
        rrn_no = no.rrn_filhos[i];
    }
    
    return -1;
}

/**
 * Função de Suporte: Inserção Recursiva e Split
 * Insere a chave de forma recursiva e realiza o Split se necessário.
 * Retorna true se o nó em rrn_atual sofreu divisão (split).
 * Se sofrer divisão, retorna por referência a chave promovida, seu rrn_dado e o RRN do novo nó filho da direita.
 */
inline bool inserir_recursivo(FILE* fp, int rrn_atual, int chave, int rrn_dado,
                              int& promovida_chave, int& promovida_rrn_dado, int& novo_filho_rrn) {
    NoArvoreB no;
    ler_no(fp, rrn_atual, no);

    if (no.eh_folha) {
        // --- INSERÇÃO EM NÓ FOLHA ---
        
        // Usamos estruturas locais maiores (tamanho ORDEM) para acolher a nova chave temporariamente antes do split
        int temp_chaves[ORDEM];
        int temp_rrn_dados[ORDEM];
        
        // Copia as chaves existentes do nó folha
        for (int k = 0; k < no.num_chaves; k++) {
            temp_chaves[k] = no.chaves[k];
            temp_rrn_dados[k] = no.rrn_dados[k];
        }
        
        // Insere a nova chave ordenadamente
        int i = no.num_chaves - 1;
        while (i >= 0 && temp_chaves[i] > chave) {
            temp_chaves[i + 1] = temp_chaves[i];
            temp_rrn_dados[i + 1] = temp_rrn_dados[i];
            i--;
        }
        temp_chaves[i + 1] = chave;
        temp_rrn_dados[i + 1] = rrn_dado;
        
        int total_chaves = no.num_chaves + 1;
        
        // Se o número de chaves atingiu ORDEM (5), precisamos dividir (Split)
        if (total_chaves >= ORDEM) {
            // Cria o nó irmão da direita
            NoArvoreB novo_no;
            novo_no.eh_folha = true;
            novo_no.num_chaves = 2; // Metade vai para a direita
            
            int rrn_novo_no = obter_novo_rrn(fp);
            
            // O elemento mediano (índice 2) é promovido
            promovida_chave = temp_chaves[2];
            promovida_rrn_dado = temp_rrn_dados[2];
            novo_filho_rrn = rrn_novo_no;
            
            // Atualiza o nó original da esquerda (terá 2 chaves)
            no.num_chaves = 2;
            no.chaves[0] = temp_chaves[0];
            no.chaves[1] = temp_chaves[1];
            no.chaves[2] = 0; // Limpa slots não usados
            no.chaves[3] = 0;
            no.rrn_dados[0] = temp_rrn_dados[0];
            no.rrn_dados[1] = temp_rrn_dados[1];
            no.rrn_dados[2] = -1;
            no.rrn_dados[3] = -1;
            
            // Preenche o novo nó da direita (terá 2 chaves)
            novo_no.chaves[0] = temp_chaves[3];
            novo_no.chaves[1] = temp_chaves[4];
            novo_no.chaves[2] = 0;
            novo_no.chaves[3] = 0;
            novo_no.rrn_dados[0] = temp_rrn_dados[3];
            novo_no.rrn_dados[1] = temp_rrn_dados[4];
            novo_no.rrn_dados[2] = -1;
            novo_no.rrn_dados[3] = -1;
            
            // Como são folhas, inicializa ponteiros de filhos com -1
            for (int k = 0; k < ORDEM; k++) {
                no.rrn_filhos[k] = -1;
                novo_no.rrn_filhos[k] = -1;
            }
            
            // Grava os dois nós de volta em disco
            gravar_no(fp, rrn_atual, no);
            gravar_no(fp, rrn_novo_no, novo_no);
            
            std::cout << "[Split Folha] No RRN " << rrn_atual << " dividiu. Nova chave " << promovida_chave 
                      << " promovida. Novo no criado no RRN " << rrn_novo_no << ".\n";
            return true;
        } else {
            // Não estourou o limite máximo: apenas salva as chaves ordenadas no próprio nó
            no.num_chaves = total_chaves;
            for (int k = 0; k < total_chaves; k++) {
                no.chaves[k] = temp_chaves[k];
                no.rrn_dados[k] = temp_rrn_dados[k];
            }
            // Limpa slots vazios
            for (int k = total_chaves; k < ORDEM - 1; k++) {
                no.chaves[k] = 0;
                no.rrn_dados[k] = -1;
            }
            gravar_no(fp, rrn_atual, no);
            return false;
        }
    } else {
        // --- INSERÇÃO EM NÓ NÃO-FOLHA (RECURSÃO) ---
        
        // Encontra em qual filho a chave deve ser inserida
        int i = 0;
        while (i < no.num_chaves && chave > no.chaves[i]) {
            i++;
        }
        
        int rrn_filho = no.rrn_filhos[i];
        
        int p_chave, p_rrn_dado, p_filho_rrn;
        bool split_ocorreu = inserir_recursivo(fp, rrn_filho, chave, rrn_dado, p_chave, p_rrn_dado, p_filho_rrn);
        
        if (split_ocorreu) {
            // Se o filho sofreu split, devemos inserir o elemento promovido e seu novo filho neste nó
            int temp_chaves[ORDEM];
            int temp_rrn_dados[ORDEM];
            int temp_rrn_filhos[ORDEM + 1];
            
            // Copia as estruturas originais
            for (int k = 0; k < no.num_chaves; k++) {
                temp_chaves[k] = no.chaves[k];
                temp_rrn_dados[k] = no.rrn_dados[k];
            }
            for (int k = 0; k <= no.num_chaves; k++) {
                temp_rrn_filhos[k] = no.rrn_filhos[k];
            }
            
            // Insere a chave promovida de forma ordenada
            int idx = no.num_chaves - 1;
            while (idx >= 0 && temp_chaves[idx] > p_chave) {
                temp_chaves[idx + 1] = temp_chaves[idx];
                temp_rrn_dados[idx + 1] = temp_rrn_dados[idx];
                temp_rrn_filhos[idx + 2] = temp_rrn_filhos[idx + 1];
                idx--;
            }
            
            temp_chaves[idx + 1] = p_chave;
            temp_rrn_dados[idx + 1] = p_rrn_dado;
            temp_rrn_filhos[idx + 2] = p_filho_rrn;
            
            int total_chaves = no.num_chaves + 1;
            
            if (total_chaves >= ORDEM) {
                // O nó interno (não-folha) estourou. Precisamos dividir (Split)
                NoArvoreB novo_no;
                novo_no.eh_folha = false;
                novo_no.num_chaves = 2; // Metade vai para a direita
                
                int rrn_novo_no = obter_novo_rrn(fp);
                
                // Promove o elemento mediano (índice 2)
                promovida_chave = temp_chaves[2];
                promovida_rrn_dado = temp_rrn_dados[2];
                novo_filho_rrn = rrn_novo_no;
                
                // Atualiza o nó original da esquerda (terá 2 chaves e 3 filhos)
                no.num_chaves = 2;
                no.chaves[0] = temp_chaves[0];
                no.chaves[1] = temp_chaves[1];
                no.chaves[2] = 0; // Limpa slots não usados
                no.chaves[3] = 0;
                no.rrn_dados[0] = temp_rrn_dados[0];
                no.rrn_dados[1] = temp_rrn_dados[1];
                no.rrn_dados[2] = -1;
                no.rrn_dados[3] = -1;
                
                no.rrn_filhos[0] = temp_rrn_filhos[0];
                no.rrn_filhos[1] = temp_rrn_filhos[1];
                no.rrn_filhos[2] = temp_rrn_filhos[2];
                no.rrn_filhos[3] = -1; // Limpa filhos não usados
                no.rrn_filhos[4] = -1;
                
                // Preenche o novo nó da direita (terá 2 chaves e 3 filhos)
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
                
                // Grava os dois nós de volta em disco
                gravar_no(fp, rrn_atual, no);
                gravar_no(fp, rrn_novo_no, novo_no);
                
                std::cout << "[Split Interno] No RRN " << rrn_atual << " dividiu. Nova chave " << promovida_chave 
                          << " promovida. Novo no criado no RRN " << rrn_novo_no << ".\n";
                return true;
            } else {
                // Não estourou o limite máximo: apenas salva no nó atual
                no.num_chaves = total_chaves;
                for (int k = 0; k < total_chaves; k++) {
                    no.chaves[k] = temp_chaves[k];
                    no.rrn_dados[k] = temp_rrn_dados[k];
                }
                // Limpa slots de chaves vazios
                for (int k = total_chaves; k < ORDEM - 1; k++) {
                    no.chaves[k] = 0;
                    no.rrn_dados[k] = -1;
                }
                for (int k = 0; k <= total_chaves; k++) {
                    no.rrn_filhos[k] = temp_rrn_filhos[k];
                }
                // Limpa slots de filhos vazios
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

/**
 * 3. Inserir na Árvore B
 * Função pública/driver principal. Lê a raiz atual, dispara a inserção recursiva.
 * Se a raiz sofrer cisão, cria uma nova raiz, atualizando o cabeçalho (offset 0).
 */
inline void inserir_na_arvore_b(FILE* fp_btree, int chave, int rrn_dado) {
    // 1. Ler o RRN da raiz atual no cabeçalho (offset 0)
    fseek(fp_btree, 0, SEEK_SET);
    int rrn_raiz;
    fread(&rrn_raiz, sizeof(int), 1, fp_btree);
    
    int p_chave, p_rrn_dado, p_filho_rrn;
    // 2. Chama a recursão de inserção
    bool split_raiz = inserir_recursivo(fp_btree, rrn_raiz, chave, rrn_dado, p_chave, p_rrn_dado, p_filho_rrn);
    
    // 3. Se a raiz sofreu cisão (split), precisamos criar uma nova raiz
    if (split_raiz) {
        NoArvoreB nova_raiz;
        nova_raiz.eh_folha = false;
        nova_raiz.num_chaves = 1;
        nova_raiz.chaves[0] = p_chave;
        nova_raiz.rrn_dados[0] = p_rrn_dado;
        // Limpa slots não usados
        for (int k = 1; k < ORDEM - 1; k++) {
            nova_raiz.chaves[k] = 0;
            nova_raiz.rrn_dados[k] = -1;
        }
        
        nova_raiz.rrn_filhos[0] = rrn_raiz;       // Antiga raiz vira o filho esquerdo
        nova_raiz.rrn_filhos[1] = p_filho_rrn;    // O novo filho gerado no split vira o filho direito
        
        // Inicializa o restante dos filhos como -1
        for (int k = 2; k < ORDEM; k++) {
            nova_raiz.rrn_filhos[k] = -1;
        }
        
        // Obtém RRN livre no final do arquivo da btree para alocar a nova raiz
        int rrn_nova_raiz = obter_novo_rrn(fp_btree);
        
        // Grava a nova raiz no final do arquivo
        gravar_no(fp_btree, rrn_nova_raiz, nova_raiz);
        
        // Atualiza o cabeçalho no offset 0 para apontar para a nova raiz
        fseek(fp_btree, 0, SEEK_SET);
        fwrite(&rrn_nova_raiz, sizeof(int), 1, fp_btree);
        fflush(fp_btree);
        
        std::cout << "[B-Tree Raiz] Nova raiz criada no RRN " << rrn_nova_raiz 
                  << " com a chave promovida " << p_chave << ".\n";
    }
}

/**
 * Função Auxiliar de Visualização Recursiva
 * Exibe a hierarquia da Árvore B para fins didáticos (fator de domínio)
 */
inline void imprimir_arvore_b_recursiva(FILE* fp, int rrn_no, int nivel) {
    if (rrn_no == -1) return;
    
    NoArvoreB no;
    ler_no(fp, rrn_no, no);
    
    // Identação pedagógica com base no nível do nó na árvore
    for (int j = 0; j < nivel; j++) std::cout << "   |";
    
    std::cout << "--- No RRN [" << rrn_no << "] (Folha: " << (no.eh_folha ? "Sim" : "Nao") 
              << ", Chaves: " << no.num_chaves << ") -> Chaves: [";
    for (int k = 0; k < no.num_chaves; k++) {
        std::cout << no.chaves[k] << " (Dado RRN: " << no.rrn_dados[k] << ")";
        if (k < no.num_chaves - 1) std::cout << ", ";
    }
    std::cout << "]";
    
    if (!no.eh_folha) {
        std::cout << " | Filhos RRNs: [";
        for (int k = 0; k <= no.num_chaves; k++) {
            std::cout << no.rrn_filhos[k];
            if (k < no.num_chaves) std::cout << ", ";
        }
        std::cout << "]";
    }
    std::cout << "\n";
    
    // Visita recursivamente os filhos da esquerda para a direita
    if (!no.eh_folha) {
        for (int k = 0; k <= no.num_chaves; k++) {
            imprimir_arvore_b_recursiva(fp, no.rrn_filhos[k], nivel + 1);
        }
    }
}

// Função pública para disparar a visualização da Árvore B
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
