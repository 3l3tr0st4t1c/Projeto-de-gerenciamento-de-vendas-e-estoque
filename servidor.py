"""
servidor.py — Servidor web mínimo para o dashboard do SGA v7.0.0
Toda a lógica de negócio está no programa C (Sistema_delivery.c).
Este servidor apenas serve os arquivos estáticos e o estado_atual.json
gerado pelo C a cada ciclo de polling.

Como usar:
  1. Compile e rode o programa C:  ./vitor_delivery  (opção 1)
  2. Em outro terminal, rode:       python servidor.py
  3. Abra no browser:               http://localhost:5000
"""

import os
import json
from flask import Flask, send_from_directory, jsonify

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

app = Flask(__name__, static_folder=BASE_DIR)


@app.route("/")
def index():
    return send_from_directory(BASE_DIR, "dashboard.html")


@app.route("/dashboard.css")
def css():
    return send_from_directory(BASE_DIR, "dashboard.css")


@app.route("/dashboard.js")
def js():
    return send_from_directory(BASE_DIR, "dashboard.js")


@app.route("/estado")
def estado():
    """Lê e serve o estado_atual.json gerado pelo programa C."""
    caminho = os.path.join(BASE_DIR, "estado_atual.json")
    try:
        with open(caminho, "r", encoding="utf-8") as f:
            return jsonify(json.load(f))
    except FileNotFoundError:
        return jsonify({
            "metricas": {"totalNaTela": 0, "grossRevenue": 0.0, "avgTicket": 0.0},
            "qtdCozinha": 0,
            "qtdLogistica": 0,
            "cozinha": [],
            "logistica": [],
            "log": "Aguardando o programa C iniciar o Motor Live..."
        })


@app.route("/historico")
def historico():
    """Lê e serve o historico_v7.txt gerado pelo programa C."""
    caminho = os.path.join(BASE_DIR, "historico_v7.txt")
    registros = []
    analytics = {"total": 0, "concluidos": 0, "cancelados": 0,
                 "faturamento": 0.0, "ticketMedio": 0.0,
                 "totalTaxas": 0.0, "totalItens": 0}

    try:
        with open(caminho, "r", encoding="utf-8") as f:
            for linha in f:
                linha = linha.strip()
                if not linha:
                    continue
                parts = linha.split("||")
                if len(parts) < 12:
                    continue

                analytics["total"] += 1
                status = parts[2]

                if status == "CONCLUDED":
                    analytics["concluidos"] += 1
                    try:
                        analytics["faturamento"] += float(parts[5])
                        analytics["totalTaxas"]  += float(parts[6])
                        analytics["totalItens"]  += int(parts[11])
                    except ValueError:
                        pass
                elif status == "CANCELLED":
                    analytics["cancelados"] += 1

                registros.append({
                    "displayId":       parts[0],
                    "orderId":         parts[1],
                    "status":          parts[2],
                    "dataHora":        parts[3],
                    "cliente":         parts[4],
                    "valorBruto":      parts[5],
                    "taxaEntrega":     parts[6],
                    "metodoPagamento": parts[7],
                    "bairro":          parts[8],
                    "logradouro":      parts[9],
                    "numero":          parts[10],
                    "qtdItens":        parts[11],
                    "resumoItens":     parts[12] if len(parts) > 12 else "",
                })

        if analytics["concluidos"] > 0:
            analytics["ticketMedio"] = round(
                analytics["faturamento"] / analytics["concluidos"], 2)
        analytics["faturamento"] = round(analytics["faturamento"], 2)
        analytics["totalTaxas"]  = round(analytics["totalTaxas"], 2)

    except FileNotFoundError:
        pass

    return jsonify({"analytics": analytics, "registros": registros})


if __name__ == "__main__":
    print(" * Dashboard SGA v7 — servidor iniciado")
    print(" * Acesse: http://localhost:5000")
    print(" * Certifique-se de que o programa C está rodando (opção 1)")
    app.run(debug=False, port=5000)
