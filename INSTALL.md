# Guia de Instalação — Vitor Delivery V7.0

> Branch: `interface` | Versão: `7.0.0-ULTIMATE`

O projeto possui **duas partes independentes**: o programa em C (núcleo do sistema) e o servidor web em Flask (dashboard). Ambos precisam estar rodando simultaneamente para usar o dashboard.

---

## Dependências

| Dependência | Função | Windows | macOS | Linux |
|---|---|---|---|---|
| `GCC` | Compilador C | MSYS2 UCRT64 | Xcode CLI / brew | `apt install gcc` |
| `libcurl` | HTTP para API iFood | MSYS2 UCRT64 | `brew install curl` | `apt install libcurl4-openssl-dev` |
| `libcjson` | Parse de JSON em C | MSYS2 UCRT64 | `brew install cjson` | `apt install libcjson-dev` |
| `Python 3.8+` | Servidor web | msys2 ou python.org | nativo | nativo |
| `Flask` | Serve o dashboard | `pip install flask` | `pip3 install flask` | `pip3 install flask` |
| `Chart.js` | Gráficos no browser | CDN (automático) | CDN (automático) | CDN (automático) |

---

## Windows (MSYS2 + UCRT64)

O projeto já está configurado para MSYS2 com toolchain UCRT64 (`tasks.json` e `c_cpp_properties.json`). Esta é a configuração recomendada para contribuintes Windows.

### 1. Instalar o MSYS2

Baixe em [msys2.org](https://www.msys2.org) e instale. Abra o terminal **MSYS2 UCRT64** e execute:

```bash
pacman -Syu
```

### 2. Instalar GCC e bibliotecas C

```bash
# Compilador GCC
pacman -S mingw-w64-ucrt-x86_64-gcc

# libcurl (requisições HTTP para a API)
pacman -S mingw-w64-ucrt-x86_64-curl

# libcjson (parse de JSON em C)
pacman -S mingw-w64-ucrt-x86_64-cjson
```

### 3. Instalar Python e Flask

```bash
# Python (via MSYS2)
pacman -S mingw-w64-ucrt-x86_64-python

# Flask via requirements.txt (recomendado)
pip install -r requirements.txt
```

### 4. Compilar e rodar

```bash
# Compilar
gcc src/Sistema_delivery.c -o vitor_delivery.exe -lcurl -lcjson

# Terminal 1 — programa C
./vitor_delivery.exe

# Terminal 2 — servidor web
python servidor.py
```

---

## macOS

### 1. Instalar Homebrew (se não tiver)

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### 2. Instalar dependências C

```bash
brew install curl cjson
```

### 3. Instalar Flask

```bash
pip3 install -r requirements.txt
```

### 4. Compilar e rodar

> ⚠️ O path do cJSON pode variar. Confirme com `brew info cjson` antes de compilar.

```bash
# Compilar (ajuste a versão do cjson se necessário)
gcc src/Sistema_delivery.c -o vitor_delivery -lcurl \
  -I/usr/local/Cellar/cjson/1.7.19/include \
  -I/usr/local/Cellar/cjson/1.7.19/include/cjson \
  -L/usr/local/Cellar/cjson/1.7.19/lib -lcjson

# Terminal 1 — programa C
./vitor_delivery

# Terminal 2 — servidor web
python3 servidor.py
```

---

## Linux (Ubuntu / Debian)

### 1. Instalar dependências C

```bash
sudo apt update
sudo apt install gcc libcurl4-openssl-dev libcjson-dev
```

### 2. Instalar Flask

```bash
pip3 install -r requirements.txt
```

### 3. Compilar e rodar

```bash
# Compilar
gcc src/Sistema_delivery.c -o vitor_delivery -lcurl -lcjson

# Terminal 1 — programa C
./vitor_delivery

# Terminal 2 — servidor web
python3 servidor.py
```

---

## Dashboard Web

Com os dois processos rodando, acesse no browser:

```
http://localhost:5000
```

**Abas disponíveis:**
- `[1] Motor Live` — pedidos ativos em tempo real (atualiza a cada 7 segundos)
- `[2] Big Data SSD` — histórico completo de pedidos com analytics
- `[3] Gráficos` — visualizações com Chart.js (requer internet para carregar via CDN)

---

## Notas

> ⚠️ As credenciais `CLIENT_ID` e `CLIENT_SECRET` estão hardcoded em `src/Sistema_delivery.c`. Não as exponha publicamente. Uma melhoria planejada é migrá-las para variáveis de ambiente.

**Ordem de execução recomendada:**
1. Compile o programa C conforme seu sistema operacional
2. Rode o executável e selecione a opção `[1] Motor Live`
3. Em outro terminal, rode `python servidor.py` (ou `python3`)
4. Acesse `http://localhost:5000` no browser
