#include <iostream>
#include <cstdio>
#include "arvore_b.hpp"

int main() {
    const char* nome_arquivo_btree = "ativos_index.btree";
    
    std::cout << "=========================================================\n";
    std::cout << "     TESTE DO INDICE PRIMARIO - ARVORE B EM DISCO        \n";
    std::cout << "=========================================================\n\n";
    
    // Remove o arquivo de teste anterior se existir
    std::remove(nome_arquivo_btree);
    
    // 1. Inicializa o arquivo de índice da Árvore B
    std::cout << ">>> PASSO 1: Inicializando o arquivo .btree <<<\n";
    inicializar_arvore_b(nome_arquivo_btree);
    
    // Abre o arquivo de índice para manipulação
    FILE* fp = fopen(nome_arquivo_btree, "rb+");
    if (!fp) {
        std::cerr << "Erro ao abrir o arquivo da Arvore B para testes.\n";
        return 1;
    }
    
    depurar_arvore_b(nome_arquivo_btree);
    
    // 2. Inserindo as primeiras 4 chaves (não devem causar split, pois o limite de chaves do nó é 4)
    std::cout << "\n>>> PASSO 2: Inserindo as chaves 10, 20, 30 e 40 (Sem Split) <<<\n";
    // rrn_dados representam o RRN lógico correspondente no arquivo de dados ativos.bin
    inserir_na_arvore_b(fp, 10, 100); 
    inserir_na_arvore_b(fp, 20, 101);
    inserir_na_arvore_b(fp, 30, 102);
    inserir_na_arvore_b(fp, 40, 103);
    
    depurar_arvore_b(nome_arquivo_btree);
    
    // 3. Inserindo a 5ª chave (DEVE causar split da folha raiz e criar uma nova raiz)
    std::cout << "\n>>> PASSO 3: Inserindo a chave 50 (Causara Split da Raiz) <<<\n";
    // Inserindo 50 deve promover a mediana 30, dividindo a raiz (RRN 0) em RRN 0 (esquerda) e RRN 1 (direita),
    // criando a nova raiz no RRN 2.
    inserir_na_arvore_b(fp, 50, 104);
    
    depurar_arvore_b(nome_arquivo_btree);
    
    // 4. Inserindo chaves adicionais para demonstrar o crescimento da árvore e split de folhas não-raiz
    std::cout << "\n>>> PASSO 4: Inserindo chaves 60 e 70 <<<\n";
    inserir_na_arvore_b(fp, 60, 105);
    inserir_na_arvore_b(fp, 70, 106);
    depurar_arvore_b(nome_arquivo_btree);
    
    std::cout << "\n>>> PASSO 5: Inserindo chave 80 (Causara Split do nó filho RRN 1) <<<\n";
    // O nó da direita (RRN 1) atualmente possui chaves [40, 50, 60, 70].
    // Ao inserir 80, ele estourará e dividirá, promovendo a chave 60 para o nó raiz (RRN 2).
    inserir_na_arvore_b(fp, 80, 107);
    
    depurar_arvore_b(nome_arquivo_btree);
    
    // 5. Testes de Busca no Índice Persistido
    std::cout << "\n>>> PASSO 6: Testando buscas por chaves no indice persistido <<<\n";
    
    // Lê o RRN da raiz atual no cabeçalho
    fseek(fp, 0, SEEK_SET);
    int rrn_raiz;
    fread(&rrn_raiz, sizeof(int), 1, fp);
    
    int chaves_busca[] = {40, 70, 10, 80, 99, 5};
    for (int chave : chaves_busca) {
        int rrn_dado_encontrado = buscar_na_arvore_b(fp, rrn_raiz, chave);
        if (rrn_dado_encontrado != -1) {
            std::cout << "Busca por ID " << chave << " -> ENCONTRADO! Registro de dados esta no RRN: " << rrn_dado_encontrado << "\n";
        } else {
            std::cout << "Busca por ID " << chave << " -> NAO ENCONTRADO!\n";
        }
    }
    
    fclose(fp);
    std::cout << "\nTeste da Arvore B encerrado com sucesso.\n";
    
    return 0;
}
