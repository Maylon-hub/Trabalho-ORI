#include <iostream>
#include <cstdio>
#include <cstring>

/**
 * =================================================================================
 * TRABALHO PRÁTICO: MOTOR DE PERSISTÊNCIA COM LISTA DE ESPAÇOS DISPONÍVEIS (LED)
 * DISCIPLINA: Organização e Recuperação de Informação (ORI)
 * =================================================================================
 * 
 * Este programa demonstra a persistência de registros de tamanho fixo em disco,
 * contendo controle de remoção lógica e reaproveitamento de espaço por meio de
 * uma LED (Lista de Espaços Disponíveis) estruturada como Pilha LIFO (Last-In, First-Out).
 */

// Garante o alinhamento de 1 byte para que a struct tenha exatamente o tamanho calculado,
// evitando que o compilador adicione "padding" (preenchimento de bytes extras para alinhamento de memória).
#pragma pack(push, 1)
struct Ativo {
    int patrimonio_id;         // 4 bytes (Chave Primária se >= 0, ou Ponteiro da LED se < 0)
    char tipo_equipamento[20]; // 20 bytes (ex: "Notebook", "Monitor")
    char setor_alocacao[20];   // 20 bytes (ex: "TI", "Financeiro")
    char marca_modelo[40];     // 40 bytes (ex: "Dell Latitude 3420")
    float valor_compra;        // 4 bytes (ex: 4500.50)
};
#pragma pack(pop)

// Validação em tempo de compilação: 4 + 20 + 20 + 40 + 4 = 88 bytes.
static_assert(sizeof(Ativo) == 88, "A struct Ativo deve ter exatamente 88 bytes de tamanho fixo!");

/**
 * ESTRATÉGIA DE CODIFICAÇÃO DA LED NO CAMPO patrimonio_id:
 * 
 * Para sabermos se um registro está ativo ou deletado na varredura sequencial, precisamos
 * que o 'patrimonio_id' de um registro deletado seja negativo.
 * Ao mesmo tempo, precisamos armazenar o RRN do próximo registro da LED (que pode ser 0, 1, 2, ... ou -1 para fim da lista).
 * 
 * Se simplesmente gravássemos o RRN positivo (ex: 2), um scan sequencial acharia que o registro está ativo com ID = 2.
 * Portanto, codificamos o próximo RRN de forma negativa usando a fórmula:
 * 
 *    patrimonio_id = -(proximo_rrn + 2)
 * 
 * Tabela de mapeamento:
 *  - Se proximo_rrn = -1 (fim da LED) -> patrimonio_id = -(-1 + 2) = -1
 *  - Se proximo_rrn = 0               -> patrimonio_id = -(0 + 2)  = -2
 *  - Se proximo_rrn = 1               -> patrimonio_id = -(1 + 2)  = -3
 *  - Se proximo_rrn = N               -> patrimonio_id = -(N + 2)
 * 
 * Assim:
 *  - Se patrimonio_id >= 0: O registro está ATIVO e este é o seu ID de patrimônio.
 *  - Se patrimonio_id < 0: O registro está DELETADO.
 *                          Para obter o próximo RRN da LED: proximo_rrn = -patrimonio_id - 2.
 */

// Função auxiliar para verificar se um registro está logicamente removido
bool esta_removido(const Ativo& a) {
    return a.patrimonio_id < 0;
}

// Codifica o próximo RRN da LED para ser armazenado no patrimonio_id do registro deletado
int codificar_proximo_rrn(int proximo_rrn) {
    return -(proximo_rrn + 2);
}

// Decodifica o próximo RRN da LED a partir do patrimonio_id do registro deletado
int decodificar_proximo_rrn(int patrimonio_id) {
    return -patrimonio_id - 2;
}

/**
 * 1. Inicializar Arquivo
 * Cria o arquivo caso não exista e inicializa o cabeçalho de 4 bytes (int) 
 * que guarda o topo da LED como -1.
 */
void inicializar_arquivo(const char* nome_arquivo) {
    // Tenta abrir para leitura e escrita binária ("rb+")
    FILE* fp = fopen(nome_arquivo, "rb+");
    
    if (!fp) {
        // Se falhou, o arquivo provavelmente não existe. Vamos criá-lo com "wb+"
        fp = fopen(nome_arquivo, "wb+");
        if (!fp) {
            std::perror("[Erro] Falha ao criar o arquivo de dados");
            return;
        }
        
        int topo_inicial = -1;
        // fseek não é necessário aqui pois o arquivo acabou de ser criado e está no offset 0.
        // Gravamos 4 bytes contendo -1 para representar que a LED está vazia.
        fwrite(&topo_inicial, sizeof(int), 1, fp);
        
        std::cout << "[Inicializar] Arquivo '" << nome_arquivo << "' criado.\n";
        std::cout << "              -> Cabecalho de 4 bytes gravado no offset 0 com valor: " << topo_inicial << " (LED Vazia)\n";
    } else {
        std::cout << "[Inicializar] Arquivo '" << nome_arquivo << "' ja existe. Pronto para operacoes.\n";
    }
    
    fclose(fp);
}

/**
 * 2. Inserir Ativo
 * Checa o cabeçalho. Se a LED não estiver vazia (topo != -1), desempilha o RRN do topo,
 * atualiza o cabeçalho para apontar para o próximo RRN da pilha e sobrescreve o Ativo naquele espaço (Reaproveitamento O(1)).
 * Se a LED estiver vazia, grava o registro no final do arquivo físico.
 */
void inserir_ativo(FILE* fp, Ativo novo_ativo) {
    if (novo_ativo.patrimonio_id < 0) {
        std::cout << "[Inserir] Erro: O ID do patrimonio deve ser maior ou igual a zero.\n";
        return;
    }

    // 1. Ler o topo da LED no cabeçalho (offset 0)
    fseek(fp, 0, SEEK_SET); 
    int topo_led;
    fread(&topo_led, sizeof(int), 1, fp);
    
    std::cout << "[Inserir] Lendo cabecalho no offset 0. Topo da LED = " << topo_led << "\n";

    if (topo_led != -1) {
        // --- CASO 1: Reaproveitar espaço (LED não está vazia) ---
        // O topo_led nos dá o RRN (Relative Record Number) do registro logicamente removido.
        int rrn_reaproveitado = topo_led;
        
        // Calcula o offset do registro que será reaproveitado:
        // Cabeçalho (4 bytes) + (RRN * tamanho do registro)
        long offset_registro = sizeof(int) + rrn_reaproveitado * sizeof(Ativo);
        
        // Movemos o cursor para ler o registro deletado e obter o próximo RRN da LED
        fseek(fp, offset_registro, SEEK_SET);
        Ativo registro_deletado;
        fread(&registro_deletado, sizeof(Ativo), 1, fp);
        
        // Decodificamos o próximo RRN guardado no patrimonio_id do registro deletado
        int proximo_rrn = decodificar_proximo_rrn(registro_deletado.patrimonio_id);
        
        // Movemos o cursor de volta ao início deste slot para escrever o novo ativo por cima
        fseek(fp, offset_registro, SEEK_SET);
        fwrite(&novo_ativo, sizeof(Ativo), 1, fp);
        
        // Atualizamos o cabeçalho do arquivo para apontar para o próximo RRN da LED
        fseek(fp, 0, SEEK_SET);
        fwrite(&proximo_rrn, sizeof(int), 1, fp);
        
        std::cout << "[Inserir] REAPROVEITAMENTO no RRN " << rrn_reaproveitado << " (offset " << offset_registro << " bytes).\n";
        std::cout << "          -> Proximo RRN da LED desempilhado: " << proximo_rrn << " (gravado no cabecalho).\n";
    } else {
        // --- CASO 2: Gravar no final do arquivo (LED vazia) ---
        // Posiciona o ponteiro no final do arquivo físico
        fseek(fp, 0, SEEK_END);
        long offset_fim = ftell(fp); // Pega a posição atual do ponteiro (tamanho total do arquivo em bytes)
        
        // Calcula qual será o RRN desse novo registro
        int rrn_novo = (offset_fim - sizeof(int)) / sizeof(Ativo);
        
        // Escreve o novo registro no fim do arquivo
        fwrite(&novo_ativo, sizeof(Ativo), 1, fp);
        
        std::cout << "[Inserir] FIM DO ARQUIVO no RRN " << rrn_novo << " (offset " << offset_fim << " bytes).\n";
    }
    
    // Força a escrita dos buffers do sistema operacional para o disco físico
    fflush(fp);
}

/**
 * 3. Buscar Ativo Sequencial
 * Realiza uma varredura sequencial direta no disco em busca do Ativo com o patrimonio_id correspondente,
 * pulando registros que estão logicamente deletados.
 */
Ativo buscar_ativo_sequencial(FILE* fp, int id_alvo) {
    // Posiciona o ponteiro logo após o cabeçalho (offset de 4 bytes)
    fseek(fp, sizeof(int), SEEK_SET);
    
    Ativo reg;
    int rrn = 0;
    
    std::cout << "[Buscar] Iniciando varredura sequencial a partir do offset " << sizeof(int) << "...\n";
    
    // Lê registro por registro sequencialmente até o fim do arquivo
    while (fread(&reg, sizeof(Ativo), 1, fp) == 1) {
        long offset_atual = sizeof(int) + rrn * sizeof(Ativo);
        
        // Se o registro não estiver deletado e o ID coincidir com o alvo
        if (!esta_removido(reg)) {
            if (reg.patrimonio_id == id_alvo) {
                std::cout << "[Buscar] SUCESSO: Ativo com ID " << id_alvo << " encontrado no RRN " << rrn 
                          << " (offset " << offset_atual << " bytes).\n";
                return reg;
            }
        } else {
            // Depuração pedagógica mostrando que o registro deletado foi pulado
            int prox = decodificar_proximo_rrn(reg.patrimonio_id);
            std::cout << "[Buscar] Ignorando registro deletado no RRN " << rrn 
                      << " (Ponteiro LED: RRN " << prox << ").\n";
        }
        rrn++;
    }
    
    std::cout << "[Buscar] AVISO: Ativo com ID " << id_alvo << " nao encontrado no arquivo.\n";
    
    // Retorna uma struct sentinela com patrimonio_id = -999 para indicar "não encontrado"
    Ativo nao_encontrado;
    nao_encontrado.patrimonio_id = -999;
    std::memset(nao_encontrado.tipo_equipamento, 0, sizeof(nao_encontrado.tipo_equipamento));
    std::memset(nao_encontrado.setor_alocacao, 0, sizeof(nao_encontrado.setor_alocacao));
    std::memset(nao_encontrado.marca_modelo, 0, sizeof(nao_encontrado.marca_modelo));
    nao_encontrado.valor_compra = 0.0f;
    
    return nao_encontrado;
}

/**
 * 4. Remover Ativo Lógico
 * Localiza o registro. Aplica a remoção lógica. Lê o antigo topo da LED no cabeçalho
 * e faz o campo 'patrimonio_id' deste registro recém-removido apontar para ele (codificado).
 * Atualiza o cabeçalho do arquivo para que o topo aponte para o RRN deste novo buraco (Estratégia LIFO Pilha O(1)).
 */
void remover_ativo_logico(FILE* fp, int id_alvo) {
    // 1. Localizar o RRN do registro a ser removido (Varredura Sequencial)
    fseek(fp, sizeof(int), SEEK_SET);
    Ativo reg;
    int rrn_alvo = -1;
    int rrn_atual = 0;
    
    while (fread(&reg, sizeof(Ativo), 1, fp) == 1) {
        if (!esta_removido(reg) && reg.patrimonio_id == id_alvo) {
            rrn_alvo = rrn_atual;
            break;
        }
        rrn_atual++;
    }
    
    if (rrn_alvo == -1) {
        std::cout << "[Remover] Erro: Ativo com ID " << id_alvo << " nao encontrado ou ja removido.\n";
        return;
    }
    
    // 2. Ler o cabeçalho para obter o antigo topo da LED
    fseek(fp, 0, SEEK_SET);
    int antigo_topo;
    fread(&antigo_topo, sizeof(int), 1, fp);
    
    // 3. Preparar o registro para a remoção lógica
    // O campo patrimonio_id passará a guardar o antigo_topo codificado de forma negativa
    reg.patrimonio_id = codificar_proximo_rrn(antigo_topo);
    
    // Opcional: Limpar os outros campos para melhor controle visual ou segurança dos dados
    std::strcpy(reg.tipo_equipamento, "[DELETADO]");
    std::strcpy(reg.setor_alocacao, "[LED]");
    std::strcpy(reg.marca_modelo, "[DISPONIVEL]");
    reg.valor_compra = 0.0f;
    
    // 4. Gravar o registro modificado de volta no mesmo RRN
    long offset_registro = sizeof(int) + rrn_alvo * sizeof(Ativo);
    fseek(fp, offset_registro, SEEK_SET);
    fwrite(&reg, sizeof(Ativo), 1, fp);
    
    // 5. Atualizar o cabeçalho do arquivo para que o topo aponte para o RRN do registro removido (rrn_alvo)
    fseek(fp, 0, SEEK_SET);
    fwrite(&rrn_alvo, sizeof(int), 1, fp);
    
    // Garante gravação imediata
    fflush(fp);
    
    std::cout << "[Remover] SUCESSO: Registro ID " << id_alvo << " no RRN " << rrn_alvo << " foi removido logicamente.\n";
    std::cout << "          -> O campo patrimonio_id do RRN " << rrn_alvo << " agora aponta para o antigo topo: RRN " << antigo_topo << ".\n";
    std::cout << "          -> O cabecalho do arquivo foi atualizado para apontar para o novo topo: RRN " << rrn_alvo << ".\n";
}

/**
 * Função Auxiliar de Visualização (Depuração)
 * Varre todo o arquivo físico mostrando o cabeçalho e cada bloco de registro
 * no disco, permitindo acompanhar didaticamente o funcionamento da LED.
 */
void depurar_arquivo(const char* nome_arquivo) {
    FILE* fp = fopen(nome_arquivo, "rb");
    if (!fp) {
        std::cout << "[Depuracao] Nao foi possivel abrir o arquivo para leitura.\n";
        return;
    }
    
    int topo_led;
    fread(&topo_led, sizeof(int), 1, fp);
    
    std::cout << "\n========================================================================\n";
    std::cout << "                       ESTADO DO DISCO EM TEMPO REAL                    \n";
    std::cout << "========================================================================\n";
    std::cout << " CABECALHO (Offset 0): Topo da LED = RRN " << topo_led << "\n";
    std::cout << " Tamanho do registro (Ativo): " << sizeof(Ativo) << " bytes\n";
    std::cout << "------------------------------------------------------------------------\n";
    
    Ativo reg;
    int rrn = 0;
    while (fread(&reg, sizeof(Ativo), 1, fp) == 1) {
        long offset = sizeof(int) + rrn * sizeof(Ativo);
        std::cout << " RRN [" << rrn << "] | Offset: " << offset << " bytes\n";
        if (esta_removido(reg)) {
            int proximo = decodificar_proximo_rrn(reg.patrimonio_id);
            std::cout << "   ESTADO: [REMOVIDO LOGICAMENTE] -> Proximo RRN na LED: " << proximo << "\n";
            std::cout << "   Valores no disco: ID = " << reg.patrimonio_id << ", Tipo = " << reg.tipo_equipamento << "\n";
        } else {
            std::cout << "   ESTADO: [ATIVO]\n";
            std::cout << "   Valores: ID = " << reg.patrimonio_id 
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

// Helper para criar um objeto Ativo de forma limpa
Ativo criar_ativo(int id, const char* tipo, const char* setor, const char* marca, float valor) {
    Ativo a;
    a.patrimonio_id = id;
    std::strncpy(a.tipo_equipamento, tipo, sizeof(a.tipo_equipamento) - 1);
    a.tipo_equipamento[sizeof(a.tipo_equipamento) - 1] = '\0'; // Garante terminação nula
    
    std::strncpy(a.setor_alocacao, setor, sizeof(a.setor_alocacao) - 1);
    a.setor_alocacao[sizeof(a.setor_alocacao) - 1] = '\0';
    
    std::strncpy(a.marca_modelo, marca, sizeof(a.marca_modelo) - 1);
    a.marca_modelo[sizeof(a.marca_modelo) - 1] = '\0';
    
    a.valor_compra = valor;
    return a;
}

int main() {
    const char* nome_arquivo = "ativos_inventario.bin";
    
    std::cout << "=========================================================\n";
    std::cout << "     TESTE DO MOTOR DE PERSISTENCIA COM LED (LIFO)       \n";
    std::cout << "=========================================================\n\n";
    
    // Removendo arquivo anterior se houver para começar o teste do zero
    std::remove(nome_arquivo);
    
    // 1. Inicialização do arquivo
    inicializar_arquivo(nome_arquivo);
    
    // Abrindo o arquivo para manipulação
    FILE* fp = fopen(nome_arquivo, "rb+");
    if (!fp) {
        std::cerr << "Erro ao abrir o arquivo para testes.\n";
        return 1;
    }
    
    // Depuração inicial
    depurar_arquivo(nome_arquivo);
    
    // 2. Inserindo 3 Ativos
    std::cout << ">>> PASSO 1: Inserindo 3 Ativos iniciais (LED vazia) <<<\n";
    inserir_ativo(fp, criar_ativo(101, "Notebook", "TI", "Dell Latitude 3420", 4500.00f));
    inserir_ativo(fp, criar_ativo(102, "Monitor", "RH", "LG UltraWide 29", 1200.50f));
    inserir_ativo(fp, criar_ativo(103, "Servidor", "Infra", "HP ProLiant DL380", 25000.00f));
    
    depurar_arquivo(nome_arquivo);
    
    // 3. Teste de Busca Sequencial
    std::cout << ">>> PASSO 2: Buscando Ativo ID 102 <<<\n";
    Ativo busca1 = buscar_ativo_sequencial(fp, 102);
    if (busca1.patrimonio_id != -999) {
        std::cout << "Resultado da busca -> ID: " << busca1.patrimonio_id << ", Marca: " << busca1.marca_modelo << "\n\n";
    }
    
    // 4. Teste de Remoção Lógica (LIFO)
    std::cout << ">>> PASSO 3: Removendo logicamente o Monitor (ID 102, RRN 1) <<<\n";
    remover_ativo_logico(fp, 102);
    depurar_arquivo(nome_arquivo);
    
    std::cout << ">>> PASSO 4: Removendo logicamente o Notebook (ID 101, RRN 0) <<<\n";
    remover_ativo_logico(fp, 101);
    depurar_arquivo(nome_arquivo);
    
    // Veja que o topo da LED agora aponta para o RRN 0, e o RRN 0 aponta para o RRN 1, que aponta para -1 (fim).
    // Ou seja: LED -> RRN 0 -> RRN 1 -> Fim (-1).
    
    // 5. Teste de Reaproveitamento de Espaço (LED)
    std::cout << ">>> PASSO 5: Inserindo novo Ativo (ID 104 - Teclado) para testar o reaproveitamento <<<\n";
    // O novo registro deve ocupar o topo da LED, que é o RRN 0 (Notebook deletado).
    // O topo da LED deve então passar a apontar para RRN 1.
    inserir_ativo(fp, criar_ativo(104, "Teclado", "Financeiro", "Logitech MX Keys", 600.00f));
    depurar_arquivo(nome_arquivo);
    
    std::cout << ">>> PASSO 6: Inserindo mais um Ativo (ID 105 - Mouse) <<<\n";
    // O novo registro deve ocupar o RRN 1 (Monitor deletado).
    // O topo da LED deve voltar a ser -1 (vazia).
    inserir_ativo(fp, criar_ativo(105, "Mouse", "Vendas", "Logitech MX Master", 450.00f));
    depurar_arquivo(nome_arquivo);
    
    std::cout << ">>> PASSO 7: Inserindo Ativo (ID 106 - Switch) com a LED ja vazia <<<\n";
    // Como a LED está vazia (-1), este registro deve ser inserido no final do arquivo (RRN 3).
    inserir_ativo(fp, criar_ativo(106, "Switch", "TI", "Cisco Catalyst 24P", 8000.00f));
    depurar_arquivo(nome_arquivo);

    // 6. Fechando o arquivo
    fclose(fp);
    std::cout << "Programa de teste encerrado com sucesso. Arquivos gravados no disco.\n";
    
    return 0;
}
