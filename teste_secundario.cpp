#include <iostream>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include "arvore_b.hpp"
#include "indice_secundario.hpp"

// Definição da struct Ativo idêntica ao main.cpp original
#pragma pack(push, 1)
struct Ativo {
    int patrimonio_id;         // 4 bytes (Chave Primária se >= 0, ou Ponteiro da LED se < 0)
    char tipo_equipamento[20]; // 20 bytes (ex: "Notebook", "Monitor")
    char setor_alocacao[20];   // 20 bytes (ex: "TI", "Financeiro")
    char marca_modelo[40];     // 40 bytes (ex: "Dell Latitude 3420")
    float valor_compra;        // 4 bytes (ex: 4500.50)
};
#pragma pack(pop)

// Garante que o tamanho da struct seja exatamente 88 bytes
static_assert(sizeof(Ativo) == 88, "A struct Ativo deve ter exatamente 88 bytes!");

// Funções auxiliares para controle da LED (Lista de Espaços Disponíveis) no arquivo de dados
bool esta_removido(const Ativo& a) {
    return a.patrimonio_id < 0;
}

int codificar_proximo_rrn(int proximo_rrn) {
    return -(proximo_rrn + 2);
}

int decodificar_proximo_rrn(int patrimonio_id) {
    return -patrimonio_id - 2;
}

// Inicializa o arquivo de dados com o cabeçalho LED (-1)
void inicializar_arquivo_dados(const char* nome_arquivo) {
    FILE* fp = fopen(nome_arquivo, "rb");
    if (!fp) {
        fp = fopen(nome_arquivo, "wb+");
        if (!fp) {
            std::perror("[Erro Dados] Falha ao criar arquivo de dados");
            return;
        }
        int topo_inicial = -1;
        fwrite(&topo_inicial, sizeof(int), 1, fp);
        std::cout << "[Dados] Arquivo de dados '" << nome_arquivo << "' criado com cabecalho LED = -1 (Vazio).\n";
    } else {
        std::cout << "[Dados] Arquivo de dados '" << nome_arquivo << "' ja existe.\n";
    }
    fclose(fp);
}

// Escreve um ativo no arquivo de dados e retorna o RRN de escrita
int inserir_ativo_dados(FILE* fp, Ativo novo_ativo) {
    // Lê o topo da LED no cabeçalho
    fseek(fp, 0, SEEK_SET);
    int topo_led;
    fread(&topo_led, sizeof(int), 1, fp);
    
    int rrn_gravado = -1;
    
    if (topo_led != -1) {
        // CASO 1: Reaproveitar espaço da LED (Pilha LIFO)
        rrn_gravado = topo_led;
        long offset_registro = sizeof(int) + rrn_gravado * sizeof(Ativo);
        
        fseek(fp, offset_registro, SEEK_SET);
        Ativo registro_deletado;
        fread(&registro_deletado, sizeof(Ativo), 1, fp);
        
        // Decodifica o próximo RRN armazenado
        int proximo_rrn = decodificar_proximo_rrn(registro_deletado.patrimonio_id);
        
        // Sobrescreve o registro no slot recuperado
        fseek(fp, offset_registro, SEEK_SET);
        fwrite(&novo_ativo, sizeof(Ativo), 1, fp);
        
        // Atualiza a cabeça da LED no cabeçalho do arquivo
        fseek(fp, 0, SEEK_SET);
        fwrite(&proximo_rrn, sizeof(int), 1, fp);
        
        std::cout << "[Dados] REAPROVEITADO RRN " << rrn_gravado << " (LED topo anterior: " << topo_led << " -> novo topo: " << proximo_rrn << ")\n";
    } else {
        // CASO 2: Gravar no final do arquivo (LED vazia)
        fseek(fp, 0, SEEK_END);
        long offset_fim = ftell(fp);
        rrn_gravado = (offset_fim - sizeof(int)) / sizeof(Ativo);
        
        fwrite(&novo_ativo, sizeof(Ativo), 1, fp);
        std::cout << "[Dados] Gravado no FIM DO ARQUIVO no RRN " << rrn_gravado << "\n";
    }
    fflush(fp);
    return rrn_gravado;
}

// Remove logicamente o ativo do arquivo de dados e atualiza a LED (Pilha LIFO)
void remover_ativo_dados(FILE* fp, int id_alvo, int rrn_alvo) {
    // 1. Lê o topo da LED atual
    fseek(fp, 0, SEEK_SET);
    int antigo_topo;
    fread(&antigo_topo, sizeof(int), 1, fp);
    
    // 2. Lê o registro a ser removido
    long offset_registro = sizeof(int) + rrn_alvo * sizeof(Ativo);
    fseek(fp, offset_registro, SEEK_SET);
    Ativo reg;
    fread(&reg, sizeof(Ativo), 1, fp);
    
    // 3. Aplica remoção lógica codificando o antigo_topo no patrimonio_id
    reg.patrimonio_id = codificar_proximo_rrn(antigo_topo);
    std::strcpy(reg.tipo_equipamento, "[DELETADO]");
    std::strcpy(reg.setor_alocacao, "[LED]");
    std::strcpy(reg.marca_modelo, "[DISPONIVEL]");
    reg.valor_compra = 0.0f;
    
    // 4. Sobrescreve o registro modificado no disco
    fseek(fp, offset_registro, SEEK_SET);
    fwrite(&reg, sizeof(Ativo), 1, fp);
    
    // 5. Atualiza o cabeçalho (topo da LED passa a ser o RRN recém-deletado)
    fseek(fp, 0, SEEK_SET);
    fwrite(&rrn_alvo, sizeof(int), 1, fp);
    fflush(fp);
    
    std::cout << "[Dados] Registro ID " << id_alvo << " no RRN " << rrn_alvo 
              << " removido logicamente (LED topo anterior: " << antigo_topo << " -> novo topo: " << rrn_alvo << ")\n";
}

// Função de inserção integrada: insere no arquivo de dados, na Árvore B e no índice secundário
void inserir_registro_completo(FILE* fp_dados, FILE* fp_btree, FILE* fp_inv, std::vector<IndiceSecundario>& idx_sec,
                               int id, const char* tipo, const char* setor, const char* marca, float valor) {
    // 1. Busca no índice primário para garantir unicidade da chave ativa
    fseek(fp_btree, 0, SEEK_SET);
    int rrn_raiz;
    fread(&rrn_raiz, sizeof(int), 1, fp_btree);
    
    int rrn_existente = buscar_na_arvore_b(fp_btree, rrn_raiz, id);
    if (rrn_existente != -1) {
        // Lê registro de dados para certificar que está ativo
        long offset = sizeof(int) + rrn_existente * sizeof(Ativo);
        fseek(fp_dados, offset, SEEK_SET);
        Ativo reg_check;
        if (fread(&reg_check, sizeof(Ativo), 1, fp_dados) == 1 && !esta_removido(reg_check)) {
            std::cout << "[Insercao] Erro: Ativo com ID " << id << " ja existe e esta ATIVO no RRN " << rrn_existente << "!\n";
            return;
        }
    }
    
    // 2. Prepara o Ativo
    Ativo a;
    a.patrimonio_id = id;
    std::strncpy(a.tipo_equipamento, tipo, sizeof(a.tipo_equipamento) - 1);
    a.tipo_equipamento[sizeof(a.tipo_equipamento) - 1] = '\0';
    std::strncpy(a.setor_alocacao, setor, sizeof(a.setor_alocacao) - 1);
    a.setor_alocacao[sizeof(a.setor_alocacao) - 1] = '\0';
    std::strncpy(a.marca_modelo, marca, sizeof(a.marca_modelo) - 1);
    a.marca_modelo[sizeof(a.marca_modelo) - 1] = '\0';
    a.valor_compra = valor;
    
    std::cout << "[Insercao] Inserindo ID: " << id << " | Tipo: " << tipo << " | Marca: " << marca << "\n";
    
    // 3. Escreve no arquivo de dados e obtém o RRN
    int rrn_dado = inserir_ativo_dados(fp_dados, a);
    
    // 4. Insere a chave primária na Árvore B (índice primário)
    inserir_na_arvore_b(fp_btree, id, rrn_dado);
    
    // 5. Insere no índice secundário em RAM e lista invertida em disco
    inserir_indice_secundario(idx_sec, fp_inv, tipo, id);
    
    std::cout << "[Insercao] Sucesso! Gravado no RRN de dados: " << rrn_dado << "\n\n";
}

// Remove um ativo do sistema (atualiza o arquivo de dados e a LED)
void remover_registro_completo(FILE* fp_dados, FILE* fp_btree, int id_alvo) {
    std::cout << ">>> INICIANDO REMOCAO DO ATIVO ID: " << id_alvo << " <<<\n";
    
    // 1. Busca a chave na Árvore B para saber qual o RRN de dados correspondente
    fseek(fp_btree, 0, SEEK_SET);
    int rrn_raiz;
    fread(&rrn_raiz, sizeof(int), 1, fp_btree);
    
    int rrn_dado = buscar_na_arvore_b(fp_btree, rrn_raiz, id_alvo);
    if (rrn_dado == -1) {
        std::cout << "[Remocao] Erro: Ativo ID " << id_alvo << " nao encontrado na Arvore B (ou ja removido).\n\n";
        return;
    }
    
    // 2. Lê do arquivo de dados para verificar estado ativo
    long offset = sizeof(int) + rrn_dado * sizeof(Ativo);
    fseek(fp_dados, offset, SEEK_SET);
    Ativo a;
    if (fread(&a, sizeof(Ativo), 1, fp_dados) == 1) {
        if (esta_removido(a)) {
            std::cout << "[Remocao] Erro: Ativo ID " << id_alvo << " ja esta removido logicamente no RRN " << rrn_dado << ".\n\n";
            return;
        }
    } else {
        std::cout << "[Remocao] Erro físico ao ler registro no RRN " << rrn_dado << ".\n\n";
        return;
    }
    
    // 3. Remove logicamente no arquivo de dados (alimentando a LED)
    remover_ativo_dados(fp_dados, id_alvo, rrn_dado);
    std::cout << "[Remocao] ID " << id_alvo << " removido do sistema com sucesso.\n\n";
}

// Busca utilizando o índice secundário e lista invertida (Loosely Binding)
void buscar_por_tipo(FILE* fp_dados, FILE* fp_btree, FILE* fp_inv, const std::vector<IndiceSecundario>& idx_sec, const char* tipo) {
    std::cout << ">>> BUSCA SECUNDARIA PELO TIPO: '" << tipo << "' <<<\n";
    
    std::vector<int> ids_encontrados;
    bool encontrou = buscar_indice_secundario(idx_sec, fp_inv, tipo, ids_encontrados);
    
    if (!encontrou || ids_encontrados.empty()) {
        std::cout << "[Busca Secundaria] Chave '" << tipo << "' nao encontrada no indice secundario.\n\n";
        return;
    }
    
    // Carrega a raiz atual da Árvore B
    fseek(fp_btree, 0, SEEK_SET);
    int rrn_raiz;
    fread(&rrn_raiz, sizeof(int), 1, fp_btree);
    
    std::cout << "[Busca Secundaria] Resolvendo " << ids_encontrados.size() << " chaves primarias via Arvore B:\n";
    int count_ativos = 0;
    
    for (int id : ids_encontrados) {
        // Resolve o ID na B-Tree para obter o RRN correspondente
        int rrn_dado = buscar_na_arvore_b(fp_btree, rrn_raiz, id);
        if (rrn_dado == -1) {
            std::cout << "  -> ID " << id << ": Nao localizado na Arvore B (pode nao ter sido indexado).\n";
            continue;
        }
        
        // Lê o registro de dados principal correspondente
        long offset = sizeof(int) + rrn_dado * sizeof(Ativo);
        fseek(fp_dados, offset, SEEK_SET);
        Ativo a;
        if (fread(&a, sizeof(Ativo), 1, fp_dados) != 1) {
            std::cout << "  -> ID " << id << " no RRN " << rrn_dado << ": Erro de leitura fisica.\n";
            continue;
        }
        
        // Verifica se o ativo foi deletado logicamente (Loosely Binding filtra aqui)
        if (esta_removido(a)) {
            std::cout << "  -> ID " << id << " no RRN " << rrn_dado << ": REMOVIDO LOGICAMENTE (Ignorado via Loosely Binding).\n";
            continue;
        }
        
        // Registro válido e ativo encontrado
        std::cout << "  -> [ATIVO ENCONTRADO] ID: " << a.patrimonio_id 
                  << " | Tipo: " << a.tipo_equipamento 
                  << " | Setor: " << a.setor_alocacao 
                  << " | Marca/Modelo: " << a.marca_modelo 
                  << " | Valor: R$ " << a.valor_compra 
                  << " (RRN Físico: " << rrn_dado << ")\n";
        count_ativos++;
    }
    std::cout << "[Busca Secundaria] Fim da busca. Ativos correspondentes ativos: " << count_ativos << "\n\n";
}

// Depuração do arquivo de dados principal em disco
void depurar_dados(const char* nome_arquivo) {
    FILE* fp = fopen(nome_arquivo, "rb");
    if (!fp) return;
    
    int topo_led;
    fread(&topo_led, sizeof(int), 1, fp);
    
    std::cout << "\n========================================================================\n";
    std::cout << "                       ARQUIVO DE DADOS PRINCIPAL                       \n";
    std::cout << "========================================================================\n";
    std::cout << " CABECALHO (Offset 0): Topo da LED = RRN " << topo_led << "\n";
    std::cout << "------------------------------------------------------------------------\n";
    
    Ativo reg;
    int rrn = 0;
    while (fread(&reg, sizeof(Ativo), 1, fp) == 1) {
        long offset = sizeof(int) + rrn * sizeof(Ativo);
        std::cout << " RRN [" << rrn << "] | Offset: " << offset << " bytes\n";
        if (esta_removido(reg)) {
            int proximo = decodificar_proximo_rrn(reg.patrimonio_id);
            std::cout << "   ESTADO: [REMOVIDO LOGICAMENTE] -> Proximo RRN na LED: " << proximo << "\n";
        } else {
            std::cout << "   ESTADO: [ATIVO] | ID = " << reg.patrimonio_id 
                      << " | Tipo = " << reg.tipo_equipamento 
                      << " | Setor = " << reg.setor_alocacao 
                      << " | Marca/Modelo = " << reg.marca_modelo 
                      << " | Valor = R$ " << reg.valor_compra << "\n";
        }
        std::cout << "------------------------------------------------------------------------\n";
        rrn++;
    }
    std::cout << "========================================================================\n\n";
    fclose(fp);
}

int main() {
    // Configura nomes dos arquivos do sistema
    const char* nome_dados = "ativos_inventario.bin";
    const char* nome_btree = "ativos_index.btree";
    const char* nome_sec = "tipo.sec";
    const char* nome_inv = "tipo.inv";
    
    std::cout << "=========================================================\n";
    std::cout << "   TESTE DE INDEXACAO SECUNDARIA COM LISTA INVERTIDA     \n";
    std::cout << "            ESTRATEGIA: LOOSELY BINDING                 \n";
    std::cout << "=========================================================\n\n";
    
    // Removendo arquivos anteriores para iniciar o teste limpo
    std::remove(nome_dados);
    std::remove(nome_btree);
    std::remove(nome_sec);
    std::remove(nome_inv);
    
    // 1. Inicialização de todos os arquivos
    std::cout << ">>> PASSO 1: Inicializando arquivos do sistema <<<\n";
    inicializar_arquivo_dados(nome_dados);
    inicializar_arvore_b(nome_btree);
    inicializar_lista_invertida(nome_inv);
    std::cout << "Inicializacao concluida.\n\n";
    
    // Vetor em RAM para manter o índice secundário carregado
    std::vector<IndiceSecundario> idx_sec;
    
    // Carrega o índice secundário na RAM (neste caso, iniciará vazio)
    carregar_indice_secundario(nome_sec, idx_sec);
    
    // Abrindo arquivos em modo leitura/escrita binária
    FILE* fp_dados = fopen(nome_dados, "rb+");
    FILE* fp_btree = fopen(nome_btree, "rb+");
    FILE* fp_inv = fopen(nome_inv, "rb+");
    
    if (!fp_dados || !fp_btree || !fp_inv) {
        std::cerr << "Falha ao abrir os arquivos para execucao.\n";
        return 1;
    }
    
    // 2. Inserindo 6 Ativos para testar duplicações e encadeamento
    std::cout << "\n>>> PASSO 2: Inserindo 6 registros no sistema <<<\n";
    
    // Inserindo Notebooks (IDs 101, 103, 106)
    inserir_registro_completo(fp_dados, fp_btree, fp_inv, idx_sec, 101, "Notebook", "TI", "Dell Latitude 3420", 4500.00f);
    inserir_registro_completo(fp_dados, fp_btree, fp_inv, idx_sec, 103, "Notebook", "RH", "HP ProBook 440", 3800.00f);
    
    // Inserindo Monitores (IDs 102, 105)
    inserir_registro_completo(fp_dados, fp_btree, fp_inv, idx_sec, 102, "Monitor", "Marketing", "LG UltraWide 29", 1200.50f);
    inserir_registro_completo(fp_dados, fp_btree, fp_inv, idx_sec, 105, "Monitor", "Financeiro", "Samsung T350", 950.00f);
    
    // Inserindo Servidor (ID 104)
    inserir_registro_completo(fp_dados, fp_btree, fp_inv, idx_sec, 104, "Servidor", "Infra", "Dell PowerEdge T150", 12500.00f);
    
    // Inserindo mais um Notebook (ID 106)
    inserir_registro_completo(fp_dados, fp_btree, fp_inv, idx_sec, 106, "Notebook", "Vendas", "Lenovo ThinkPad E14", 5100.00f);
    
    // Depura estado inicial de disco e RAM
    depurar_indice_secundario(nome_sec, nome_inv, idx_sec);
    
    // 3. Teste de Busca Secundária para tipos duplicados
    std::cout << "\n>>> PASSO 3: Efetuando buscas secundarias no sistema <<<\n";
    // Notebook deve retornar IDs [106, 103, 101] nessa ordem (LIFO da lista invertida)
    buscar_por_tipo(fp_dados, fp_btree, fp_inv, idx_sec, "Notebook");
    
    // Monitor deve retornar IDs [105, 102]
    buscar_por_tipo(fp_dados, fp_btree, fp_inv, idx_sec, "Monitor");
    
    // Servidor deve retornar ID [104]
    buscar_por_tipo(fp_dados, fp_btree, fp_inv, idx_sec, "Servidor");
    
    // Teste com tipo inexistente
    buscar_por_tipo(fp_dados, fp_btree, fp_inv, idx_sec, "Teclado");
    
    // 4. Teste do Loosely Binding ao remover logicamente um registro
    std::cout << "\n>>> PASSO 4: Removendo logicamente o Notebook HP (ID 103) <<<\n";
    remover_registro_completo(fp_dados, fp_btree, 103);
    
    // Mostra como o arquivo de dados se comporta (RRN 1 deletado, adicionado à LED)
    depurar_dados(nome_dados);
    
    // Executa busca por Notebook novamente para mostrar o Loosely Binding em acao.
    // O ID 103 ainda constará na lista invertida física em tipo.inv,
    // mas a busca secundária irá ignorá-lo porque lerá do disco que ele está removido logicamente!
    std::cout << ">>> PASSO 5: Efetuando nova busca por 'Notebook' para mostrar o Loosely Binding <<<\n";
    buscar_por_tipo(fp_dados, fp_btree, fp_inv, idx_sec, "Notebook");
    
    // 5. Testando o reaproveitamento de espaço com a LED inserindo um novo Ativo
    std::cout << "\n>>> PASSO 6: Inserindo novo Ativo (ID 107 - Notebook) para verificar LED e reaproveitamento <<<\n";
    // Como o RRN 1 está vago (LED topo apontava para 1), o novo Notebook (ID 107) deve ocupar o RRN 1.
    inserir_registro_completo(fp_dados, fp_btree, fp_inv, idx_sec, 107, "Notebook", "TI", "Acer Aspire 5", 3500.00f);
    
    depurar_dados(nome_dados);
    depurar_indice_secundario(nome_sec, nome_inv, idx_sec);
    
    // Busca novamente por Notebook: a lista agora deve mostrar o ID 107 no RRN 1
    buscar_por_tipo(fp_dados, fp_btree, fp_inv, idx_sec, "Notebook");
    
    // 6. Fechando os arquivos e salvando o índice secundário ordenado no disco
    fclose(fp_dados);
    fclose(fp_btree);
    fclose(fp_inv);
    
    std::cout << ">>> PASSO 7: Fechando arquivos e salvando 'tipo.sec' no disco <<<\n";
    salvar_indice_secundario(nome_sec, idx_sec);
    
    // 7. Simular uma nova inicialização (carregar dados do disco de novo)
    std::cout << "\n>>> PASSO 8: Simulando reinicializacao do sistema (recarga de indice secundario) <<<\n";
    std::vector<IndiceSecundario> novo_idx_sec;
    carregar_indice_secundario(nome_sec, novo_idx_sec);
    
    // Abre novamente para busca rápida no arquivo recém-carregado
    FILE* fp_dados_reaberto = fopen(nome_dados, "rb+");
    FILE* fp_btree_reaberto = fopen(nome_btree, "rb+");
    FILE* fp_inv_reaberto = fopen(nome_inv, "rb+");
    
    if (fp_dados_reaberto && fp_btree_reaberto && fp_inv_reaberto) {
        buscar_por_tipo(fp_dados_reaberto, fp_btree_reaberto, fp_inv_reaberto, novo_idx_sec, "Notebook");
        
        fclose(fp_dados_reaberto);
        fclose(fp_btree_reaberto);
        fclose(fp_inv_reaberto);
    }
    
    std::cout << "\nPrograma de testes concluido com sucesso.\n";
    return 0;
}
