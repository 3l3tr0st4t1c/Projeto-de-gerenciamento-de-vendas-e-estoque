import os
import time
import threading
import requests
from flask import Flask, jsonify, render_template

app = Flask(__name__)

# === CREDENCIAIS iFood ===
CLIENT_ID     = "0ed948ff-e60d-4bee-bd6f-f8bdc9856ab0"
CLIENT_SECRET = "c6ml41du8cormt3v55zqtqc70ybkg6ty6ibnbozpg0qphh7od80dwtr1uqdhwzhq2ugrxhxmj15jc9gs3hvauh3m25t7xa8myl4"

INTERVALO_POLLING    = 7
LIMITE_ATRASO_COZINHA = 1200  # 20 min em segundos
LIMITE_ATRASO_ENTREGA = 2400  # 40 min em segundos

BASE_DIR      = os.path.dirname(os.path.abspath(__file__))
HISTORICO_FILE = os.path.join(BASE_DIR, "historico_v7.txt")

# === ESTADO GLOBAL (equivalente à ListaGestao RAM do C) ===
g_token            = {"access_token": "", "expires_at": 0}
g_pedidos          = {}   # orderId -> dict com os 5 bancos
g_faturamento_live = 0.0
g_ultimo_log       = "V7 Inicializado. Aguardando eventos..."
g_lock             = threading.Lock()


# ------------------------------------------------------------------ helpers --

def registrar_log(msg):
    global g_ultimo_log
    t = time.strftime("[%H:%M:%S]")
    g_ultimo_log = f"{t} {msg}"


def garantir_token():
    agora = time.time()
    if g_token["access_token"] and agora < g_token["expires_at"]:
        return g_token["access_token"]

    try:
        r = requests.post(
            "https://merchant-api.ifood.com.br/authentication/v1.0/oauth/token",
            data={
                "grantType":    "client_credentials",
                "clientId":     CLIENT_ID,
                "clientSecret": CLIENT_SECRET,
            },
            timeout=10,
        )
        data = r.json()
        g_token["access_token"] = data.get("accessToken", "")
        g_token["expires_at"]   = agora + 3500
        registrar_log("Token API Renovado com Sucesso. Conexao Segura.")
    except Exception as e:
        registrar_log(f"ERRO CRITICO: Falha na autenticacao — {e}")

    return g_token["access_token"]


def buscar_detalhes_pedido(order_id, token):
    try:
        r = requests.get(
            f"https://merchant-api.ifood.com.br/order/v1.0/orders/{order_id}",
            headers={"Authorization": f"Bearer {token}"},
            timeout=10,
        )
        det = r.json()
    except Exception as e:
        registrar_log(f"ERRO ao buscar detalhes do pedido: {e}")
        return None

    pedido = {
        "orderId":        order_id,
        "displayId":      det.get("displayId", "0000"),
        "status":         "PREPARO",
        "momentoEntrada": time.time(),
        "momentoDespacho": 0,
        "cliente": {
            "nome":      "CLIENTE NAO IDENTIFICADO",
            "telefone":  "SEM_NUMERO",
            "idCliente": "ID_NULL",
        },
        "financeiro": {
            "valorBruto":        0.0,
            "taxaEntrega":       0.0,
            "metodoPagamento":   "DESCONHECIDO",
        },
        "entrega": {
            "bairro":     "RETIRADA/BALCAO",
            "logradouro": "",
            "numero":     "S/N",
        },
        "dna": {
            "quantidadeTotalItens": 0,
            "resumoItens":          "",
        },
    }

    # Banco Cliente
    customer = det.get("customer") or {}
    pedido["cliente"]["nome"]      = customer.get("name", "CLIENTE NAO IDENTIFICADO")
    pedido["cliente"]["idCliente"] = customer.get("id",   "ID_NULL")
    phone = customer.get("phone") or {}
    pedido["cliente"]["telefone"]  = phone.get("number", "SEM_NUMERO")

    # Banco Financeiro
    total = det.get("total") or {}
    pedido["financeiro"]["valorBruto"]  = total.get("orderAmount", 0.0)
    pedido["financeiro"]["taxaEntrega"] = total.get("deliveryFee",  0.0)
    payments = det.get("payments") or {}
    methods  = payments.get("methods") or []
    if methods:
        pedido["financeiro"]["metodoPagamento"] = methods[0].get("method", "DESCONHECIDO")

    # Banco Entrega
    delivery = det.get("delivery") or {}
    addr     = delivery.get("deliveryAddress") or {}
    if addr:
        pedido["entrega"]["bairro"]     = addr.get("neighborhood", "RETIRADA/BALCAO")
        pedido["entrega"]["logradouro"] = addr.get("streetName",   "")
        pedido["entrega"]["numero"]     = addr.get("streetNumber",  "S/N")

    # Banco DNA (itens)
    items   = det.get("items") or []
    resumo  = ""
    qtd_tot = 0
    for item in items:
        qtd  = item.get("quantity", 0)
        nome = item.get("name", "")
        qtd_tot += qtd
        parte = f"{qtd}x {nome}, "
        if len(resumo) + len(parte) < 290:
            resumo += parte
        elif "..." not in resumo:
            resumo += "..."

    pedido["dna"]["quantidadeTotalItens"] = qtd_tot
    pedido["dna"]["resumoItens"]          = resumo.rstrip(", ")

    return pedido


def salvar_ssd(pedido, resultado):
    try:
        with open(HISTORICO_FILE, "a", encoding="utf-8") as f:
            agora = time.strftime("%a %b %d %H:%M:%S %Y")
            f.write(
                f"{pedido['displayId']}||{pedido['orderId']}||{resultado}||{agora}||"
                f"{pedido['cliente']['nome']}||{pedido['financeiro']['valorBruto']:.2f}||"
                f"{pedido['financeiro']['taxaEntrega']:.2f}||{pedido['financeiro']['metodoPagamento']}||"
                f"{pedido['entrega']['bairro']}||{pedido['entrega']['logradouro']}||{pedido['entrega']['numero']}||"
                f"{pedido['dna']['quantidadeTotalItens']}||{pedido['dna']['resumoItens']}\n"
            )
    except Exception as e:
        registrar_log(f"ERRO ao salvar no SSD: {e}")


def processar_evento(order_id, full_code):
    global g_faturamento_live

    with g_lock:
        existente = g_pedidos.get(order_id)

        if full_code == "PLACED" and not existente:
            token = g_token["access_token"]
            novo  = buscar_detalhes_pedido(order_id, token)
            if novo:
                g_pedidos[order_id]  = novo
                g_faturamento_live  += novo["financeiro"]["valorBruto"]
                registrar_log(
                    f"NOVO PEDIDO CAPTURADO! Display #{novo['displayId']} | "
                    f"Valor: R${novo['financeiro']['valorBruto']:.2f}"
                )

        elif existente:
            if full_code in ("CONCLUDED", "CANCELLED"):
                salvar_ssd(existente, full_code)
                g_faturamento_live -= existente["financeiro"]["valorBruto"]
                del g_pedidos[order_id]
                registrar_log(
                    f"CICLO DE VIDA ENCERRADO: Pedido #{existente['displayId']} ({full_code}). "
                    f"Dados Salvos no SSD."
                )
            elif full_code == "DISPATCHED":
                existente["status"]          = "EM ROTA"
                existente["momentoDespacho"] = time.time()
                registrar_log("MOTOBOY LIBERADO: Pedido saiu para entrega.")
            elif full_code == "CONFIRMED":
                existente["status"] = "ACEITO"
                registrar_log("Confirmacao automatica via API registrada.")


def enviar_ack(eventos, token):
    try:
        payload = [{"id": ev["id"]} for ev in eventos if "id" in ev]
        if payload:
            requests.post(
                "https://merchant-api.ifood.com.br/order/v1.0/events/acknowledgment",
                headers={
                    "Authorization":  f"Bearer {token}",
                    "Content-Type":   "application/json",
                },
                json=payload,
                timeout=10,
            )
    except Exception as e:
        registrar_log(f"ERRO no ACK: {e}")


# --------------------------------------------------------- polling thread ---

def loop_polling():
    while True:
        try:
            token = garantir_token()
            r = requests.get(
                "https://merchant-api.ifood.com.br/order/v1.0/events:polling",
                headers={"Authorization": f"Bearer {token}"},
                timeout=10,
            )
            eventos = r.json()
            if isinstance(eventos, list) and eventos:
                for ev in eventos:
                    oid  = ev.get("orderId")
                    code = ev.get("fullCode")
                    if oid and code:
                        processar_evento(oid, code)
                enviar_ack(eventos, token)
        except Exception as e:
            registrar_log(f"ERRO no polling: {e}")

        time.sleep(INTERVALO_POLLING)


# -------------------------------------------------------------- rotas Flask --

@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/live")
def api_live():
    agora = time.time()
    with g_lock:
        pedidos = list(g_pedidos.values())

    cozinha   = [p for p in pedidos if p["status"] != "EM ROTA"]
    logistica = [p for p in pedidos if p["status"] == "EM ROTA"]
    total     = len(pedidos)
    ticket    = g_faturamento_live / total if total > 0 else 0.0

    def fmt(p, ref_time):
        segs = int(agora - ref_time) if ref_time else 0
        return {
            "displayId":        p["displayId"],
            "status":           p["status"],
            "timerStr":         f"{segs // 60:02d}m {segs % 60:02d}s",
            "atrasado":         segs > (LIMITE_ATRASO_ENTREGA if p["status"] == "EM ROTA"
                                        else LIMITE_ATRASO_COZINHA),
            "cliente":          p["cliente"]["nome"],
            "resumoItens":      p["dna"]["resumoItens"],
            "bairro":           p["entrega"]["bairro"],
            "valor":            p["financeiro"]["valorBruto"],
        }

    return jsonify({
        "metricas": {
            "totalNaTela":  total,
            "grossRevenue": round(g_faturamento_live, 2),
            "avgTicket":    round(ticket, 2),
        },
        "qtdCozinha":   len(cozinha),
        "qtdLogistica": len(logistica),
        "cozinha":   [fmt(p, p["momentoEntrada"])  for p in cozinha],
        "logistica": [fmt(p, p["momentoDespacho"]) for p in logistica],
        "log":       g_ultimo_log,
    })


@app.route("/api/historico")
def api_historico():
    if not os.path.exists(HISTORICO_FILE):
        return jsonify({"analytics": None, "registros": []})

    registros  = []
    total = concluidos = cancelados = 0
    faturamento = total_taxas = 0.0
    total_itens = 0

    with open(HISTORICO_FILE, "r", encoding="utf-8") as f:
        for linha in f:
            linha = linha.strip()
            if not linha:
                continue
            parts = linha.split("||")
            if len(parts) < 12:
                continue

            total += 1
            status = parts[2]

            if status == "CONCLUDED":
                concluidos += 1
                try:
                    faturamento += float(parts[5])
                    total_taxas += float(parts[6])
                    total_itens += int(parts[11])
                except ValueError:
                    pass
            elif status == "CANCELLED":
                cancelados += 1

            registros.append({
                "displayId":        parts[0],
                "orderId":          parts[1],
                "status":           parts[2],
                "dataHora":         parts[3],
                "cliente":          parts[4],
                "valorBruto":       parts[5],
                "taxaEntrega":      parts[6],
                "metodoPagamento":  parts[7],
                "bairro":           parts[8],
                "logradouro":       parts[9],
                "numero":           parts[10],
                "qtdItens":         parts[11],
                "resumoItens":      parts[12] if len(parts) > 12 else "",
            })

    ticket_medio = faturamento / concluidos if concluidos > 0 else 0.0

    return jsonify({
        "analytics": {
            "total":       total,
            "concluidos":  concluidos,
            "cancelados":  cancelados,
            "faturamento": round(faturamento, 2),
            "ticketMedio": round(ticket_medio, 2),
            "totalTaxas":  round(total_taxas, 2),
            "totalItens":  total_itens,
        },
        "registros": registros,
    })


# --------------------------------------------------------------- iniciar ---

if __name__ == "__main__":
    t = threading.Thread(target=loop_polling, daemon=True)
    t.start()
    print(" * Motor V7 engatado — polling iFood iniciado")
    print(" * Acesse: http://localhost:5000")
    app.run(debug=False, port=5000)
