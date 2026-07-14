#include <iostream>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

#include "ativos.h"
#include "arvore_b.hpp"
#include "lista_invertida.hpp"

// Validação em tempo de compilação da struct Ativo
static_assert(sizeof(Ativo) == 88, "A struct Ativo deve ter exatamente 88 bytes de tamanho fixo!");

// --- FUNÇÕES DE SUPORTE A LOGICAL DELETION (LED) ---

inline bool esta_removido(const Ativo& a) {
    return a.patrimonio_id < 0;
}

inline int codificar_proximo_rrn(int proximo_rrn) {
    return -(proximo_rrn + 2);
}

inline int decodificar_proximo_rrn(int patrimonio_id) {
    return -patrimonio_id - 2;
}

// Inicializa o arquivo de dados com LED vazia (-1) caso não exista
void inicializar_arquivo_dados(const char* nome_arquivo) {
    FILE* fp = fopen(nome_arquivo, "rb+");
    if (!fp) {
        fp = fopen(nome_arquivo, "wb+");
        if (!fp) {
            std::perror("[Erro] Falha ao criar arquivo de dados principal");
            return;
        }
        int topo_inicial = -1;
        fwrite(&topo_inicial, sizeof(int), 1, fp);
        std::cout << "[Dados] Arquivo '" << nome_arquivo << "' inicializado com LED = -1.\n";
    }
    if (fp) fclose(fp);
}

// Insere um ativo no arquivo de dados reaproveitando espaço da LED (LIFO) ou gravando no final
int inserir_ativo(FILE* fp, Ativo novo_ativo) {
    if (novo_ativo.patrimonio_id < 0) {
        std::cout << "[Inserir] Erro: ID deve ser maior ou igual a zero.\n";
        return -1;
    }

    fseek(fp, 0, SEEK_SET); 
    int topo_led;
    fread(&topo_led, sizeof(int), 1, fp);
    
    int rrn_gravado = -1;

    if (topo_led != -1) {
        // reaproveita espaço da LED
        rrn_gravado = topo_led;
        long offset_registro = sizeof(int) + rrn_gravado * sizeof(Ativo);
        
        fseek(fp, offset_registro, SEEK_SET);
        Ativo registro_deletado;
        fread(&registro_deletado, sizeof(Ativo), 1, fp);
        
        int proximo_rrn = decodificar_proximo_rrn(registro_deletado.patrimonio_id);
        
        fseek(fp, offset_registro, SEEK_SET);
        fwrite(&novo_ativo, sizeof(Ativo), 1, fp);
        
        fseek(fp, 0, SEEK_SET);
        fwrite(&proximo_rrn, sizeof(int), 1, fp);
        
        std::cout << "[Dados] Reaproveitando RRN " << rrn_gravado << " da LED (novo topo: RRN " << proximo_rrn << ").\n";
    } else {
        // insere no final do arquivo
        fseek(fp, 0, SEEK_END);
        long offset_fim = ftell(fp);
        rrn_gravado = (offset_fim - sizeof(int)) / sizeof(Ativo);
        
        fwrite(&novo_ativo, sizeof(Ativo), 1, fp);
        std::cout << "[Dados] Gravado no FIM DO ARQUIVO no RRN " << rrn_gravado << ".\n";
    }
    
    fflush(fp);
    return rrn_gravado;
}

// Remove logicamente o ativo em disco colocando-o no topo da pilha LIFO da LED
int remover_ativo_logico(FILE* fp, int id_alvo) {
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
        return -1; // Não encontrado
    }
    
    fseek(fp, 0, SEEK_SET);
    int antigo_topo;
    fread(&antigo_topo, sizeof(int), 1, fp);
    
    reg.patrimonio_id = codificar_proximo_rrn(antigo_topo);
    std::strcpy(reg.tipo_equipamento, "[DELETADO]");
    std::strcpy(reg.setor_alocacao, "[LED]");
    std::strcpy(reg.marca_modelo, "[DISPONIVEL]");
    reg.valor_compra = 0.0f;
    
    long offset_registro = sizeof(int) + rrn_alvo * sizeof(Ativo);
    fseek(fp, offset_registro, SEEK_SET);
    fwrite(&reg, sizeof(Ativo), 1, fp);
    
    fseek(fp, 0, SEEK_SET);
    fwrite(&rrn_alvo, sizeof(int), 1, fp);
    
    fflush(fp);
    std::cout << "[Dados] Registro ID " << id_alvo << " no RRN " << rrn_alvo << " removido logicamente.\n";
    return rrn_alvo;
}

// Imprime a formatação visual de um Ativo
void imprimir_ativo(const Ativo &a) {
    std::cout << "Patrimonio : " << a.patrimonio_id << "\n";
    std::cout << "Tipo       : " << a.tipo_equipamento << "\n";
    std::cout << "Setor      : " << a.setor_alocacao << "\n";
    std::cout << "Marca      : " << a.marca_modelo << "\n";
    std::cout << "Valor      : R$ " << a.valor_compra << "\n\n";
}

// Depura sequencialmente o arquivo físico de dados mostrando a LED
void depurar_arquivo_dados(const char* nome_arquivo) {
    FILE* fp = fopen(nome_arquivo, "rb");
    if (!fp) {
        std::cout << "[Depuracao] Nao foi possivel abrir o arquivo de dados.\n";
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

// Varre o arquivo sequencialmente imprimindo apenas ativos que estão online
void listar_ativos(FILE *fp) {
    fseek(fp, sizeof(int), SEEK_SET);
    Ativo a;
    std::cout << "\n========== ATIVOS NO SISTEMA ==========\n";
    int count = 0;
    while (fread(&a, sizeof(Ativo), 1, fp) == 1) {
        if (a.patrimonio_id >= 0) {
            imprimir_ativo(a);
            std::cout << "-----------------------------\n";
            count++;
        }
    }
    if (count == 0) {
        std::cout << "(Nenhum ativo cadastrado no sistema)\n";
    }
}

// --- INTEGRAÇÃO DO CRUD COMPLETO ---

void cadastrar_ativo_completo(FILE* fpDados, FILE* fpIndice, 
                              const char* fSecTipo, const char* fLstTipo,
                              const char* fSecSetor, const char* fLstSetor,
                              Ativo novo) 
{
    // 1. Validar unicidade da Chave Primária via Árvore B
    fseek(fpIndice, 0, SEEK_SET);
    int rrn_raiz;
    fread(&rrn_raiz, sizeof(int), 1, fpIndice);
    
    int rrn_existente = buscar_na_arvore_b(fpIndice, rrn_raiz, novo.patrimonio_id);
    if (rrn_existente != -1) {
        std::cout << "[Erro] Ja existe um ativo cadastrado com o ID " << novo.patrimonio_id << ".\n";
        return;
    }

    // 2. Inserir no arquivo de dados (reaproveita LED se houver)
    int rrn_dados = inserir_ativo(fpDados, novo);
    if (rrn_dados == -1) return;

    // 3. Atualizar índice primário da Árvore B
    inserir_na_arvore_b(fpIndice, novo.patrimonio_id, rrn_dados);

    // 4. Atualizar índices secundários de Lista Invertida
    inserir_indice_secundario(fSecTipo, fLstTipo, novo.tipo_equipamento, novo.patrimonio_id);
    inserir_indice_secundario(fSecSetor, fLstSetor, novo.setor_alocacao, novo.patrimonio_id);
    
    std::cout << "\nAtivo cadastrado com sucesso no RRN " << rrn_dados << "!\n";
}

void remover_ativo_completo(FILE* fpDados, FILE* fpIndice,
                            const char* fSecTipo, const char* fLstTipo,
                            const char* fSecSetor, const char* fLstSetor,
                            int id) 
{
    // 1. Procurar na Árvore B
    fseek(fpIndice, 0, SEEK_SET);
    int rrn_raiz;
    fread(&rrn_raiz, sizeof(int), 1, fpIndice);
    
    int rrn_dados = buscar_na_arvore_b(fpIndice, rrn_raiz, id);
    if (rrn_dados == -1) {
        std::cout << "[Erro] Ativo com ID " << id << " nao encontrado ou ja removido.\n";
        return;
    }

    // 2. Ler o registro do arquivo de dados para obter campos secundários
    long offset = sizeof(int) + rrn_dados * sizeof(Ativo);
    fseek(fpDados, offset, SEEK_SET);
    Ativo reg;
    if (fread(&reg, sizeof(Ativo), 1, fpDados) != 1 || esta_removido(reg)) {
        std::cout << "[Erro] Falha ao ler registro de dados.\n";
        return;
    }

    // 3. Remover logicamente do arquivo de dados (insere na LED)
    int rrn_removido = remover_ativo_logico(fpDados, id);
    if (rrn_removido == -1) return;

    // 4. Remover logicamente do índice primário da Árvore B (rrn_dados = -1)
    remover_logico_arvore_b(fpIndice, id);

    // 5. Remover dos índices secundários
    remover_indice_secundario(fSecTipo, fLstTipo, reg.tipo_equipamento, id);
    remover_indice_secundario(fSecSetor, fLstSetor, reg.setor_alocacao, id);
    
    std::cout << "\nAtivo ID " << id << " removido logicamente com sucesso.\n";
}

void atualizar_ativo_completo(FILE* fpDados, FILE* fpIndice,
                              const char* fSecTipo, const char* fLstTipo,
                              const char* fSecSetor, const char* fLstSetor,
                              int id) 
{
    // 1. Procurar na Árvore B
    fseek(fpIndice, 0, SEEK_SET);
    int rrn_raiz;
    fread(&rrn_raiz, sizeof(int), 1, fpIndice);
    
    int rrn_dados = buscar_na_arvore_b(fpIndice, rrn_raiz, id);
    if (rrn_dados == -1) {
        std::cout << "[Erro] Ativo com ID " << id << " nao encontrado ou ja removido.\n";
        return;
    }

    // 2. Ler o registro
    long offset = sizeof(int) + rrn_dados * sizeof(Ativo);
    fseek(fpDados, offset, SEEK_SET);
    Ativo antigo;
    if (fread(&antigo, sizeof(Ativo), 1, fpDados) != 1 || esta_removido(antigo)) {
        std::cout << "[Erro] Falha ao acessar registro de dados.\n";
        return;
    }

    Ativo novo = antigo; // ID é imutável

    std::cin.ignore();
    std::cout << "Novo Tipo [" << antigo.tipo_equipamento << "]: ";
    char temp[40];
    std::cin.getline(temp, 40);
    if (std::strlen(temp) > 0) {
        std::strncpy(novo.tipo_equipamento, temp, 19);
        novo.tipo_equipamento[19] = '\0';
    }

    std::cout << "Novo Setor [" << antigo.setor_alocacao << "]: ";
    std::cin.getline(temp, 40);
    if (std::strlen(temp) > 0) {
        std::strncpy(novo.setor_alocacao, temp, 19);
        novo.setor_alocacao[19] = '\0';
    }

    std::cout << "Nova Marca/Modelo [" << antigo.marca_modelo << "]: ";
    std::cin.getline(temp, 40);
    if (std::strlen(temp) > 0) {
        std::strncpy(novo.marca_modelo, temp, 39);
        novo.marca_modelo[39] = '\0';
    }

    std::cout << "Novo Valor [" << antigo.valor_compra << "]: ";
    std::cin.getline(temp, 40);
    if (std::strlen(temp) > 0) {
        try {
            novo.valor_compra = std::stof(temp);
        } catch(...) {
            std::cout << "[Aviso] Valor invalido. Mantendo antigo.\n";
        }
    }

    // 3. Escrever novo registro de volta
    fseek(fpDados, offset, SEEK_SET);
    fwrite(&novo, sizeof(Ativo), 1, fpDados);
    fflush(fpDados);

    // 4. Se mudou de Tipo, atualiza o índice secundário
    if (std::strcmp(antigo.tipo_equipamento, novo.tipo_equipamento) != 0) {
        remover_indice_secundario(fSecTipo, fLstTipo, antigo.tipo_equipamento, id);
        inserir_indice_secundario(fSecTipo, fLstTipo, novo.tipo_equipamento, id);
    }

    // 5. Se mudou de Setor, atualiza o índice secundário
    if (std::strcmp(antigo.setor_alocacao, novo.setor_alocacao) != 0) {
        remover_indice_secundario(fSecSetor, fLstSetor, antigo.setor_alocacao, id);
        inserir_indice_secundario(fSecSetor, fLstSetor, novo.setor_alocacao, id);
    }

    std::cout << "\nAtivo ID " << id << " atualizado com sucesso!\n";
}

// --- DESAFIO DO CHEFÃO: VACUUM (DESFRAGMENTADOR E COMPACTADOR) ---

void vacuum_defragmentar(
    const char* nome_dados, 
    const char* nome_indice,
    const char* fSecTipo, const char* fLstTipo,
    const char* fSecSetor, const char* fLstSetor) 
{
    std::cout << "\n[Vacuum] Iniciando desfragmentacao fisica do disco...\n";
    
    FILE* fpDados = fopen(nome_dados, "rb");
    if (!fpDados) {
        std::cout << "[Vacuum] Erro: Arquivo de dados original nao encontrado.\n";
        return;
    }
    
    const char* temp_dados = "ativos_inventario_temp.bin";
    const char* temp_indice = "ativos_index_temp.btree";
    
    FILE* fpDadosTemp = fopen(temp_dados, "wb+");
    if (!fpDadosTemp) {
        std::cout << "[Vacuum] Erro ao criar arquivo de dados temporario.\n";
        fclose(fpDados);
        return;
    }
    // Inicializa cabeçalho com LED vazia (-1)
    int topo_inicial = -1;
    fwrite(&topo_inicial, sizeof(int), 1, fpDadosTemp);
    
    // Inicializa nova Árvore B temporária
    inicializar_arvore_b(temp_indice);
    FILE* fpIndiceTemp = fopen(temp_indice, "rb+");
    if (!fpIndiceTemp) {
        std::cout << "[Vacuum] Erro ao criar indice temporario.\n";
        fclose(fpDados);
        fclose(fpDadosTemp);
        return;
    }
    
    // Reconstruímos também os índices secundários de tipo e setor limpos de fragmentação
    const char* temp_sec_tipo = "tipo_secundario_temp.idx";
    const char* temp_lst_tipo = "tipo_lista_temp.bin";
    const char* temp_sec_setor = "setor_secundario_temp.idx";
    const char* temp_lst_setor = "setor_lista_temp.bin";
    
    inicializar_indice_secundario(temp_sec_tipo, temp_lst_tipo);
    inicializar_indice_secundario(temp_sec_setor, temp_lst_setor);
    
    // Varre o arquivo original a partir do offset 4
    fseek(fpDados, sizeof(int), SEEK_SET);
    Ativo reg;
    int rrn_novo = 0;
    int total_copiados = 0;
    
    while (fread(&reg, sizeof(Ativo), 1, fpDados) == 1) {
        if (reg.patrimonio_id >= 0) { // Se estiver ativo
            // Escreve no arquivo temporário
            fseek(fpDadosTemp, sizeof(int) + rrn_novo * sizeof(Ativo), SEEK_SET);
            fwrite(&reg, sizeof(Ativo), 1, fpDadosTemp);
            
            // Grava na nova Árvore B
            inserir_na_arvore_b(fpIndiceTemp, reg.patrimonio_id, rrn_novo);
            
            // Re-insere nas listas invertidas temporárias
            inserir_indice_secundario(temp_sec_tipo, temp_lst_tipo, reg.tipo_equipamento, reg.patrimonio_id);
            inserir_indice_secundario(temp_sec_setor, temp_lst_setor, reg.setor_alocacao, reg.patrimonio_id);
            
            rrn_novo++;
            total_copiados++;
        }
    }
    
    fclose(fpDados);
    fclose(fpDadosTemp);
    fclose(fpIndiceTemp);
    
    // Substitui arquivos antigos
    std::remove(nome_dados);
    std::remove(nome_indice);
    std::remove(fSecTipo);
    std::remove(fLstTipo);
    std::remove(fSecSetor);
    std::remove(fLstSetor);
    
    std::rename(temp_dados, nome_dados);
    std::rename(temp_indice, nome_indice);
    std::rename(temp_sec_tipo, fSecTipo);
    std::rename(temp_lst_tipo, fLstTipo);
    std::rename(temp_sec_setor, fSecSetor);
    std::rename(temp_lst_setor, fLstSetor);
    
    std::cout << "[Vacuum] Compactacao concluida com SUCESSO!\n";
    std::cout << "         " << total_copiados << " registros ativos copiados. LED zerada e indices reconstruidos.\n";
}

// --- FUNÇÃO MAIN COM MENU CLI INTERATIVO ---

int main() {
    const char* arquivo_dados = "ativos_inventario.bin";
    const char* arquivo_indice = "ativos_index.btree";
    
    const char* sec_tipo = "tipo_secundario.idx";
    const char* lst_tipo = "tipo_lista.bin";
    
    const char* sec_setor = "setor_secundario.idx";
    const char* lst_setor = "setor_lista.bin";

    // Inicialização dos arquivos físicos de armazenamento e índices
    inicializar_arquivo_dados(arquivo_dados);
    inicializar_arvore_b(arquivo_indice);
    inicializar_indice_secundario(sec_tipo, lst_tipo);
    inicializar_indice_secundario(sec_setor, lst_setor);

    FILE* fpDados = fopen(arquivo_dados, "rb+");
    FILE* fpIndice = fopen(arquivo_indice, "rb+");

    if (!fpDados || !fpIndice) {
        std::cout << "[Erro] Falha catastrófica ao abrir arquivos do SGBD.\n";
        return 1;
    }

    int opcao = -1;
    while (opcao != 0) {
        std::cout << "\n";
        std::cout << "=========================================\n";
        std::cout << "     SGBD: RASTREAMENTO DE ATIVOS TI     \n";
        std::cout << "=========================================\n";
        std::cout << "1 - Cadastrar ativo\n";
        std::cout << "2 - Buscar ativo por ID (Arvore B)\n";
        std::cout << "3 - Buscar ativos por Tipo (Lista Invertida)\n";
        std::cout << "4 - Buscar ativos por Setor (Lista Invertida)\n";
        std::cout << "5 - Atualizar ativo (Chave ID imutavel)\n";
        std::cout << "6 - Remover ativo (LED / LIFO)\n";
        std::cout << "7 - Listar todos os ativos\n";
        std::cout << "8 - Mostrar arquivo de dados (LED visual)\n";
        std::cout << "9 - Mostrar Arvore B (Estrutura hierarquica)\n";
        std::cout << "10 - Compactar arquivos (Vacuum / Defragmentar)\n";
        std::cout << "0 - Sair\n";
        std::cout << "Opcao: ";

        std::cin >> opcao;
        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(1000, '\n');
            std::cout << "\nOpcao invalida. Digite um numero.\n";
            continue;
        }

        switch (opcao) {
            case 1: {
                Ativo novo;
                std::cout << "\nPatrimonio ID: ";
                std::cin >> novo.patrimonio_id;
                if (std::cin.fail() || novo.patrimonio_id < 0) {
                    std::cin.clear();
                    std::cin.ignore(1000, '\n');
                    std::cout << "[Erro] ID deve ser um numero inteiro positivo.\n";
                    break;
                }
                std::cin.ignore();

                std::cout << "Tipo Equipamento: ";
                std::cin.getline(novo.tipo_equipamento, 20);

                std::cout << "Setor Alocacao: ";
                std::cin.getline(novo.setor_alocacao, 20);

                std::cout << "Marca/Modelo: ";
                std::cin.getline(novo.marca_modelo, 40);

                std::cout << "Valor Compra: ";
                std::cin >> novo.valor_compra;

                cadastrar_ativo_completo(fpDados, fpIndice, sec_tipo, lst_tipo, sec_setor, lst_setor, novo);
                break;
            }

            case 2: {
                int id;
                std::cout << "\nDigite o Patrimonio ID para busca: ";
                std::cin >> id;
                
                fseek(fpIndice, 0, SEEK_SET);
                int rrn_raiz;
                fread(&rrn_raiz, sizeof(int), 1, fpIndice);
                
                int rrn = buscar_na_arvore_b(fpIndice, rrn_raiz, id);
                if (rrn != -1) {
                    long offset = sizeof(int) + rrn * sizeof(Ativo);
                    fseek(fpDados, offset, SEEK_SET);
                    Ativo a;
                    if (fread(&a, sizeof(Ativo), 1, fpDados) == 1 && !esta_removido(a)) {
                        std::cout << "\n--- REGISTRO ENCONTRADO (RRN " << rrn << ") ---\n";
                        imprimir_ativo(a);
                    } else {
                        std::cout << "\n[Erro] Registro apontado no indice nao esta ativo.\n";
                    }
                } else {
                    std::cout << "\nAtivo nao encontrado via Arvore B.\n";
                }
                break;
            }

            case 3: {
                std::cin.ignore();
                std::cout << "\nDigite o Tipo de Equipamento (ex: Notebook): ";
                char tipo_alvo[20];
                std::cin.getline(tipo_alvo, 20);

                std::vector<int> ids = buscar_indice_secundario(sec_tipo, lst_tipo, tipo_alvo);
                std::cout << "\n--- ATIVOS ENCONTRADOS PARA O TIPO '" << tipo_alvo << "' ---\n";
                int count = 0;
                for (int id : ids) {
                    fseek(fpIndice, 0, SEEK_SET);
                    int rrn_raiz;
                    fread(&rrn_raiz, sizeof(int), 1, fpIndice);
                    
                    int rrn = buscar_na_arvore_b(fpIndice, rrn_raiz, id);
                    if (rrn != -1) {
                        fseek(fpDados, sizeof(int) + rrn * sizeof(Ativo), SEEK_SET);
                        Ativo a;
                        if (fread(&a, sizeof(Ativo), 1, fpDados) == 1 && !esta_removido(a)) {
                            imprimir_ativo(a);
                            count++;
                        }
                    }
                }
                if (count == 0) {
                    std::cout << "(Nenhum ativo localizado para esta pesquisa)\n";
                }
                break;
            }

            case 4: {
                std::cin.ignore();
                std::cout << "\nDigite o Setor de Alocacao (ex: TI): ";
                char setor_alvo[20];
                std::cin.getline(setor_alvo, 20);

                std::vector<int> ids = buscar_indice_secundario(sec_setor, lst_setor, setor_alvo);
                std::cout << "\n--- ATIVOS ENCONTRADOS PARA O SETOR '" << setor_alvo << "' ---\n";
                int count = 0;
                for (int id : ids) {
                    fseek(fpIndice, 0, SEEK_SET);
                    int rrn_raiz;
                    fread(&rrn_raiz, sizeof(int), 1, fpIndice);
                    
                    int rrn = buscar_na_arvore_b(fpIndice, rrn_raiz, id);
                    if (rrn != -1) {
                        fseek(fpDados, sizeof(int) + rrn * sizeof(Ativo), SEEK_SET);
                        Ativo a;
                        if (fread(&a, sizeof(Ativo), 1, fpDados) == 1 && !esta_removido(a)) {
                            imprimir_ativo(a);
                            count++;
                        }
                    }
                }
                if (count == 0) {
                    std::cout << "(Nenhum ativo localizado para esta pesquisa)\n";
                }
                break;
            }

            case 5: {
                int id;
                std::cout << "\nDigite o ID do Ativo a atualizar: ";
                std::cin >> id;
                atualizar_ativo_completo(fpDados, fpIndice, sec_tipo, lst_tipo, sec_setor, lst_setor, id);
                break;
            }

            case 6: {
                int id;
                std::cout << "\nDigite o ID do Ativo a remover: ";
                std::cin >> id;
                remover_ativo_completo(fpDados, fpIndice, sec_tipo, lst_tipo, sec_setor, lst_setor, id);
                break;
            }

            case 7: {
                listar_ativos(fpDados);
                break;
            }

            case 8: {
                depurar_arquivo_dados(arquivo_dados);
                break;
            }

            case 9: {
                depurar_arvore_b(arquivo_indice);
                break;
            }

            case 10: {
                // Para rodar o Vacuum, precisamos fechar os descritores de arquivo abertos
                fclose(fpDados);
                fclose(fpIndice);
                
                vacuum_defragmentar(arquivo_dados, arquivo_indice, sec_tipo, lst_tipo, sec_setor, lst_setor);
                
                // Reabrir os descritores
                fpDados = fopen(arquivo_dados, "rb+");
                fpIndice = fopen(arquivo_indice, "rb+");
                break;
            }

            case 0:
                std::cout << "\nFinalizando o sistema...\n";
                break;

            default:
                std::cout << "\nOpcao invalida. Escolha novamente.\n";
                break;
        }
    }

    fclose(fpDados);
    fclose(fpIndice);
    return 0;
}
