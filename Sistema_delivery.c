#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <windows.h>
#include <time.h>

/* =============================================================================
   [ VITOR DELIVERY V7.0 - ULTIMATE CORE ]
   ARQUITETURA DE SISTEMAS DE INFORMAÇÃO - CARTA BRANCA EDITION
   ============================================================================= */

#define VERSAO "7.0.0-ULTIMATE"
#define INTERVALO_POLLING 7
#define LIMITE_ATRASO_COZINHA 1200 
#define LIMITE_ATRASO_ENTREGA 2400 

// Paleta de Cores ANSI Avançada
#define COR_RESET   "\033[0m"
#define COR_VERDE   "\033[38;5;46m"
#define COR_AMARELO "\033[38;5;226m"
#define COR_VERMELHO "\033[38;5;196m"
#define COR_CIANO   "\033[38;5;51m"
#define COR_MAGENTA "\033[38;5;201m"
#define COR_AZUL    "\033[38;5;33m"
#define COR_BRANCO  "\033[38;5;231m"
#define COR_CINZA   "\033[38;5;240m"
#define FUNDO_AZUL  "\033[48;5;17m"

/* =============================================================================
   MODELAGEM DOS 5 BANCOS DE DADOS (STRUCTS ANINHADAS)
   ============================================================================= */

// BANCO 1: Cliente
typedef struct {
    char nome[100];
    char telefone[20];
    char idCliente[50];
} BancoCliente;

// BANCO 2: Financeiro
typedef struct {
    double valorBruto;
    double taxaEntrega;
    char metodoPagamento[50];
} BancoFinanceiro;

// BANCO 3: Logística / Entrega
typedef struct {
    char bairro[100];
    char logradouro[150];
    char numero[20];
} BancoEntrega;

// BANCO 4: DNA do Pedido (Itens)
typedef struct {
    int quantidadeTotalItens;
    char resumoItens[300]; // Uma string concatenada com os nomes dos itens
} BancoDNA;

// BANCO 5: Identidade e Controle (Master Struct)
typedef struct PedidoAtivo {
    char orderId[50];
    char displayId[15];
    char status[30];
    time_t momentoEntrada;
    time_t momentoDespacho;
    
    // Aninhamento dos outros 4 bancos
    BancoCliente cliente;
    BancoFinanceiro financeiro;
    BancoEntrega entrega;
    BancoDNA dna;

    struct PedidoAtivo *prox;
    struct PedidoAtivo *prev;
} PedidoAtivo;

// Controle de Memória RAM Global
typedef struct {
    PedidoAtivo *inicio;
    int qtdCozinha;
    int qtdEntrega;
    double faturamentoLive;
} ListaGestao;

struct MemoryStruct {
    char *memory;
    size_t size;
};

// Variaveis de Estado do Sistema
ListaGestao g_listaRAM = {NULL, 0, 0, 0.0};
char g_accessToken[2048] = "";
time_t g_proxima_renovacao = 0;
char g_ultimo_log[256] = "V7 Inicializado. Aguardando eventos...";

// Constantes de Acesso (API iFood)
const char *CLIENT_ID = "0ed948ff-e60d-4bee-bd6f-f8bdc9856ab0";
const char *CLIENT_SECRET = "c6ml41du8cormt3v55zqtqc70ybkg6ty6ibnbozpg0qphh7od80dwtr1uqdhwzhq2ugrxhxmj15jc9gs3hvauh3m25t7xa8myl4";

/* =============================================================================
   PROTÓTIPOS DA ENGENHARIA DE SOFTWARE
   ============================================================================= */
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
void registrar_log(const char *msg);
void garantir_token_valido();
void limpar_string(char *str);
void parse_detalhes_pedido(PedidoAtivo *novo, const char *json_string);
void buscar_detalhes_e_adicionar_ram(const char *orderId);
void processar_mudanca_estado(const char *orderId, const char *fullCode);
void finalizar_pedido(PedidoAtivo *p, const char *resultado);
void salvar_banco_ssd(PedidoAtivo *p, const char *resultado);
void enviar_ack(const char *json_eventos);
void desenhar_cabecalho(const char *titulo);
void desenhar_monitor_operacional_live();
void exibir_painel_bancos_ssd();
void modo_vigilante_live();

/* =============================================================================
   NÚCLEO DE INFRAESTRUTURA E REDE (cURL)
   ============================================================================= */

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) return 0;
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

void registrar_log(const char *msg) {
    time_t agora = time(NULL);
    struct tm *t = localtime(&agora);
    sprintf(g_ultimo_log, "[%02d:%02d:%02d] %s", t->tm_hour, t->tm_min, t->tm_sec, msg);
}

void limpar_string(char *str) {
    // Remove quebras de linha e pipes que podem quebrar o CSV/TXT do SSD
    for(int i = 0; str[i] != '\0'; i++) {
        if(str[i] == '\n' || str[i] == '\r' || str[i] == '|') {
            str[i] = ' ';
        }
    }
}

void garantir_token_valido() {
    time_t agora = time(NULL);
    if (strlen(g_accessToken) > 0 && agora < g_proxima_renovacao) return;

    CURL *curl = curl_easy_init();
    struct MemoryStruct chunk = {malloc(1), 0};
    char fields[1024];
    sprintf(fields, "grantType=client_credentials&clientId=%s&clientSecret=%s", CLIENT_ID, CLIENT_SECRET);

    curl_easy_setopt(curl, CURLOPT_URL, "https://merchant-api.ifood.com.br/authentication/v1.0/oauth/token");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, fields);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    if (curl_easy_perform(curl) == CURLE_OK) {
        cJSON *json = cJSON_Parse(chunk.memory);
        if (json) {
            cJSON *token = cJSON_GetObjectItem(json, "accessToken");
            if (token) {
                strcpy(g_accessToken, token->valuestring);
                g_proxima_renovacao = agora + 3500;
                registrar_log("Token API Renovado com Sucesso. Conexao Segura.");
            }
            cJSON_Delete(json);
        }
    } else {
        registrar_log("ERRO CRITICO: Falha na autenticacao com iFood.");
    }
    curl_easy_cleanup(curl);
    free(chunk.memory);
}

/* =============================================================================
   EXTRATOR DE DADOS DE ALTA PRECISÃO (JSON PARSER)
   ============================================================================= */

void parse_detalhes_pedido(PedidoAtivo *novo, const char *json_string) {
    cJSON *det = cJSON_Parse(json_string);
    if (!det) return;

    // 1. Identidade
    cJSON *dId = cJSON_GetObjectItem(det, "displayId");
    strcpy(novo->displayId, dId ? dId->valuestring : "0000");

    // 2. Banco Cliente
    cJSON *customer = cJSON_GetObjectItem(det, "customer");
    if (customer) {
        cJSON *cName = cJSON_GetObjectItem(customer, "name");
        cJSON *cId = cJSON_GetObjectItem(customer, "id");
        cJSON *cPhone = cJSON_GetObjectItem(cJSON_GetObjectItem(customer, "phone"), "number");
        
        strcpy(novo->cliente.nome, cName ? cName->valuestring : "CLIENTE NAO IDENTIFICADO");
        strcpy(novo->cliente.idCliente, cId ? cId->valuestring : "ID_NULL");
        strcpy(novo->cliente.telefone, cPhone ? cPhone->valuestring : "SEM_NUMERO");
    }

    // 3. Banco Financeiro
    cJSON *total = cJSON_GetObjectItem(det, "total");
    if (total) {
        cJSON *orderAmt = cJSON_GetObjectItem(total, "orderAmount");
        cJSON *delivFee = cJSON_GetObjectItem(total, "deliveryFee");
        novo->financeiro.valorBruto = orderAmt ? orderAmt->valuedouble : 0.0;
        novo->financeiro.taxaEntrega = delivFee ? delivFee->valuedouble : 0.0;
    }
    cJSON *payments = cJSON_GetObjectItem(det, "payments");
    if (payments) {
        cJSON *methods = cJSON_GetObjectItem(payments, "methods");
        if (methods && cJSON_IsArray(methods)) {
            cJSON *method0 = cJSON_GetArrayItem(methods, 0);
            cJSON *type = cJSON_GetObjectItem(method0, "method");
            strcpy(novo->financeiro.metodoPagamento, type ? type->valuestring : "DESCONHECIDO");
        }
    }

    // 4. Banco Entrega
    cJSON *delivery = cJSON_GetObjectItem(det, "delivery");
    if (delivery) {
        cJSON *addr = cJSON_GetObjectItem(delivery, "deliveryAddress");
        if (addr) {
            cJSON *neigh = cJSON_GetObjectItem(addr, "neighborhood");
            cJSON *street = cJSON_GetObjectItem(addr, "streetName");
            cJSON *num = cJSON_GetObjectItem(addr, "streetNumber");
            
            strcpy(novo->entrega.bairro, neigh ? neigh->valuestring : "RETIRADA/BALCAO");
            strcpy(novo->entrega.logradouro, street ? street->valuestring : "");
            strcpy(novo->entrega.numero, num ? num->valuestring : "S/N");
        }
    }

    // 5. Banco DNA (Itens do Pedido)
    cJSON *items = cJSON_GetObjectItem(det, "items");
    novo->dna.quantidadeTotalItens = 0;
    strcpy(novo->dna.resumoItens, "");
    
    if (items && cJSON_IsArray(items)) {
        int arrSize = cJSON_GetArraySize(items);
        for (int i = 0; i < arrSize; i++) {
            cJSON *item = cJSON_GetArrayItem(items, i);
            cJSON *qtd = cJSON_GetObjectItem(item, "quantity");
            cJSON *nomeItem = cJSON_GetObjectItem(item, "name");
            
            if (qtd && nomeItem) {
                novo->dna.quantidadeTotalItens += qtd->valueint;
                char bufferItem[150];
                sprintf(bufferItem, "%dx %s, ", qtd->valueint, nomeItem->valuestring);
                
                // Evita estourar o buffer do resumo (300 chars)
                if (strlen(novo->dna.resumoItens) + strlen(bufferItem) < 290) {
                    strcat(novo->dna.resumoItens, bufferItem);
                } else if (strstr(novo->dna.resumoItens, "...") == NULL) {
                    strcat(novo->dna.resumoItens, "..."); // Indica que tem mais
                }
            }
        }
    }
    
    // Limpeza de sujeira nas strings para garantir gravação segura no SSD
    limpar_string(novo->cliente.nome);
    limpar_string(novo->entrega.bairro);
    limpar_string(novo->entrega.logradouro);
    limpar_string(novo->dna.resumoItens);

    cJSON_Delete(det);
}

void buscar_detalhes_e_adicionar_ram(const char *orderId) {
    CURL *curl = curl_easy_init();
    struct MemoryStruct chunk = {malloc(1), 0};
    struct curl_slist *headers = NULL;
    char url[256], auth_h[2048];

    sprintf(url, "https://merchant-api.ifood.com.br/order/v1.0/orders/%s", orderId);
    sprintf(auth_h, "Authorization: Bearer %s", g_accessToken);
    headers = curl_slist_append(headers, auth_h);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    if (curl_easy_perform(curl) == CURLE_OK) {
        PedidoAtivo *novo = malloc(sizeof(PedidoAtivo));
        memset(novo, 0, sizeof(PedidoAtivo)); // Zera a memória estrutural
        
        strcpy(novo->orderId, orderId);
        strcpy(novo->status, "PREPARO");
        novo->momentoEntrada = time(NULL);
        novo->momentoDespacho = 0;

        // Chamada do motor de extração de dados (Preenche os 5 bancos)
        parse_detalhes_pedido(novo, chunk.memory);

        // Inserção na Lista RAM Duplamente Encadeada
        novo->prox = g_listaRAM.inicio;
        novo->prev = NULL;
        if (g_listaRAM.inicio) g_listaRAM.inicio->prev = novo;
        g_listaRAM.inicio = novo;
        
        g_listaRAM.qtdCozinha++;
        g_listaRAM.faturamentoLive += novo->financeiro.valorBruto;
        
        char msg[150];
        sprintf(msg, "NOVO PEDIDO CAPTURADO! Display #%s | Valor: R$%.2f", novo->displayId, novo->financeiro.valorBruto);
        registrar_log(msg);
    }
    curl_easy_cleanup(curl);
    free(chunk.memory);
}

/* =============================================================================
   MOTOR DE ESTADOS E GRAVAÇÃO SSD
   ============================================================================= */

void processar_mudanca_estado(const char *orderId, const char *fullCode) {
    PedidoAtivo *atual = g_listaRAM.inicio;
    while (atual && strcmp(atual->orderId, orderId) != 0) atual = atual->prox;

    if (strcmp(fullCode, "PLACED") == 0 && !atual) {
        buscar_detalhes_e_adicionar_ram(orderId);
    } 
    else if (atual) {
        if (strcmp(fullCode, "CONCLUDED") == 0 || strcmp(fullCode, "CANCELLED") == 0) {
            finalizar_pedido(atual, fullCode);
        } 
        else if (strcmp(fullCode, "DISPATCHED") == 0) {
            strcpy(atual->status, "EM ROTA");
            atual->momentoDespacho = time(NULL);
            g_listaRAM.qtdCozinha--;
            g_listaRAM.qtdEntrega++;
            registrar_log("MOTOBOY LIBERADO: Pedido saiu para entrega.");
        }
        else if (strcmp(fullCode, "CONFIRMED") == 0) {
            strcpy(atual->status, "ACEITO");
            registrar_log("Confirmacao automatica via API registrada.");
        }
    }
}

void salvar_banco_ssd(PedidoAtivo *p, const char *resultado) {
    // Formato Ultra Seguro com delimitador "||" para evitar conflitos com textos sujos
    FILE *arq = fopen("historico_v7.txt", "a");
    if (arq) {
        time_t agora = time(NULL);
        char *tempo = ctime(&agora);
        tempo[strcspn(tempo, "\n")] = 0; // Remove o \n do ctime
        
        // Escreve os 5 bancos serializados
        fprintf(arq, "%s||%s||%s||%s||%s||%.2f||%.2f||%s||%s||%s||%s||%d||%s\n", 
                p->displayId, p->orderId, resultado, tempo, 
                p->cliente.nome, p->financeiro.valorBruto, p->financeiro.taxaEntrega, p->financeiro.metodoPagamento,
                p->entrega.bairro, p->entrega.logradouro, p->entrega.numero,
                p->dna.quantidadeTotalItens, p->dna.resumoItens);
        fclose(arq);
    }
}

void finalizar_pedido(PedidoAtivo *p, const char *resultado) {
    salvar_banco_ssd(p, resultado);

    if (strcmp(p->status, "EM ROTA") == 0) g_listaRAM.qtdEntrega--;
    else g_listaRAM.qtdCozinha--;

    // Remoção da Estrutura Duplamente Encadeada
    if (p->prev) p->prev->prox = p->prox;
    else g_listaRAM.inicio = p->prox;
    if (p->prox) p->prox->prev = p->prev;
    
    char msg[100];
    sprintf(msg, "CICLO DE VIDA ENCERRADO: Pedido #%s (%s). Dados Salvos no SSD.", p->displayId, resultado);
    registrar_log(msg);
    
    free(p); // Prevenção de Memory Leak
}

void enviar_ack(const char *json_eventos) {
    cJSON *root = cJSON_Parse(json_eventos);
    int n = cJSON_GetArraySize(root);
    if (n <= 0) { cJSON_Delete(root); return; }
    
    cJSON *ack_array = cJSON_CreateArray();
    for (int i = 0; i < n; i++) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "id", cJSON_GetObjectItem(cJSON_GetArrayItem(root, i), "id")->valuestring);
        cJSON_AddItemToArray(ack_array, obj);
    }
    
    char *payload = cJSON_PrintUnformatted(ack_array);
    CURL *curl = curl_easy_init();
    struct curl_slist *headers = NULL;
    char auth_h[2048];
    sprintf(auth_h, "Authorization: Bearer %s", g_accessToken);
    headers = curl_slist_append(headers, auth_h);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    curl_easy_setopt(curl, CURLOPT_URL, "https://merchant-api.ifood.com.br/order/v1.0/events/acknowledgment");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_perform(curl);
    
    curl_easy_cleanup(curl);
    cJSON_Delete(root);
    free(payload);
}

/* =============================================================================
   INTERFACE GRÁFICA DO USUÁRIO (TUI) - AGRESSIVA & PRO
   ============================================================================= */

void desenhar_cabecalho(const char *titulo) {
    system("cls");
    printf(FUNDO_AZUL COR_BRANCO "================================================================================\n");
    printf(" || VITOR DELIVERY || SGA v%s || %s \n", VERSAO, titulo);
    printf("================================================================================\n" COR_RESET);
}

void desenhar_monitor_operacional_live() {
    desenhar_cabecalho("PAINEL TÁTICO AO VIVO (RAM)");
    
    time_t agora = time(NULL);
    double ticketMedio = (g_listaRAM.qtdCozinha + g_listaRAM.qtdEntrega > 0) ? 
                         g_listaRAM.faturamentoLive / (g_listaRAM.qtdCozinha + g_listaRAM.qtdEntrega) : 0.0;

    printf(COR_VERDE " \n[ METRICAS EM TEMPO REAL ]\n" COR_RESET);
    printf(" " COR_BRANCO "Total na Tela:" COR_RESET " %02d   |   " COR_BRANCO "Gross Revenue:" COR_RESET " R$ %8.2f   |   " COR_BRANCO "Avg. Ticket:" COR_RESET " R$ %5.2f\n\n", 
           g_listaRAM.qtdCozinha + g_listaRAM.qtdEntrega, g_listaRAM.faturamentoLive, ticketMedio);

    // BOX DE PRODUÇÃO (COZINHA)
    printf(COR_AMARELO " [>] ESTAÇÃO DE PRODUÇÃO (Cozinha & Preparo) - %d Ativos\n" COR_RESET, g_listaRAM.qtdCozinha);
    printf(" %-8s | %-12s | %-10s | %-15s | %-15s\n", "DISPLAY", "STATUS", "TIMER", "CLIENTE", "RESUMO ITENS");
    printf(COR_CINZA "--------------------------------------------------------------------------------\n" COR_RESET);
    
    PedidoAtivo *atual = g_listaRAM.inicio;
    while (atual) {
        if (strcmp(atual->status, "EM ROTA") != 0) {
            int segs = (int)difftime(agora, atual->momentoEntrada);
            char *corTimer = (segs > LIMITE_ATRASO_COZINHA) ? COR_VERMELHO : COR_BRANCO;
            
            // Tratamento de string para caber na tabela
            char cli[16], dnaRes[16];
            snprintf(cli, 16, "%s", atual->cliente.nome);
            snprintf(dnaRes, 16, "%s", atual->dna.resumoItens);

            printf(" " COR_VERDE "#%s" COR_RESET "  | %-12s | %s%02dm %02ds%s   | %-15s | %-15s\n", 
                   atual->displayId, atual->status, corTimer, segs/60, segs%60, COR_RESET, cli, dnaRes);
        }
        atual = atual->prox;
    }

    // BOX DE LOGÍSTICA (MOTOBOYS)
    printf(COR_MAGENTA "\n [>] ESTAÇÃO DE LOGÍSTICA (Despacho & Rotas) - %d Ativos\n" COR_RESET, g_listaRAM.qtdEntrega);
    printf(" %-8s | %-12s | %-10s | %-15s | %-15s\n", "DISPLAY", "STATUS", "TIMER", "BAIRRO", "VALOR");
    printf(COR_CINZA "--------------------------------------------------------------------------------\n" COR_RESET);
    
    atual = g_listaRAM.inicio;
    while (atual) {
        if (strcmp(atual->status, "EM ROTA") == 0) {
            int segs = (int)difftime(agora, atual->momentoDespacho);
            char *corTimer = (segs > LIMITE_ATRASO_ENTREGA) ? COR_VERMELHO : COR_BRANCO;
            
            char bairroCurto[16];
            snprintf(bairroCurto, 16, "%s", atual->entrega.bairro);

            printf(" " COR_MAGENTA "#%s" COR_RESET "  | %-12s | %s%02dm %02ds%s   | %-15s | R$ %.2f\n", 
                   atual->displayId, atual->status, corTimer, segs/60, segs%60, COR_RESET, bairroCurto, atual->financeiro.valorBruto);
        }
        atual = atual->prox;
    }

    printf(COR_AZUL "\n================================================================================\n" COR_RESET);
    printf(COR_CINZA " TERMINAL LOG: %s\n", g_ultimo_log);
    printf(" [ PRESSIONE CTRL+C PARA ABORTAR O LOOP E RETORNAR AO MENU ]\n" COR_RESET);
}

// O NOVO MODO SSD: LENDO OS 5 BANCOS
void exibir_painel_bancos_ssd() {
    desenhar_cabecalho("BIG DATA - CONSULTA DE BANCOS DE DADOS (SSD)");
    FILE *arq = fopen("historico_v7.txt", "r");
    
    if (!arq) { 
        printf(COR_AMARELO "\n [!] O Banco de Dados SSD esta vazio ou nao foi criado.\n" COR_RESET); 
        system("pause"); 
        return; 
    }

    char linha[2048];
    int totalRegistros = 0, concluidos = 0, cancelados = 0;
    double faturamentoGeral = 0.0, totalTaxas = 0.0;
    int totalItensVendidos = 0;

    // 1ª PASSAGEM: Varredura de Métricas (Analytics)
    while (fgets(linha, sizeof(linha), arq)) {
        totalRegistros++;
        char *copia = strdup(linha);
        
        // Pula até o status (3º campo) e o valor (6º campo)
        char *token = strtok(copia, "||"); // display
        token = strtok(NULL, "||"); // orderId
        char *status = strtok(NULL, "||"); 
        strtok(NULL, "||"); // datahora
        strtok(NULL, "||"); // cliente
        char *valorStr = strtok(NULL, "||");
        char *taxaStr = strtok(NULL, "||");
        strtok(NULL, "||"); // metodo
        strtok(NULL, "||"); // bairro
        strtok(NULL, "||"); // logradouro
        strtok(NULL, "||"); // num
        char *itensStr = strtok(NULL, "||");

        if (status && strcmp(status, "CONCLUDED") == 0) {
            concluidos++;
            if(valorStr) faturamentoGeral += atof(valorStr);
            if(taxaStr) totalTaxas += atof(taxaStr);
            if(itensStr) totalItensVendidos += atoi(itensStr);
        } else if (status && strcmp(status, "CANCELLED") == 0) {
            cancelados++;
        }
        free(copia);
    }
    
    double tckMedio = (concluidos > 0) ? (faturamentoGeral / concluidos) : 0;

    // Renderização do Dashboard Analítico
    printf(COR_VERDE "\n [ ANALYTICS MACRO ]" COR_RESET " Resumo do Fechamento\n");
    printf(COR_CINZA " ------------------------------------------------------------------------------\n" COR_RESET);
    printf(" Volume de Ordens: %d (%d Concluidos | %d Cancelados)\n", totalRegistros, concluidos, cancelados);
    printf(" " COR_VERDE "Receita Bruta:" COR_RESET " R$ %.2f   |   " COR_VERDE "Ticket Medio:" COR_RESET " R$ %.2f\n", faturamentoGeral, tckMedio);
    printf(" Taxas de Entrega: R$ %.2f   |   Volume de Itens Produzidos: %d unidades\n\n", totalTaxas, totalItensVendidos);

    // 2ª PASSAGEM: Tabela de Dados Detalhada (Deep Dive nos 5 Bancos)
    rewind(arq);
    
    printf(COR_CIANO " [ DUMP DE DADOS - ÚLTIMOS REGISTROS ]\n" COR_RESET);
    printf(COR_CINZA " %-6s | %-10s | %-12s | %-10s | %-15s | %-10s\n", "ID", "STATUS", "CLIENTE", "FINANCEIRO", "LOGISTICA (BAIRRO)", "DNA (ITENS)");
    printf(" ------------------------------------------------------------------------------\n" COR_RESET);

    // Vamos exibir apenas os ultimos para nao poluir se houver milhares
    // Numa versão de produção, implementaríamos paginação real aqui.
    while (fgets(linha, sizeof(linha), arq)) {
        // Remontando a linha lida usando strtok_r (mais seguro) ou sscanf avançado
        char disp[15]="", order[50]="", sts[30]="", datah[50]="";
        char cli[100]="", val[20]="", tax[20]="", met[50]="";
        char bai[100]="", logr[150]="", num[20]="";
        char qItens[10]="", resItens[300]="";

        // Usando ponteiros para fatiar o delimitador "||"
        char *p = linha;
        char *tokens[13];
        for(int i=0; i<13; i++) {
            tokens[i] = p;
            p = strstr(p, "||");
            if(p) {
                *p = '\0'; // Quebra a string
                p += 2;    // Pula o "||"
            }
        }
        
        char *corSts = (strcmp(tokens[2], "CANCELLED") == 0) ? COR_VERMELHO : COR_VERDE;
        
        // Tratamento visual para não quebrar a interface
        char nomeCli[13], financeiro[11], bairro[16], dnaCurto[11];
        snprintf(nomeCli, 13, "%s", tokens[4]);
        snprintf(financeiro, 11, "R$ %s", tokens[5]);
        snprintf(bairro, 16, "%s", tokens[8]);
        snprintf(dnaCurto, 11, "%s un.", tokens[11]); // Qtd de Itens

        printf(" %-6s | %s%-10s%s | %-12s | %-10s | %-15s | %-10s\n", 
               tokens[0], corSts, tokens[2], COR_RESET, nomeCli, financeiro, bairro, dnaCurto);
    }
    
    fclose(arq);
    printf(COR_CINZA " ------------------------------------------------------------------------------\n" COR_RESET);
    printf(" Pressione qualquer tecla para voltar ao centro de comando...\n");
    system("pause > nul");
}

/* =============================================================================
   MOTORES DE EXECUÇÃO (LOOP PRINCIPAL)
   ============================================================================= */

void modo_vigilante_live() {
    registrar_log("Motor V7 engatado. Iniciando Polling iFood.");
    while(1) {
        garantir_token_valido();
        struct MemoryStruct poll_chunk = {malloc(1), 0};
        CURL *curl_poll = curl_easy_init();
        struct curl_slist *headers = NULL;
        char auth_h[2048];
        sprintf(auth_h, "Authorization: Bearer %s", g_accessToken);
        headers = curl_slist_append(headers, auth_h);

        curl_easy_setopt(curl_poll, CURLOPT_URL, "https://merchant-api.ifood.com.br/order/v1.0/events:polling");
        curl_easy_setopt(curl_poll, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl_poll, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl_poll, CURLOPT_WRITEDATA, (void *)&poll_chunk);

        if (curl_easy_perform(curl_poll) == CURLE_OK) {
            cJSON *eventos = cJSON_Parse(poll_chunk.memory);
            if (eventos && cJSON_IsArray(eventos)) {
                int n = cJSON_GetArraySize(eventos);
                for (int i = 0; i < n; i++) {
                    cJSON *ev = cJSON_GetArrayItem(eventos, i);
                    cJSON *oId = cJSON_GetObjectItem(ev, "orderId");
                    cJSON *fCode = cJSON_GetObjectItem(ev, "fullCode");
                    if (oId && fCode) processar_mudanca_estado(oId->valuestring, fCode->valuestring);
                }
                if (n > 0) enviar_ack(poll_chunk.memory);
                cJSON_Delete(eventos);
            }
        }
        curl_easy_cleanup(curl_poll);
        free(poll_chunk.memory);
        
        desenhar_monitor_operacional_live();
        Sleep(INTERVALO_POLLING * 1000);
    }
}

int main(void) {
    // Configura o console para UTF-8 (Caracteres Especiais)
    SetConsoleOutputCP(65001); 
    curl_global_init(CURL_GLOBAL_ALL);

    int opcao = 0;
    while(opcao != 3) {
        desenhar_cabecalho("CENTRO DE COMANDO PRINCIPAL");
        
        printf(COR_CIANO " [1]" COR_RESET " INICIAR MOTOR LIVE (Dashboard Tempo Real)\n");
        printf(COR_CIANO " [2]" COR_RESET " CONSULTAR BIG DATA SSD (Analise dos 5 Bancos)\n");
        printf(COR_CIANO " [3]" COR_RESET " ENCERRAR SISTEMA\n\n");
        
        printf(COR_AMARELO " ACAO DE COMANDO: " COR_RESET);
        
        if (scanf("%d", &opcao) != 1) {
            while(getchar() != '\n'); 
            continue;
        }

        switch(opcao) {
            case 1: modo_vigilante_live(); break;
            case 2: exibir_painel_bancos_ssd(); break;
            case 3: 
                printf("\nDesligando Motores... Salve a firma do seu pai! Ate logo, Vitor.\n"); 
                Sleep(2000);
                break;
            default: 
                printf("\n" COR_VERMELHO "Comando Invalido." COR_RESET "\n"); 
                Sleep(1000);
        }
    }

    curl_global_cleanup();
    return 0;
}