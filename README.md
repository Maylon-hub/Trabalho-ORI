# Sistema de Rastreamento de Ativos e Inventário de TI

Este repositório contém a implementação completa da persistência física e indexação para o **Sistema de Rastreamento de Ativos e Inventário de TI**. O projeto foi desenvolvido totalmente em C++ para a disciplina de **Organização e Recuperação de Informação (ORI)**, atendendo à restrição de não utilizar bancos de dados prontos (como SQLite ou MySQL) nem bibliotecas de alto nível para serialização. 

Todos os dados e índices são manipulados por meio de ponteiros de disco nativos (`fseek`, `ftell`, `fread`, `fwrite`).

---

## 🛠️ Arquitetura e Estruturas de Dados

O sistema é composto por 4 arquivos principais e utiliza uma arquitetura baseada em registros de tamanho fixo com indexação primária (Árvore B) e secundária (Listas Invertidas com *Loosely Binding*).

### 1. Arquivo de Dados Principal (`ativos_inventario.bin`)
- **Tamanho Fixo**: Cada registro da struct `Ativo` possui exatamente **88 bytes** em disco, forçado com a diretiva `#pragma pack(push, 1)`.
- **Campos**:
  - `int patrimonio_id` (4 bytes) — Chave Primária (imutável após a inserção).
  - `char tipo_equipamento[20]` (20 bytes).
  - `char setor_alocacao[20]` (20 bytes).
  - `char marca_modelo[40]` (40 bytes).
  - `float valor_compra` (4 bytes).

### 2. Gerenciamento de Espaço Livre (LED - Pilha LIFO)
- A exclusão de ativos é **estritamente lógica**. O cabeçalho do arquivo de dados (primeiros 4 bytes) armazena um `int` correspondente ao RRN do topo da LED.
- **Mapeamento de Remoção**: Quando um ativo é excluído, seu ID é codificado de forma negativa para marcar a exclusão e apontar para o próximo RRN livre na pilha:
  (patrimonio_id = proximo_rrn + 2)
- Nas inserções, a LED é verificada: se houver buracos disponíveis, o espaço é reaproveitado na ordem LIFO (O(1)). Caso contrário, grava-se no final do arquivo físico.

### 3. Índice Primário por Árvore B (`ativos_index.btree`)
- Permite busca direta em complexidade logarítmica (O(log N)).
- **Nós de Tamanho Fixo**: Cada nó da Árvore B (`NoArvoreB`) possui exatamente **57 bytes** estruturados em disco, com ordem m = 5 (máximo de 4 chaves por nó).
- **Compatibilidade Binária**: O layout em disco foi alinhado e ordenado de forma equivalente ao formato em C tradicional (`eh_folha` no offset 0 e `num_chaves` no offset 1), garantindo portabilidade.
- **Remoção Lógica**: Quando um ativo é excluído, o RRN de dados correspondente é marcado como `-1` no nó da Árvore B correspondente.

### 4. Índices Secundários por Lista Invertida (Loosely Binding)
- O sistema indexa **dois campos não exclusivos**: **Tipo** (`tipo_equipamento`) e **Setor** (`setor_alocacao`).
- **Arquivos**:
  - Setor: `setor_secundario.idx` e `setor_lista.bin`
  - Tipo: `tipo_secundario.idx` e `tipo_lista.bin`
- **Loosely Binding**: Os índices secundários associam os termos pesquisados (como `"TI"` ou `"Notebook"`) às suas chaves primárias correspondentes (`patrimonio_id`). Ao fazer a busca secundária, o ID é retornado e consultado na Árvore B para obter o RRN de dados real. Isso garante que desfragmentações físicas do disco não quebrem os índices secundários.

---

## 📂 Estrutura do Repositório

O projeto foi unificado em C++ e consolidado em apenas **4 arquivos de código** para máxima clareza e facilidade de entendimento durante a explicação:

```bash
├── ativos.h               # Estrutura do registro Ativo (88 bytes) e ajudantes de I/O em disco
├── arvore_b.hpp           # Implementação da Árvore B em disco (busca, inserção, split e deleção lógica)
├── lista_invertida.hpp    # Implementação de Listas Invertidas parametrizadas para múltiplos campos
├── main.cpp               # Menu interativo CLI, LED, fluxo CRUD integrado e Vacuum
└── README.md              # Esta documentação do projeto
```

---

## ⚙️ Compilação e Execução

### Como compilar:
Compile usando `g++` com otimização `-O3` e suporte ao padrão C++17:
```powershell
g++ -O3 -std=c++17 main.cpp -o SGBD.exe
```

### Como executar:
```powershell
.\SGBD.exe
```

---

## 💎 Funcionalidades de Destaque

### 🔄 CRUD Completo
- **Inserir**: Checa se a chave é única e a insere no arquivo de dados, B-Tree e Índices Secundários.
- **Buscar**: Busca extremamente rápida por ID (via Árvore B) ou por Tipo/Setor (via Listas Invertidas).
- **Atualizar**: Altera os campos do ativo buscando-o pelo ID (imutável) e atualiza automaticamente os índices secundários se houver alteração de Tipo ou Setor.
- **Remover**: Marca o registro como deletado em disco, insere o RRN na LED e invalida as referências na Árvore B e nas Listas Invertidas.

### 🌪️ Desafio do Chefão: Vacuum (Desfragmentador + Reconstrução)
O sistema possui a opção administrativa **Vacuum (Opção 9)** que executa a manutenção do banco de dados físico:
1. Copia fisicamente apenas os registros válidos para um arquivo temporário de dados.
2. Limpa completamente a LED (redefinindo o topo da pilha como `-1`).
3. Reconstrói a Árvore B do zero com os novos RRNs compactados.
4. Reconstrói as listas invertidas de Tipo e Setor apenas para registros ativos.
5. Renomeia os arquivos para substituir a base fragmentada.

---

## 👨‍🎓 Demonstração para a Apresentação Oral
Para explicar as lógicas solicitadas pelo professor na banca:
- **Cálculo de Offsets**:
  - Dados: Offset = 4 + RRN * 88 bytes
  - Árvore B: $\text{Offset} = 4 + RRN * 57 bytes
- **LED Visual (Opção 7)**: Mostra como a pilha de registros excluídos armazena ponteiros negativos em disco.
- **Árvore B Hierárquica (Opção 8)**: Imprime a estrutura de níveis em árvore com RRNs dos nós e chaves promovidas.
