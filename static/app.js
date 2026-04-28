/* ============================================================
   VITOR DELIVERY — SGA v7.0.0 — Frontend JS
   ============================================================ */

const INTERVALO = 7; // segundos
let abaAtual = 'live';
let countdown = INTERVALO;
let charts = {};

// Paleta de cores para os gráficos (combina com o CSS dark theme)
const CORES = [
  '#00e5ff', '#2ecc40', '#ffd700', '#ff44ff',
  '#1a8cff', '#ff3b30', '#ff9500', '#5ac8fa',
  '#30d158', '#bf5af2',
];

const CHART_DEFAULTS = {
  color: '#e8edf5',
  borderColor: '#2a3a5a',
  backgroundColor: 'transparent',
};

Chart.defaults.color           = CHART_DEFAULTS.color;
Chart.defaults.borderColor     = CHART_DEFAULTS.borderColor;
Chart.defaults.backgroundColor = CHART_DEFAULTS.backgroundColor;

// --------------------------------------------------------- relógio ---------

function atualizarRelogio() {
  const agora = new Date();
  const h = String(agora.getHours()).padStart(2, '0');
  const m = String(agora.getMinutes()).padStart(2, '0');
  const s = String(agora.getSeconds()).padStart(2, '0');
  document.getElementById('header-clock').textContent = `${h}:${m}:${s}`;
}
setInterval(atualizarRelogio, 1000);
atualizarRelogio();

// --------------------------------------------------------- countdown -------

function tickCountdown() {
  countdown--;
  if (countdown <= 0) countdown = INTERVALO;
  document.getElementById('refresh-countdown').textContent =
    `próx. atualização em ${countdown}s`;
}
setInterval(tickCountdown, 1000);

// --------------------------------------------------------- navegação -------

function mudarAba(aba) {
  abaAtual = aba;
  countdown = INTERVALO;

  ['live', 'ssd', 'graf'].forEach(a => {
    document.getElementById(`aba-${a}`).classList.toggle('hidden', a !== aba);
    document.getElementById(`btn-${a}`).classList.toggle('active', a === aba);
  });

  const subs = {
    live: 'PAINEL TÁTICO AO VIVO (RAM)',
    ssd:  'BIG DATA — CONSULTA DE BANCOS DE DADOS (SSD)',
    graf: 'PAINEL ANALÍTICO VISUAL',
  };
  document.getElementById('header-sub').textContent = subs[aba];

  if (aba === 'ssd')  carregarSSD();
  if (aba === 'graf') carregarGraficos();
}

// --------------------------------------------------------- utilitários -----

function fmtBRL(valor) {
  return 'R$ ' + parseFloat(valor || 0).toFixed(2).replace('.', ',');
}

function statusBadge(status) {
  const map = {
    'PREPARO': ['status-preparo', 'PREPARO'],
    'ACEITO':  ['status-aceito',  'ACEITO'],
    'EM ROTA': ['status-em-rota', 'EM ROTA'],
  };
  const [cls, label] = map[status] || ['status-preparo', status];
  return `<span class="status-badge ${cls}">${label}</span>`;
}

function animarValor(id, novoTexto) {
  const el = document.getElementById(id);
  if (el.textContent !== novoTexto) {
    el.textContent = novoTexto;
    el.classList.remove('updated');
    void el.offsetWidth; // força reflow
    el.classList.add('updated');
    setTimeout(() => el.classList.remove('updated'), 800);
  }
}

// --------------------------------------------------------- ABA LIVE --------

let ultimoTotalPedidos = 0;

function atualizarLive() {
  fetch('/api/live')
    .then(r => r.json())
    .then(dados => {
      const m = dados.metricas;

      animarValor('m-total',   String(m.totalNaTela).padStart(2, '0'));
      animarValor('m-revenue', fmtBRL(m.grossRevenue));
      animarValor('m-ticket',  fmtBRL(m.avgTicket));

      // Flash no bloco de métricas se chegou novo pedido
      if (m.totalNaTela > ultimoTotalPedidos) {
        const el = document.querySelector('.metricas');
        el.classList.add('flash');
        setTimeout(() => el.classList.remove('flash'), 600);
      }
      ultimoTotalPedidos = m.totalNaTela;

      document.getElementById('qtd-cozinha').textContent   = dados.qtdCozinha;
      document.getElementById('qtd-logistica').textContent = dados.qtdLogistica;

      renderizarCozinha(dados.cozinha);
      renderizarLogistica(dados.logistica);

      document.getElementById('log-msg').textContent = dados.log;

      const agora = new Date();
      document.getElementById('ultima-atualizacao').textContent =
        agora.toLocaleTimeString('pt-BR');
    })
    .catch(() => {
      document.getElementById('log-msg').textContent =
        'ERRO: Sem conexão com o servidor Flask.';
    });
}

function renderizarCozinha(pedidos) {
  const tbody = document.getElementById('tbody-cozinha');
  if (!pedidos || pedidos.length === 0) {
    tbody.innerHTML = '<tr><td colspan="5" class="vazio">Nenhum pedido em produção</td></tr>';
    return;
  }
  tbody.innerHTML = pedidos.map(p => `
    <tr>
      <td class="display-id">#${p.displayId}</td>
      <td>${statusBadge(p.status)}</td>
      <td class="${p.atrasado ? 'timer-atrasado' : 'timer-ok'}">${p.timerStr}</td>
      <td>${p.cliente}</td>
      <td>${p.resumoItens || '—'}</td>
    </tr>
  `).join('');
}

function renderizarLogistica(pedidos) {
  const tbody = document.getElementById('tbody-logistica');
  if (!pedidos || pedidos.length === 0) {
    tbody.innerHTML = '<tr><td colspan="5" class="vazio">Nenhum pedido em rota</td></tr>';
    return;
  }
  tbody.innerHTML = pedidos.map(p => `
    <tr>
      <td class="display-id">#${p.displayId}</td>
      <td>${statusBadge(p.status)}</td>
      <td class="${p.atrasado ? 'timer-atrasado' : 'timer-ok'}">${p.timerStr}</td>
      <td>${p.bairro}</td>
      <td class="valor-cell">${fmtBRL(p.valor)}</td>
    </tr>
  `).join('');
}

// --------------------------------------------------------- ABA SSD ---------

function carregarSSD() {
  fetch('/api/historico')
    .then(r => r.json())
    .then(dados => {
      if (!dados.analytics) {
        document.getElementById('tbody-ssd').innerHTML =
          '<tr><td colspan="8" class="vazio">Banco de dados SSD vazio. Nenhum pedido finalizado ainda.</td></tr>';
        return;
      }

      const a = dados.analytics;
      document.getElementById('a-total').textContent       = a.total;
      document.getElementById('a-concluidos').textContent  = a.concluidos;
      document.getElementById('a-cancelados').textContent  = a.cancelados;
      document.getElementById('a-faturamento').textContent = fmtBRL(a.faturamento);
      document.getElementById('a-ticket').textContent      = fmtBRL(a.ticketMedio);
      document.getElementById('a-taxas').textContent       = fmtBRL(a.totalTaxas);
      document.getElementById('a-itens').textContent       = a.totalItens + ' un.';

      const tbody = document.getElementById('tbody-ssd');
      if (!dados.registros || dados.registros.length === 0) {
        tbody.innerHTML = '<tr><td colspan="8" class="vazio">Sem registros</td></tr>';
        return;
      }

      tbody.innerHTML = [...dados.registros].reverse().map(r => {
        const cls = r.status === 'CONCLUDED' ? 's-concluded' : 's-cancelled';
        return `
          <tr>
            <td class="display-id">#${r.displayId}</td>
            <td class="${cls}">${r.status}</td>
            <td>${r.dataHora}</td>
            <td>${r.cliente}</td>
            <td class="valor-cell">${fmtBRL(r.valorBruto)}</td>
            <td>${r.bairro}</td>
            <td>${r.qtdItens} un.</td>
            <td>${r.metodoPagamento}</td>
          </tr>
        `;
      }).join('');
    })
    .catch(() => {
      document.getElementById('tbody-ssd').innerHTML =
        '<tr><td colspan="8" class="vazio">ERRO ao carregar histórico.</td></tr>';
    });
}

// --------------------------------------------------------- ABA GRÁFICOS ----

function destruirChart(id) {
  if (charts[id]) { charts[id].destroy(); delete charts[id]; }
}

function criarLegenda(containerId, labels, cores) {
  const el = document.getElementById(containerId);
  if (!el) return;
  el.innerHTML = labels.map((l, i) => `
    <div class="legenda-item">
      <div class="legenda-cor" style="background:${cores[i % cores.length]}"></div>
      <span>${l}</span>
    </div>
  `).join('');
}

function carregarGraficos() {
  fetch('/api/historico')
    .then(r => r.json())
    .then(dados => {
      const semDados = !dados.analytics || dados.registros.length === 0;
      document.getElementById('graf-vazio').classList.toggle('hidden', !semDados);
      document.querySelector('.graficos-grid').classList.toggle('hidden', semDados);
      if (semDados) return;

      const a = dados.analytics;
      const registros = dados.registros;

      // --- 1. Donut: Status dos pedidos ---
      destruirChart('status');
      const ctxStatus = document.getElementById('chart-status').getContext('2d');
      const coresStatus = [CORES[1], CORES[5]];
      charts['status'] = new Chart(ctxStatus, {
        type: 'doughnut',
        data: {
          labels: ['Concluídos', 'Cancelados'],
          datasets: [{
            data: [a.concluidos, a.cancelados],
            backgroundColor: coresStatus,
            borderColor: '#161b22',
            borderWidth: 3,
            hoverOffset: 8,
          }],
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          cutout: '65%',
          plugins: {
            legend: { display: false },
            tooltip: {
              callbacks: {
                label: ctx => ` ${ctx.label}: ${ctx.parsed} pedidos`,
              },
            },
          },
        },
      });
      criarLegenda('legenda-status', ['Concluídos', 'Cancelados'], coresStatus);

      // --- 2. Donut: Métodos de pagamento ---
      destruirChart('pagamento');
      const pagamentos = {};
      registros.forEach(r => {
        const m = r.metodoPagamento || 'DESCONHECIDO';
        pagamentos[m] = (pagamentos[m] || 0) + 1;
      });
      const labPag  = Object.keys(pagamentos);
      const valPag  = Object.values(pagamentos);
      const coresPag = labPag.map((_, i) => CORES[i % CORES.length]);

      const ctxPag = document.getElementById('chart-pagamento').getContext('2d');
      charts['pagamento'] = new Chart(ctxPag, {
        type: 'doughnut',
        data: {
          labels: labPag,
          datasets: [{
            data: valPag,
            backgroundColor: coresPag,
            borderColor: '#161b22',
            borderWidth: 3,
            hoverOffset: 8,
          }],
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          cutout: '65%',
          plugins: {
            legend: { display: false },
            tooltip: {
              callbacks: {
                label: ctx => ` ${ctx.label}: ${ctx.parsed} pedidos`,
              },
            },
          },
        },
      });
      criarLegenda('legenda-pagamento', labPag, coresPag);

      // --- 3. Barras horizontais: Top bairros por receita ---
      destruirChart('bairros');
      const bairros = {};
      registros
        .filter(r => r.status === 'CONCLUDED')
        .forEach(r => {
          const b = r.bairro || 'DESCONHECIDO';
          bairros[b] = (bairros[b] || 0) + parseFloat(r.valorBruto || 0);
        });

      const top5 = Object.entries(bairros)
        .sort((a, b) => b[1] - a[1])
        .slice(0, 7);

      const ctxBairros = document.getElementById('chart-bairros').getContext('2d');
      charts['bairros'] = new Chart(ctxBairros, {
        type: 'bar',
        data: {
          labels: top5.map(([b]) => b),
          datasets: [{
            label: 'Receita (R$)',
            data: top5.map(([, v]) => parseFloat(v.toFixed(2))),
            backgroundColor: top5.map((_, i) => CORES[i % CORES.length] + 'cc'),
            borderColor:     top5.map((_, i) => CORES[i % CORES.length]),
            borderWidth: 1,
            borderRadius: 4,
          }],
        },
        options: {
          indexAxis: 'y',
          responsive: true,
          maintainAspectRatio: false,
          plugins: {
            legend: { display: false },
            tooltip: {
              callbacks: {
                label: ctx => ` R$ ${ctx.parsed.x.toFixed(2)}`,
              },
            },
          },
          scales: {
            x: {
              grid: { color: '#2a3a5a' },
              ticks: {
                color: '#5a6a7a',
                callback: v => `R$ ${v}`,
              },
            },
            y: { grid: { color: '#2a3a5a' }, ticks: { color: '#e8edf5' } },
          },
        },
      });

      // --- 4. Linha: Receita acumulada por pedido ---
      destruirChart('receita');
      const concluidos = registros.filter(r => r.status === 'CONCLUDED');
      let acumulado = 0;
      const valoresAcumulados = concluidos.map((r, i) => {
        acumulado += parseFloat(r.valorBruto || 0);
        return { x: i + 1, y: parseFloat(acumulado.toFixed(2)) };
      });

      const ctxReceita = document.getElementById('chart-receita').getContext('2d');
      charts['receita'] = new Chart(ctxReceita, {
        type: 'line',
        data: {
          labels: valoresAcumulados.map(p => `#${p.x}`),
          datasets: [{
            label: 'Receita Acumulada',
            data: valoresAcumulados.map(p => p.y),
            borderColor: CORES[0],
            backgroundColor: CORES[0] + '22',
            borderWidth: 2,
            pointBackgroundColor: CORES[0],
            pointRadius: valoresAcumulados.length > 30 ? 0 : 4,
            fill: true,
            tension: 0.4,
          }],
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          plugins: {
            legend: { display: false },
            tooltip: {
              callbacks: {
                label: ctx => ` R$ ${ctx.parsed.y.toFixed(2)}`,
              },
            },
          },
          scales: {
            x: {
              grid: { color: '#2a3a5a' },
              ticks: { color: '#5a6a7a', maxTicksLimit: 10 },
            },
            y: {
              grid: { color: '#2a3a5a' },
              ticks: {
                color: '#5a6a7a',
                callback: v => `R$ ${v}`,
              },
            },
          },
        },
      });
    })
    .catch(e => console.error('Erro ao carregar gráficos:', e));
}

// --------------------------------------------------------- iniciar ---------

atualizarLive();

setInterval(() => {
  if (abaAtual === 'live') atualizarLive();
}, INTERVALO * 1000);
