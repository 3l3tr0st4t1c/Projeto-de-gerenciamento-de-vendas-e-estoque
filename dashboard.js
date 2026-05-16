/* ============================================================
   dashboard.js — Frontend do SGA v7.0.0
   Os dados vêm do estado_atual.json gerado pelo programa C.
   O servidor.py apenas serve esse arquivo via /estado.
   ============================================================ */

const INTERVALO = 7;
let abaAtual = 'live';
let countdown = INTERVALO;
let charts = {};

const CORES = [
  '#00e5ff','#2ecc40','#ffd700','#ff44ff',
  '#1a8cff','#ff3b30','#ff9500','#5ac8fa',
  '#30d158','#bf5af2',
];

Chart.defaults.color       = '#e8edf5';
Chart.defaults.borderColor = '#2a3a5a';

// ---- relógio ----
function atualizarRelogio() {
  const a = new Date();
  const pad = n => String(n).padStart(2,'0');
  document.getElementById('header-clock').textContent =
    `${pad(a.getHours())}:${pad(a.getMinutes())}:${pad(a.getSeconds())}`;
}
setInterval(atualizarRelogio, 1000);
atualizarRelogio();

// ---- countdown (dinâmico em todas as abas) ----
setInterval(() => {
  countdown--;
  if (countdown <= 0) countdown = INTERVALO;
  document.getElementById('refresh-countdown').textContent =
    `próx. atualização em ${countdown}s`;
}, 1000);

// ---- navegação ----
function mudarAba(aba) {
  abaAtual = aba;
  ['live','ssd','graf'].forEach(a => {
    document.getElementById(`aba-${a}`).classList.toggle('hidden', a !== aba);
    document.getElementById(`btn-${a}`).classList.toggle('active',  a === aba);
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

// ---- utilitários ----
function fmtBRL(v) {
  return 'R$ ' + parseFloat(v || 0).toFixed(2).replace('.', ',');
}

function statusBadge(s) {
  const m = {
    'PREPARO': ['status-preparo','PREPARO'],
    'ACEITO':  ['status-aceito', 'ACEITO'],
    'EM ROTA': ['status-em-rota','EM ROTA'],
  };
  const [cls, label] = m[s] || ['status-preparo', s];
  return `<span class="status-badge ${cls}">${label}</span>`;
}

function animarValor(id, texto) {
  const el = document.getElementById(id);
  if (el.textContent !== texto) {
    el.textContent = texto;
    el.classList.remove('updated');
    void el.offsetWidth;
    el.classList.add('updated');
    setTimeout(() => el.classList.remove('updated'), 800);
  }
}

// ---- ABA LIVE — lê /estado (gerado pelo C) ----
let ultimoTotal = 0;

function atualizarLive() {
  fetch('/estado')
    .then(r => r.json())
    .then(d => {
      const m = d.metricas || {};
      animarValor('m-total',   String(m.totalNaTela || 0).padStart(2,'0'));
      animarValor('m-revenue', fmtBRL(m.grossRevenue));
      animarValor('m-ticket',  fmtBRL(m.avgTicket));

      if ((m.totalNaTela || 0) > ultimoTotal) {
        const el = document.querySelector('.metricas');
        el.classList.add('flash');
        setTimeout(() => el.classList.remove('flash'), 600);
      }
      ultimoTotal = m.totalNaTela || 0;

      document.getElementById('qtd-cozinha').textContent   = d.qtdCozinha   || 0;
      document.getElementById('qtd-logistica').textContent = d.qtdLogistica || 0;

      renderTbody('tbody-cozinha',   d.cozinha   || [], renderRowCozinha,   5);
      renderTbody('tbody-logistica', d.logistica || [], renderRowLogistica, 5);

      document.getElementById('log-msg').textContent = d.log || '—';
      document.getElementById('ultima-atualizacao').textContent =
        new Date().toLocaleTimeString('pt-BR');
    })
    .catch(() => {
      document.getElementById('log-msg').textContent =
        'ERRO: servidor.py não está rodando ou programa C ainda não iniciou.';
    });
}

function renderTbody(id, items, rowFn, cols) {
  const tb = document.getElementById(id);
  if (!items.length) {
    tb.innerHTML = `<tr><td colspan="${cols}" class="vazio">Nenhum registro</td></tr>`;
    return;
  }
  tb.innerHTML = items.map(rowFn).join('');
}

function renderRowCozinha(p) {
  return `<tr>
    <td class="display-id">#${p.displayId}</td>
    <td>${statusBadge(p.status)}</td>
    <td class="${p.atrasado ? 'timer-atrasado' : 'timer-ok'}">${p.timerStr}</td>
    <td>${p.cliente}</td>
    <td>${p.resumoItens || '—'}</td>
  </tr>`;
}

function renderRowLogistica(p) {
  return `<tr>
    <td class="display-id">#${p.displayId}</td>
    <td>${statusBadge(p.status)}</td>
    <td class="${p.atrasado ? 'timer-atrasado' : 'timer-ok'}">${p.timerStr}</td>
    <td>${p.bairro}</td>
    <td class="valor-cell">${fmtBRL(p.valor)}</td>
  </tr>`;
}

// ---- ABA SSD ----
function carregarSSD() {
  fetch('/historico')
    .then(r => r.json())
    .then(d => {
      const a = d.analytics;
      document.getElementById('a-total').textContent       = a.total;
      document.getElementById('a-concluidos').textContent  = a.concluidos;
      document.getElementById('a-cancelados').textContent  = a.cancelados;
      document.getElementById('a-faturamento').textContent = fmtBRL(a.faturamento);
      document.getElementById('a-ticket').textContent      = fmtBRL(a.ticketMedio);
      document.getElementById('a-taxas').textContent       = fmtBRL(a.totalTaxas);
      document.getElementById('a-itens').textContent       = a.totalItens + ' un.';

      const tb = document.getElementById('tbody-ssd');
      if (!d.registros.length) {
        tb.innerHTML = '<tr><td colspan="8" class="vazio">Banco de dados SSD vazio.</td></tr>';
        return;
      }
      tb.innerHTML = [...d.registros].reverse().map(r => {
        const cls = r.status === 'CONCLUDED' ? 's-concluded' : 's-cancelled';
        return `<tr>
          <td class="display-id">#${r.displayId}</td>
          <td class="${cls}">${r.status}</td>
          <td>${r.dataHora}</td>
          <td>${r.cliente}</td>
          <td class="valor-cell">${fmtBRL(r.valorBruto)}</td>
          <td>${r.bairro}</td>
          <td>${r.qtdItens} un.</td>
          <td>${r.metodoPagamento}</td>
        </tr>`;
      }).join('');
    });
}

// ---- ABA GRÁFICOS ----
function destruirChart(id) {
  if (charts[id]) { charts[id].destroy(); delete charts[id]; }
}

function criarLegenda(containerId, labels, cores) {
  const el = document.getElementById(containerId);
  if (!el) return;
  el.innerHTML = labels.map((l, i) =>
    `<div class="legenda-item">
       <div class="legenda-cor" style="background:${cores[i % cores.length]}"></div>
       <span>${l}</span>
     </div>`
  ).join('');
}

function carregarGraficos() {
  fetch('/historico')
    .then(r => r.json())
    .then(d => {
      const semDados = !d.registros || d.registros.length === 0;
      document.getElementById('graf-vazio').classList.toggle('hidden',  !semDados);
      document.getElementById('graficos-grid').classList.toggle('hidden', semDados);
      if (semDados) return;

      const a = d.analytics;
      const registros = d.registros;

      // Donut status
      destruirChart('status');
      const coresStatus = [CORES[1], CORES[5]];
      charts['status'] = new Chart(
        document.getElementById('chart-status').getContext('2d'), {
          type: 'doughnut',
          data: {
            labels: ['Concluídos','Cancelados'],
            datasets: [{ data: [a.concluidos, a.cancelados],
              backgroundColor: coresStatus, borderColor: '#161b22', borderWidth: 3, hoverOffset: 8 }]
          },
          options: { responsive: true, maintainAspectRatio: false, cutout: '65%',
            plugins: { legend: { display: false } } }
        }
      );
      criarLegenda('legenda-status', ['Concluídos','Cancelados'], coresStatus);

      // Donut pagamento
      destruirChart('pagamento');
      const pags = {};
      registros.forEach(r => { const m = r.metodoPagamento || 'DESCONHECIDO'; pags[m] = (pags[m]||0)+1; });
      const labPag = Object.keys(pags), valPag = Object.values(pags);
      const coresPag = labPag.map((_,i) => CORES[i % CORES.length]);
      charts['pagamento'] = new Chart(
        document.getElementById('chart-pagamento').getContext('2d'), {
          type: 'doughnut',
          data: { labels: labPag, datasets: [{ data: valPag,
            backgroundColor: coresPag, borderColor: '#161b22', borderWidth: 3, hoverOffset: 8 }] },
          options: { responsive: true, maintainAspectRatio: false, cutout: '65%',
            plugins: { legend: { display: false } } }
        }
      );
      criarLegenda('legenda-pagamento', labPag, coresPag);

      // Barras bairros
      destruirChart('bairros');
      const bairros = {};
      registros.filter(r => r.status === 'CONCLUDED')
        .forEach(r => { const b = r.bairro||'DESCONHECIDO'; bairros[b] = (bairros[b]||0)+parseFloat(r.valorBruto||0); });
      const top7 = Object.entries(bairros).sort((a,b)=>b[1]-a[1]).slice(0,7);
      charts['bairros'] = new Chart(
        document.getElementById('chart-bairros').getContext('2d'), {
          type: 'bar',
          data: { labels: top7.map(([b])=>b), datasets: [{
            label: 'Receita (R$)', data: top7.map(([,v])=>parseFloat(v.toFixed(2))),
            backgroundColor: top7.map((_,i)=>CORES[i%CORES.length]+'cc'),
            borderColor: top7.map((_,i)=>CORES[i%CORES.length]), borderWidth:1, borderRadius:4
          }]},
          options: { indexAxis:'y', responsive:true, maintainAspectRatio:false,
            plugins: { legend:{display:false} },
            scales: { x:{grid:{color:'#2a3a5a'}, ticks:{color:'#5a6a7a',callback:v=>`R$ ${v}`}},
                      y:{grid:{color:'#2a3a5a'}, ticks:{color:'#e8edf5'}} } }
        }
      );

      // Linha receita acumulada
      destruirChart('receita');
      let acum = 0;
      const pontos = registros.filter(r=>r.status==='CONCLUDED').map((r,i)=>({
        x: i+1, y: parseFloat((acum += parseFloat(r.valorBruto||0)).toFixed(2))
      }));
      charts['receita'] = new Chart(
        document.getElementById('chart-receita').getContext('2d'), {
          type: 'line',
          data: { labels: pontos.map(p=>`#${p.x}`), datasets: [{
            label:'Receita Acumulada', data: pontos.map(p=>p.y),
            borderColor: CORES[0], backgroundColor: CORES[0]+'22',
            borderWidth:2, pointBackgroundColor: CORES[0],
            pointRadius: pontos.length>30?0:4, fill:true, tension:0.4
          }]},
          options: { responsive:true, maintainAspectRatio:false,
            plugins: { legend:{display:false} },
            scales: { x:{grid:{color:'#2a3a5a'}, ticks:{color:'#5a6a7a',maxTicksLimit:10}},
                      y:{grid:{color:'#2a3a5a'}, ticks:{color:'#5a6a7a',callback:v=>`R$ ${v}`}} } }
        }
      );
    });
}

// ---- iniciar ----
atualizarLive();
setInterval(() => { if (abaAtual === 'live') atualizarLive(); }, INTERVALO * 1000);
