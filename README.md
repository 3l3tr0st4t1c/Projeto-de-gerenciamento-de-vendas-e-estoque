# Projeto-de-gerenciamento-de-vendas-e-estoque
Aqui jaz as pastas de arquivos do sistema supracitado, focando na estruturação de dados em linguagem C.

Para facilitar a navegação, atente-se que:

src/ — É a pasta onde os arquivos .c estão.

include/ — Cabeçalhos para chamadas, APIs e arquivos de configurações em geral.

data/ — Para arquivos de teste e banco de dados.

---

## Novas pastas (Dashboard Web)

templates/ — Template HTML do dashboard web.

static/ — CSS e JS do dashboard web.

app.py — Servidor Flask (dashboard web).

historico_v7.txt — Banco de dados SSD, gerado automaticamente ao finalizar pedidos.

---

## Como rodar — Interface Terminal (C)

### Dependências

- `libcurl`
- `libcjson`

No macOS (Homebrew):

```bash
brew install curl cjson
```

### Compilar

```bash
gcc src/Sistema_delivery.c -o vitor_delivery -lcurl -lcjson
```

### Rodar

```bash
./vitor_delivery
```

**Menu:**
- `[1]` Iniciar Motor Live — painel tático ao vivo no terminal
- `[2]` Consultar Big Data SSD — histórico de pedidos finalizados
- `[3]` Encerrar sistema

---

## Como rodar — Dashboard Web (Flask)

### Dependências

- Python 3.8+
- Flask
- Requests

```bash
pip install flask requests
```

### Rodar

```bash
python app.py
```

Acesse no browser: [http://localhost:5000](http://localhost:5000)

**Abas:**
- `[1] Motor Live` — métricas em tempo real, Estação de Produção (Cozinha) e Estação de Logística, atualiza a cada 7 segundos
- `[2] Big Data SSD` — analytics macro e histórico completo de pedidos finalizados

> O dashboard realiza o mesmo polling da API iFood em background. O arquivo `historico_v7.txt` é compartilhado entre as duas interfaces.
