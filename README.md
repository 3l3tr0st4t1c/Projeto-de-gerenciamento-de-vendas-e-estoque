# Projeto-de-gerenciamento-de-vendas-e-estoque
Aqui jaz as pastas de arquivos do sistema supracitado, focando na estruturação de dados em linguagem C.

Para facilitar a navegação, atente-se que:

src/ — É a pasta onde os arquivos .c estão.

include/ — Cabeçalhos para chamadas, APIs e arquivos de configurações em geral.

data/ — Para arquivos de teste e banco de dados.

---

## Novos arquivos (branch interface — Dashboard Web via C)

servidor.py — Servidor Flask mínimo. Serve o dashboard e os arquivos gerados pelo programa C. Não contém lógica de negócio.

dashboard.html — Página do dashboard com 3 abas: Motor Live, Big Data SSD e Gráficos.

dashboard.css — Estilos visuais do dashboard (dark theme).

dashboard.js — Lógica do frontend: lê os dados gerados pelo C e atualiza a tela a cada 7 segundos.

estado_atual.json — Gerado automaticamente pelo programa C a cada ciclo de polling. Contém o estado atual dos pedidos em RAM.

historico_v7.txt — Banco de dados em disco. Gerado pelo programa C ao finalizar pedidos.

---

## Alterações no programa C (compatibilidade cross-platform)

O arquivo `src/Sistema_delivery.c` foi atualizado para rodar em **Windows e macOS**:

- `#include <windows.h>` substituído por guarda `#ifdef _WIN32` com `#include <unistd.h>` no macOS
- `Sleep()` substituído por `usleep()` no macOS
- `system("cls")` substituído por `system("clear")` no macOS
- `system("pause")` substituído por `getchar()` no macOS
- `SetConsoleOutputCP(65001)` isolado em `#ifdef _WIN32`
- Nova função `exportar_estado_json()`: grava o estado atual da RAM em `estado_atual.json` a cada ciclo de polling, permitindo que o dashboard web exiba os dados em tempo real

---

## Como rodar — Interface Terminal (C)

### Dependências

- `libcurl`
- `libcjson`

No macOS (Homebrew):

```bash
brew install curl cjson
```

### Compilar (macOS)

```bash
gcc src/Sistema_delivery.c -o vitor_delivery -lcurl \
  -I/usr/local/Cellar/cjson/1.7.19/include \
  -I/usr/local/Cellar/cjson/1.7.19/include/cjson \
  -L/usr/local/Cellar/cjson/1.7.19/lib -lcjson
```

### Compilar (Windows)

```bash
gcc src/Sistema_delivery.c -o vitor_delivery.exe -lcurl -lcjson
```

### Rodar

```bash
./vitor_delivery
```

**Menu:**
- `[1]` Iniciar Motor Live — painel tático ao vivo no terminal e gera `estado_atual.json`
- `[2]` Consultar Big Data SSD — histórico de pedidos finalizados
- `[3]` Encerrar sistema

---

## Como rodar — Dashboard Web

O dashboard web exibe os dados gerados pelo programa C. Toda a lógica permanece em C.

### Dependências

- Python 3.8+
- Flask

```bash
pip install flask
```

### Rodar

**Terminal 1 — programa C (Motor Live):**

```bash
./vitor_delivery
# Selecione a opção 1
```

**Terminal 2 — servidor web:**

```bash
python servidor.py
```

Acesse no browser: [http://localhost:5000](http://localhost:5000)

**Abas:**
- `[1] Motor Live` — pedidos ativos em tempo real (dados do `estado_atual.json` gerado pelo C)
- `[2] Big Data SSD` — analytics e histórico completo (dados do `historico_v7.txt`)
- `[3] Gráficos` — visualizações analíticas com Chart.js

> O programa C continua sendo o núcleo do sistema. O servidor web apenas serve os arquivos ao browser.
