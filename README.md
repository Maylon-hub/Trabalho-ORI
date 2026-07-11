# Sistema de Rastreamento de Ativos e Inventário de TI

Este repositório contém a implementação completa da camada de persistência e indexação de dados para um **Sistema de Rastreamento de Ativos e Inventário de TI**. O projeto foi desenvolvido "do zero" em C++, em conformidade com as restrições acadêmicas da disciplina de **Organização e Recuperação de Informação (ORI)**, sem a utilização de banco de dados ou serializadores prontos.

Toda a manipulação dos dados é feita diretamente no disco de forma nativa por meio de arquivos binários estruturados, utilizando operações de baixo nível como `fseek`, `ftell`, `fread` e `fwrite`.

---

## 🛠️ Arquitetura e Estruturas de Dados

O motor de banco de dados foi construído com base em registros de tamanho fixo em disco e índices persistidos para otimização de busca rápida.

### 1. Arquivo de Dados Principal (`ativos_inventario.bin`)
- **Tamanho Fixo do Registro**: Cada registro da struct `Ativo` possui exatamente **88 bytes** em disco. O empacotamento é forçado com a diretiva `#pragma pack(push, 1)` para evitar o preenchimento de bytes extras pelo compilador (*padding*).
- **Layout de Campos**:
  - `int patrimonio_id` (4 bytes) — Chave Primária.
  - `char tipo_equipamento[20]` (20 bytes).
  - `char setor_alocacao[20]` (20 bytes).
  - `char marca_modelo[40]` (40 bytes).
  - `float valor_compra` (4 bytes).

### 2. Gerenciamento de Exclusão por LED (LIFO)
Para evitar a fragmentação física e o custo de reorganização de arquivos, a deleção de registros é estritamente **lógica**. 
- Os espaços vazios (buracos) são catalogados em uma **Lista de Espaços Disponíveis (LED)** operando como uma **Pilha LIFO** (Last-In, First-Out).
- O cabeçalho do arquivo de dados (primeiros 4 bytes) armazena um inteiro contendo o RRN do topo da LED.
- **Reaproveitamento de Espaço ($O(1)$)**: Quando um registro é removido, seu campo `patrimonio_id` passa a atuar como o ponteiro lógico para o próximo RRN da pilha LED. Para evitar colisões com IDs válidos (positivos), codificamos os ponteiros na LED como valores negativos:
  $$\text{patrimonio\_id} = -(\text{proximo\_rrn} + 2)$$
- Nas novas inserções, o sistema verifica a LED. Se houver espaços vazios, o topo da pilha é desempilhado e o novo registro sobrescreve o RRN recuperado, atualizando o cabeçalho. Caso a LED esteja vazia, a gravação é feita por append no fim do arquivo físico.

### 3. Indexação Primária com Árvore B em Disco
Para eliminar a busca sequencial e possibilitar buscas com complexidade logarítmica ($O(\log N)$), implementamos um índice primário persistido no arquivo `ativos_index.btree`.
- **Nós de Tamanho Fixo**: Cada nó da Árvore B (`NoArvoreB`) possui exatamente **57 bytes** estruturados em disco. O RRN da raiz da árvore fica armazenado nos primeiros 4 bytes do arquivo.
- **Ordem $m = 5$**: Cada nó pode conter no máximo $4$ chaves e $5$ ponteiros para filhos.
- **Cisão (Split) no Disco**: O algoritmo detecta quando o nó ultrapassa o limite de chaves, dividindo-o em dois nós (esquerda e direita) e promovendo a chave mediana. O processo é gravado no final do arquivo de índice e o cabeçalho é atualizado caso a raiz sofra divisão.
- **Busca Otimizada**: O índice é navegado por leitura sob demanda na memória RAM nó por nó de forma iterativa.

### 4. Indexação Secundária com Lista Invertida (Loosely Binding)
Para buscas não-exclusivas (ex: busca por marca ou setor), o sistema implementa uma estrutura de **Lista Invertida Persistida** operando sob a estratégia **Loosely Binding** (Ligação Tardia).
- O índice secundário guarda a chave de busca (ex: marca) e aponta para o ID de patrimônio (Chave Primária) correspondente na Lista Invertida.
- Na hora de efetuar a consulta, o ID de patrimônio é obtido e a Árvore B é consultada para localizar o RRN físico exato do registro ativo. Essa estratégia evita que alterações físicas (mudanças de RRN devido a deleções ou reinserções) quebrem os índices secundários.

---

## 📂 Estrutura do Repositório

```bash
├── arvore_b.hpp           # Cabeçalho da Árvore B (Structs, Busca, Inserção e Splits)
├── main.cpp               # Motor principal contendo CRUD de Ativos e Gerenciador da LED
├── teste_btree.cpp        # Testes unitários de inserções sequenciais e buscas na Árvore B
└── README.md              # Documentação oficial do projeto
```

---

## ⚙️ Compilação e Execução

As instruções a seguir assumem o uso do compilador padrão GCC (`g++`) com suporte a C++17.

### Compilando o CRUD e Motor de Dados Principal
```powershell
g++ -O3 -std=c++17 main.cpp -o main.exe
```

### Compilando o Módulo de Índice da Árvore B
```powershell
g++ -O3 -std=c++17 teste_btree.cpp -o teste_btree.exe
```

### Executando os Sistemas
Após a compilação bem-sucedida, você pode executar os binários gerados para simular os fluxos de persistência e indexação:

```powershell
# Executa os testes do arquivo de dados, LED (LIFO) e reaproveitamento de espaço
.\main.exe

# Executa as operações de inserção, split de nós e busca indexada na Árvore B
.\teste_btree.exe
```

---

## 👨‍🎓 Critérios de Avaliação e Demonstrações Pedagógicas

O projeto foi planejado de forma a facilitar as explicações solicitadas nas avaliações teóricas do **Fator de Domínio**:
- **Cálculo de Offsets**:
  - Arquivo de Dados: $\text{Offset} = 4 + (\text{RRN} \times 88 \text{ bytes})$
  - Arquivo de Índices: $\text{Offset} = 4 + (\text{RRN} \times 57 \text{ bytes})$
- **Demonstração do Split no Disco**: A saída em tempo de execução no console exibe didaticamente como as partições folha e interna dividem e como a chave mediana é promovida de nível.
- **Rastreabilidade da LED**: O depurador exibe em tempo real o encadeamento dos blocos lógicos deletados através de representações de offsets binários.
