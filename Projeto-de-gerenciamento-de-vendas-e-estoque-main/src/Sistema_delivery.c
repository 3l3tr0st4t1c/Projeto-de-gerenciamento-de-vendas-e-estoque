// ====================================================================================== //
//  Primeiro passo, declarar quais bibliotecas iremos usar ao longo do código             //
// ====================================================================================== //
#include <stdio.h>  // em resumo é para o printf e o scanf
#include <stdlib.h> // gerenciamento de memória com malloc e exite
#include <string.h> //
#include <curl/curl.h> //manipulador de strings como strcpy
#include <cjson/cJSON.h> // é a forma de traduzir em c os dados captados do ifood
#include <windows.h> // Permite funções que só existem no windows, como mudar cor do terminal e etc...
#include <time.h> // essencial para registrar a hora que um pedido saiu

void aceitar_pedido_ifood(char *id_do_pedido, char *token_autenticacao);

// Procure a lista de includes no topo do seu main.c e adicione esta linha:
#include "status_loja.h"

#include <conio.h> // Permite capturar o teclado sem travar o sistema (kbhit), o modo vigilante é um loop infinito enquanto o banco de dados é finito, no infinito
// criamos um loop sem parar, criando assimo "ao vivo", e atráves dessa biblioteca iremos conseguir criar uma para sem enterronper esse infinito.


const char* INTERFACE_WEB_CSS = 
    "body { font-family: 'Segoe UI', sans-serif; background: #121212; color: #e0e0e0; padding: 20px; }"
    ".nav-terminal { display: flex; justify-content: center; gap: 10px; margin-bottom: 20px; background: #000; padding: 10px; border-bottom: 2px solid #ffcc00; }"
    ".btn-aba { background: transparent; border: 1px solid #ffcc00; color: #ffcc00; padding: 10px 20px; cursor: pointer; font-family: monospace; font-weight: bold; }"
    ".btn-aba:hover, .btn-aba.active { background: #ffcc00; color: #000; }"
    ".pedido-card { background: #1e1e1e; border-radius: 10px; padding: 15px; margin-bottom: 10px; border-left: 8px solid #ffcc00; display: flex; justify-content: space-between; align-items: center; }"
    ".valor { font-weight: bold; color: #ffcc00; font-size: 1.2em; }"
    ".resumo-footer { margin-top: 30px; background: #ffcc00; color: black; padding: 20px; border-radius: 8px; font-weight: bold; }";


// ====================================================================================== //
//  Nomeando sistema, definindo seu intervalo de loop e tempo de preparo e atraso         //
// ====================================================================================== //
#define VERSAO "1.0.6" // Totalmente visual, apenas para dar um nome ao programa
#define INTERVALO_POLLING 1 // tempo em que o loop de verredura leva para reiniciar

// O tempo que a loja promete entregar o lanche na mão do motoboy
#define TEMPO_PREPARO_PADRAO 120 // 

// Quando o pedido passar de 25 minutos na tela, ele vira um problema.
// A tolerância máxima antes de ficar VERMELHO na sua tela:
#define TOLERANCIA_ATRASO_COZINHA 60 // 

// O tempo máximo que o motoboy tem para ir e voltar sem a comida esfriar:
#define LIMITE_TEMPO_ENTREGA 60 // 

#define SAFE_STRCPY(dest, src) strcpy(dest, (src && src->valuestring) ? src->valuestring : "N/A")





// ====================================================================================== //
//  Colorindo o sistema e manual                                                          //
// ====================================================================================== //
/* * 
 * MANUAL DE CORES ANSI - SISTEMA DE DELIVERY (VITOR RAPHAEL)
 * 
 * * ESTRUTURA DO COMANDO: \033[ [PLANO] ; 5 ; [ID] m
 * *                               1. INÍCIO (CSI): \033[ ou \x1B[ 
 * Avisa ao terminal: "Lá vem instrução de formatação!"
 * *                                2. PLANO (Onde a cor será aplicada):
 * 38 -> Frente (Foreground / Texto)
 * 48 -> Fundo  (Background)
 * *                                3. MODO DE COR:
 * ;5; -> Indica o uso da tabela de 256 cores (8-bit).
 * ID; -> DA COR (0 a 255):         4. Escolhe a cor:
 * 0-7:   Cores básicas do sistema.
 * 8-15:  Cores vibrantes (bright).
 * 16-231: Cubo RGB (onde a mágica acontece).
 * 232-255: Tons de cinza (grayscale).
 * * 5. FINALIZADOR:
 * m -> Fecha a sequência de comando.
 * 
 * EXEMPLOS PRÁTICOS:
 * 
 * Texto Verde:     "\033[38;5;46m"
 * Fundo Vermelho:  "\033[48;5;196m"
 * Reset Total:     "\033[0m"      (VOLTA AO PADRÃO - ESSENCIAL!)
 * COMBINAÇÃO (Frente e Fundo):
 * "\033[38;5;231;48;5;196m" -> Texto Branco(231) com Fundo Vermelho(196)
 * 
 */
#define COR_RESET   "\033[0m"       


// ====================================================================================== //
// DEFINIÇÃO VISUAL DA PÁGINA WEB (CSS)                                                  //
// ====================================================================================== //
/* * TEORIA: O CSS (Cascading Style Sheets) define a "roupa" do HTML. 
 * Usamos 'Flexbox' para organizar os cards e 'Media Queries' para que 
 * a página fique bonita tanto no PC quanto no celular.
 */
#define INTERFACE_WEB_CSS \
    "body { font-family: 'Segoe UI', sans-serif; background: #121212; color: #e0e0e0; padding: 20px; } " \
    ".container { max-width: 1000px; margin: auto; } " \
    "h1 { color: #ffcc00; text-align: center; border-bottom: 2px solid #333; padding-bottom: 10px; } " \
    ".pedido-card { background: #1e1e1e; border-radius: 10px; padding: 15px; margin-bottom: 15px; " \
    "               border-left: 8px solid #ffcc00; box-shadow: 0 4px 6px rgba(0,0,0,0.3); " \
    "               display: flex; justify-content: space-between; align-items: center; } " \
    ".status-aceito { border-left-color: #00ff44; } " \
    ".status-rota { border-left-color: #00ccff; } " \
    ".info-cliente { flex-grow: 1; margin-left: 20px; } " \
    ".valor { font-weight: bold; color: #ffcc00; font-size: 1.2em; } " \
    ".itens-resumo { font-size: 0.85em; color: #888; margin-top: 5px; }"

#define COR_VERDE   "\033[38;5;46m"
#define COR_AMARELO "\033[38;5;226m"
#define COR_VERMELHO "\033[38;5;196m"
#define COR_CIANO   "\033[38;5;51m"
#define COR_MAGENTA "\033[38;5;201m"
#define COR_AZUL    "\033[38;5;33m"
#define COR_BRANCO  "\033[38;5;231m"
#define COR_CINZA   "\033[38;5;240m"
#define FUNDO_AZUL  "\033[48;5;17m"
#define COR_PRETO     "\033[38;5;16m"  // Código 16 é o Preto Absoluto na tabela 256
#define FUNDO_AMARELO "\033[48;5;226m" // Código 226 é o mesmo amarelo forte, mas no fundo (48)










// ====================================================================================== //
//  Estrutura do pedido (ORGANIZADA POR DEPARTAMENTOS)                                    //
// ====================================================================================== //

// 1. Criamos os "Bancadas" de cada departamento primeiro
typedef struct {
    char nome[100];
    char idCliente[50];
    char telefone[20];
} BancoCliente;

typedef struct {
    double valorBruto;
    double taxaEntrega;
    char metodoPagamento[50];
} BancoFinanceiro;

typedef struct {
    char bairro[50];
    char logradouro[150];
    char numero[20];
    char complemento[100]; // NOVO
    char referencia[150];  // NOVO
} BancoEntrega;

typedef struct {
    int quantidadeTotalItens;
    char resumoItens[4000];
} BancoDNA;

// 2. Agora montamos a Super Struct Principal
// Note o nome 'PedidoAtivo' logo apos o 'struct' para permitir os ponteiros
typedef struct PedidoAtivo {
    char orderId[50];   // ID Longo da API
    char displayId[20]; // ID Curto (#1234)
    char status[30];
    
    time_t momentoEntrada;
    time_t momentoDespacho;

    // Acoplamos os bancos aqui
    BancoCliente cliente;
    BancoFinanceiro financeiro;
    BancoEntrega entrega;
    BancoDNA dna;

    // Os "Ganchos" da Lista Duplamente Encadeada
    struct PedidoAtivo *prox;
    struct PedidoAtivo *prev;
} PedidoAtivo;

void parse_detalhes_pedido(PedidoAtivo *novo, const char *json_string);





// ====================================================================================== //
//  Contador de pedidos  em preparo e entregue                                            //
// ====================================================================================== //
typedef struct {
    PedidoAtivo *inicio;  // O "Ponteiro Mestre": aponta para o 1º pedido da fila.
    int qtdCozinha;       // Quantos pedidos estão com status "Preparando".
    int qtdEntrega;       // Quantos pedidos estão com status "Preparando".
} ListaGestao;








// ====================================================================================== //
//  Recepcionista de Dados do ifood (MemoryStruct)                                        //
// ====================================================================================== //
struct MemoryStruct {
    char *memory;  // Um ponteiro que vai "esticando" para caber o texto (JSON) que chega.
    size_t size;   // O tamanho atual desse texto (para o C não se perder na memória).
};








// ====================================================================================== //
//  Moça da limpeza do programa                                                           //
// ====================================================================================== //
void inicializarSistema(ListaGestao *g) {
    g->inicio = NULL;
    g->qtdCozinha = 0;
    g->qtdEntrega = 0;
    printf(COR_VERDE "Sistema de Gestao Inicializado com Sucesso!\n" COR_RESET);
} // faz o programa não ter lixo ao iniciar e assi evitando erros 




// ====================================================================================== //
//  Variaveis de estado global                                                            //
// ====================================================================================== //
/* * g_listaRAM: O "Gerente" da memória. 
 * Inicializado com:
 * {NULL (sem pedidos), 0 (cozinha vazia), 0 (entrega vazia), 0.0 (faturamento zero)}
 */
ListaGestao g_listaRAM = {NULL, 0, 0}; 

/* * g_accessToken: A "Chave Mestra" digital.
 * Espaço de 2048 caracteres para armazenar o token de autorização das APIs (iFood/99).
 * Começa vazio ("") até o sistema realizar o primeiro login.
 */
char g_accessToken[2048] = "";

/* * g_proxima_renovacao: Cronômetro de segurança.
 * Armazena o timestamp (data/hora em segundos) de quando o token vai expirar.
 * Quando o tempo atual chegar aqui, o sistema renova a chave automaticamente.
 */
time_t g_proxima_renovacao = 0;

/* * g_ultimo_log: A "Caixa Preta" do sistema.
 * Guarda uma frase curta sobre a última ação realizada pelo programa.
 * Ideal para mostrar no rodapé da tela para o seu pai acompanhar o status.
 */
char g_ultimo_log[256] = "V7 Inicializado. Aguardando eventos...";








// ====================================================================================== //
//  Credenciais do ifood                                                                  //
// ====================================================================================== //
/* * CLIENT_ID: Identificador único do seu aplicativo no iFood.
 * É público dentro da sua organização, mas identifica QUE sistema está conectando.
 */
const char *CLIENT_ID = "0ed948ff-e60d-4bee-bd6f-f8bdc9856ab0";

/* * CLIENT_SECRET: A Chave Secreta (Senha).
 * ESTE DADO É SENSÍVEL. É usado para gerar o 'g_accessToken' que vimos antes.
 * Nunca compartilhe este código com pessoas fora do desenvolvimento, pois
 * ele dá acesso total à recepção de pedidos da loja.
 */
const char *CLIENT_SECRET = "c6ml41du8cormt3v55zqtqc70ybkg6ty6ibnbozpg0qphh7od80dwtr1uqdhwzhq2ugrxhxmj15jc9gs3hvauh3m25t7xa8myl4";









// ====================================================================================== //
// Mapa de comandos ( prototípos de funções )                                             //
// ====================================================================================== //
/* * 1. RECEPCIONISTA DE DADOS (CURL)
 * Esta função é usada pela biblioteca libcurl. Ela recebe os "pedaços" de dados 
 * da internet e os junta na MemoryStruct para formar o texto completo (JSON).
 * 'contents' é o dado que chegou, 'userp' é onde vamos guardar na RAM.
 */
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);

/* * 2. CAIXA PRETA (LOG)
 * Exibe mensagens de status na tela com o horário atualizado.
 * Serve para você saber exatamente o que o programa está fazendo a cada segundo.
 */
void registrar_log(const char *msg);


void atualizar_dashboard_web(void);

/* * 3. SEGURANÇA (TOKEN)
 * Checa o cronômetro (g_proxima_renovacao). Se o tempo expirou, ela usa o 
 * CLIENT_ID e SECRET para pedir uma nova chave de acesso ao iFood.
 */
void garantir_token_valido();

/* * 4. FAXINEIRA DE TEXTO (PARSER)
 * Remove caracteres especiais ou espaços inúteis que podem vir no JSON 
 * (como aspas extras ou quebras de linha) para não sujar seu banco de dados.
 */
void limpar_string(char *str);


  
/* * 6. CRIADORA DE VAGÕES (LISTA RAM)
 * Quando um ID de pedido novo aparece, ela cria o espaço na memória RAM, 
 * chama a tradutora e "pendura" esse novo pedido na sua lista encadeada.
 */






void buscar_detalhes_e_adicionar_ram(const char *orderId) {
    CURL *curl = curl_easy_init();
    struct MemoryStruct chunk = {malloc(1), 0};
    struct curl_slist *headers = NULL;
    char url[256], auth_h[2048];

    sprintf(url, "https://merchant-api.ifood.com.br/order/v1.0/orders/%s", orderId);
    
    // AQUI VEMOS O NOME REAL DA SUA VARIÁVEL DE TOKEN: g_accessToken
    sprintf(auth_h, "Authorization: Bearer %s", g_accessToken);
    headers = curl_slist_append(headers, auth_h);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    if (curl_easy_perform(curl) == CURLE_OK) {
        PedidoAtivo *novo = malloc(sizeof(PedidoAtivo));
        memset(novo, 0, sizeof(PedidoAtivo));
        
        strcpy(novo->orderId, orderId);
        strcpy(novo->status, "PREPARO");
        novo->momentoEntrada = time(NULL);

        parse_detalhes_pedido(novo, chunk.memory);

        novo->prox = g_listaRAM.inicio;
        novo->prev = NULL;
        if (g_listaRAM.inicio) g_listaRAM.inicio->prev = novo;
        g_listaRAM.inicio = novo;
        g_listaRAM.qtdCozinha++;

        // =================================================================
        // GATILHOS DE REDE E WEB (MÁGICA DO DATA CENTER)
        // =================================================================

        // 1. Atualiza o site na hora (Amarelo/PREPARO)
        atualizar_dashboard_web(); 

        // 2. Avisa o iFood que você assumiu o controle (Usando sua variável global)
        aceitar_pedido_ifood(novo->orderId, g_accessToken); 
        
        // 3. Muda para verde e atualiza o site novamente
        strcpy(novo->status, "ACEITO");
        atualizar_dashboard_web();
        
        // 4. Salva no Log que o pedido foi capturado com sucesso
        registrar_log("NOVO PEDIDO CAPTURADO E ACEITO!");
    } // FECHAMENTO CORRETO DO 'IF' (sem chaves sobrando agora)

    curl_easy_cleanup(curl);
    free(chunk.memory);
}






/* * 8. FECHAMENTO DE CONTA
 * Calcula o tempo total desde o início, aplica os descontos finais, 
 * gera o valor total a pagar e marca o status como "Finalizado".
 */
void finalizar_pedido(PedidoAtivo *p, const char *resultado);

/* * 9. ARQUIVISTA (SSD)
 * Pega o pedido finalizado e grava em um arquivo (.txt ou binário) no seu SSD.
 * Isso garante que mesmo se o PC desligar, o histórico do seu pai está salvo.
 */
void salvar_banco_ssd(PedidoAtivo *p, const char *resultado);

/* * 10. PROTOCOLO DE CONFIRMAÇÃO (ACK)
 * Avisa ao servidor do iFood: "Eu recebi esse pacote de eventos, pode limpar da fila".
 * Se você não enviar isso, o iFood te manda o mesmo pedido mil vezes.
 */
void enviar_ack(const char *json_eventos);

/* * 11. DESIGNER DE TÍTULO
 * Imprime aquele topo bonitão com fundo azul e letras brancas que definimos.
 */
void desenhar_cabecalho(const char *titulo);

/* * 12. PAINEL DE CONTROLE (LIVE)
 * Desenha na tela a tabela com os pedidos que estão na RAM agora, 
 * mostrando quem está na cozinha e o faturamento do dia.
 */
void desenhar_monitor_operacional_live();

/* * 13. CONSULTA DE HISTÓRICO
 * Abre o arquivo salvo no SSD e mostra os pedidos de dias anteriores na tela.
 */
void exibir_painel_bancos_ssd();

/* * 14. O MOTOR (VIGILANTE)
 * É o loop infinito do programa. Ele fica rodando, chamando a internet de 7 em 7 
 * segundos, atualizando a tela e mantendo o sistema vivo.
 */
void modo_vigilante_live();







// ====================================================================================== //
//  Porteiro de dados dos pedidos                                                         //
// ====================================================================================== //
// Esta função é chamada pelo cURL várias vezes conforme os dados chegam da internet
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    
    // 1. Calcula o tamanho real do "pedaço" de dado que acabou de chegar (tamanho do bloco * número de blocos)
    size_t realsize = size * nmemb;
    
    // 2. Converte o ponteiro genérico 'userp' de volta para a nossa estrutura de memória (MemoryStruct)
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    
    // 3. O PULO DO GATO: Usa o realloc para "esticar" o espaço na RAM. 
    // Ele pega o que já tínhamos (mem->size), soma o que chegou (realsize) e reserva +1 byte para o '\0' (fim da string).
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    
    // 4. Se o realloc falhar (falta de RAM), o programa retorna 0 e interrompe a transferência
    if (!ptr) return 0;
    
    // 5. Atualiza o ponteiro da nossa estrutura para o novo endereço de memória que foi esticado
    mem->memory = ptr;
    
    // 6. Copia os dados que chegaram (contents) para o final do nosso bloco de memória atual
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    
    // 7. Atualiza o tamanho total acumulado na nossa estrutura
    mem->size += realsize;
    
    // 8. Garante que o último caractere seja sempre o '0' (nulo), transformando o bloco em uma String válida no C
    mem->memory[mem->size] = 0;
    
    // 9. Informa ao cURL que processamos todos os bytes com sucesso
    return realsize;
}







// ====================================================================================== //
//  Registrador temporal dos eventos                                                      //
// ====================================================================================== //
void registrar_log(const char *msg) {
    // 1. Captura o tempo atual do sistema (segundos passados desde 01/01/1970)
    time_t agora = time(NULL);
    
    // 2. Converte esses segundos para uma estrutura que separa Horas, Minutos e Segundos
    struct tm *t = localtime(&agora);
    
    // 3. O CORAÇÃO DA FUNÇÃO: O sprintf escreve um texto formatado dentro da nossa variável global
    // [%02d:%02d:%02d] -> Garante que a hora sempre tenha 2 dígitos (ex: 09 em vez de 9)
    // %s -> Insere a mensagem que você enviou para a função
    sprintf(g_ultimo_log, "[%02d:%02d:%02d] %s", 
            t->tm_hour, t->tm_min, t->tm_sec, msg);
}








// ====================================================================================== //
//  Filtro de intregridade                                                                //
// ====================================================================================== //
void limpar_string(char *str) {
    // 1. Iniciamos um loop que percorre a string caractere por caractere
    // O loop para apenas quando encontra o '\0' (o sinalizador de fim de texto em C)
    for(int i = 0; str[i] != '\0'; i++) {
        
        // 2. Verificamos se o caractere atual é uma "sujeira":
        // '\n' (Nova linha), '\r' (Retorno de carro) ou '|' (Pipe separador)
        if(str[i] == '\n' || str[i] == '\r' || str[i] == '|') {
            
            // 3. Substituímos a "sujeira" por um espaço vazio ' '
            // Isso mantém o tamanho da string igual, mas remove o caractere perigoso
            str[i] = ' ';
        }
    }
}






// ====================================================================================== //
//  Chave de ignição do sistema                                                           //
// ====================================================================================== //
void garantir_token_valido() {
    time_t agora = time(NULL);
    
    // 1. VERIFICAÇÃO DE VALIDADE: Se já temos um token e ele ainda não expirou, 
    // a função encerra aqui (return) para não gastar internet desnecessariamente.
    if (strlen(g_accessToken) > 0 && agora < g_proxima_renovacao) return;

    // 2. PREPARAÇÃO DA REQUISIÇÃO: Inicializa o cURL e reserva 1 byte de memória 
    // inicial para receber a resposta (que vai "esticar" depois).
    CURL *curl = curl_easy_init();
    struct MemoryStruct chunk = {malloc(1), 0};
    char fields[1024];
    
    // 3. MONTAGEM DO PACOTE: Junta o ClientID e o Secret para enviar ao iFood.
    sprintf(fields, "grantType=client_credentials&clientId=%s&clientSecret=%s", CLIENT_ID, CLIENT_SECRET);

    // 4. CONFIGURAÇÃO DO "DISCAGEM": Define a URL de login do iFood e os dados que vamos mandar.
    curl_easy_setopt(curl, CURLOPT_URL, "https://merchant-api.ifood.com.br/authentication/v1.0/oauth/token");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, fields);
    
    // 5. CALLBACK: Avisa ao cURL para usar aquela função 'WriteMemoryCallback' que comentamos antes.
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    // 6. EXECUÇÃO: Tenta conectar. Se der certo (CURLE_OK)...
    if (curl_easy_perform(curl) == CURLE_OK) {
        // 7. TRADUÇÃO (JSON): Pega o texto que veio do iFood e transforma em um objeto cJSON.
        cJSON *json = cJSON_Parse(chunk.memory);
        if (json) {
            // 8. EXTRAÇÃO: Procura pela chave "accessToken" dentro do JSON.
            cJSON *token = cJSON_GetObjectItem(json, "accessToken");
            if (token) {
                // 9. ATUALIZAÇÃO GLOBAL: Copia o token para a nossa variável global e 
                // define a próxima renovação para daqui a 3500 segundos (quase 1 hora).
                strcpy(g_accessToken, token->valuestring);
                g_proxima_renovacao = agora + 3500;
                registrar_log("Token API Renovado com Sucesso. Conexao Segura.");
            }
            cJSON_Delete(json); // Limpa o objeto JSON da memória
        }
    } else {
        registrar_log("ERRO CRITICO: Falha na autenticacao com iFood.");
    }
    
    // 10. LIMPEZA: Libera a conexão cURL e a memória temporária usada.
    curl_easy_cleanup(curl);
    free(chunk.memory);
}





// ====================================================================================== //
//  Interpretador e tradutor do ifood para C                                              //
// ====================================================================================== //
void parse_detalhes_pedido(PedidoAtivo *novo, const char *json_string) {
    if (!json_string) return;
    cJSON *det = cJSON_Parse(json_string);
    if (!det) return;

    // 1. ID do Pedido
    cJSON *dId = cJSON_GetObjectItem(det, "displayId");
    SAFE_STRCPY(novo->displayId, dId);

    // 2. Dados do Cliente
    cJSON *customer = cJSON_GetObjectItem(det, "customer");
    if (customer) {
        SAFE_STRCPY(novo->cliente.nome, cJSON_GetObjectItem(customer, "name"));
        SAFE_STRCPY(novo->cliente.idCliente, cJSON_GetObjectItem(customer, "id"));
        cJSON *phoneObj = cJSON_GetObjectItem(customer, "phone");
        if (phoneObj) SAFE_STRCPY(novo->cliente.telefone, cJSON_GetObjectItem(phoneObj, "number"));
    }

    // 3. Financeiro (A parte que você mandou agora)
    cJSON *total = cJSON_GetObjectItem(det, "total");
    if (total) {
        cJSON *amt = cJSON_GetObjectItem(total, "orderAmount");
        cJSON *taxa = cJSON_GetObjectItem(total, "deliveryFee");
        novo->financeiro.valorBruto = amt ? amt->valuedouble : 0.0;
        novo->financeiro.taxaEntrega = taxa ? taxa->valuedouble : 0.0;
    }

    // 4. DNA do Pedido / Itens (A parte que você mandou agora)
    cJSON *items = cJSON_GetObjectItem(det, "items");
    novo->dna.quantidadeTotalItens = 0;
    strcpy(novo->dna.resumoItens, "");

    if (items && cJSON_IsArray(items)) {
        for (int i = 0; i < cJSON_GetArraySize(items); i++) {
            cJSON *item = cJSON_GetArrayItem(items, i);
            cJSON *q = cJSON_GetObjectItem(item, "quantity");
            cJSON *n = cJSON_GetObjectItem(item, "name");
            
            if (q && n && n->valuestring) { 
                novo->dna.quantidadeTotalItens += q->valueint; // Soma a qtd total
                char buf[128];
                sprintf(buf, "%dx %s, ", q->valueint, n->valuestring);
                if (strlen(novo->dna.resumoItens) + strlen(buf) < 1400) {
                    strcat(novo->dna.resumoItens, buf);
                }
            }
        }
    }

    // 5. Endereço (Importante para o BI do seu pai)
    cJSON *delivery = cJSON_GetObjectItem(det, "delivery");
    if (delivery) {
        cJSON *addr = cJSON_GetObjectItem(delivery, "deliveryAddress");
        if (addr) {
            cJSON *neigh = cJSON_GetObjectItem(addr, "neighborhood");
            SAFE_STRCPY(novo->entrega.bairro, neigh);
        }
    }

    // 6. Sanitização e Limpeza de Memória
    limpar_string(novo->cliente.nome);
    limpar_string(novo->dna.resumoItens);
    
    cJSON_Delete(det); // O ESCUDO ANTI-CRASH
}






// ====================================================================================== //
// Comunicador Universal (Motor POST para API iFood)                                      //
// ====================================================================================== //
/* *
 * Esta função age como uma "Casca" (Wrapper) genérica para requisições HTTP POST.
 * Em vez de criar funções repetidas para Aceitar, Despachar e Cancelar, usamos o 
 * parâmetro 'acao' para injetar dinamicamente o endpoint desejado na URL.
 */
void enviar_comando_ifood(const char *orderId, const char *acao) {
    // 1. Inicializa o ponteiro de sessão do cURL (O motor de rede)
    CURL *curl = curl_easy_init();
    if (!curl) return; // Trava de segurança: Aborta se faltar memória para iniciar a rede

    char url[256], auth_h[2048];
    struct curl_slist *headers = NULL;

    // 2. ROTEAMENTO DINÂMICO (Endpoint Construction)
    // Concatena a base da API com o ID do pedido específico e a ação desejada.
    sprintf(url, "https://merchant-api.ifood.com.br/order/v1.0/orders/%s/%s", orderId, acao);
    
    // 3. INJEÇÃO DE CABEÇALHOS (HTTP Headers)
    // O protocolo OAuth2.0 exige o envio do Token no formato 'Bearer'.
    sprintf(auth_h, "Authorization: Bearer %s", g_accessToken);
    headers = curl_slist_append(headers, auth_h);
    // Informa ao servidor que o corpo da mensagem é um objeto JSON
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // 4. CONFIGURAÇÃO DA REQUISIÇÃO POST
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L); 
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{}"); 

    // 5. EXECUÇÃO SÍNCRONA E LOG DE AUDITORIA
    if (curl_easy_perform(curl) == CURLE_OK) {
        char msg[150];
        sprintf(msg, "Comando HTTP '%s' enviado com sucesso (Pedido: %s)", acao, orderId);
        registrar_log(msg); 
    } else {
        registrar_log("ERRO CRÍTICO: Falha ao enviar comando para a API do iFood.");
    }

    // 6. COLETA DE LIXO (Limpeza de memória)
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}





// ====================================================================================== //
// Orquestrador do ciclo de vida do pedido (Máquina de Estados)                           //
// ====================================================================================== //
/* *
 * Esta função atua como um 'Controller'. Ela recebe os eventos crus do Polling 
 * e decide como a Lista Encadeada na RAM deve ser atualizada.
 */
void processar_mudanca_estado(const char *orderId, const char *fullCode) {
    // CHECKPOINT DE ENTRADA
    printf(COR_CIANO "\n[MOTOR] Evento: %s para Pedido: %s\n" COR_RESET, fullCode, orderId);

    // 1. BUSCA LINEAR: Procura na RAM
    PedidoAtivo *atual = g_listaRAM.inicio;
    while (atual && strcmp(atual->orderId, orderId) != 0) {
        atual = atual->prox; 
    }

    /* PASSO 2: FILTRO DE ENTRADA (Novo Pedido) */
    if (strcmp(fullCode, "PLACED") == 0 && !atual) {
        printf(COR_AMARELO "[INFO] Capturando novos detalhes do iFood...\n" COR_RESET);
        
        // A. Aloca RAM e processa JSON
        buscar_detalhes_e_adicionar_ram(orderId);
        
        // B. Aceite automático
        enviar_comando_ifood(orderId, "confirm");
        printf(COR_VERDE "[SUCESSO] Pedido %s adicionado a RAM e aceito!\n" COR_RESET, orderId);
    } 
    
    /* PASSO 3: GESTÃO DE CICLO DE VIDA */
    else if (atual) {
        printf("[INFO] Atualizando estado de pedido existente...\n");

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
    
    // CHECKPOINT DE SAÍDA
    printf("[MOTOR] Processamento de estado concluido.\n");
}




// ====================================================================================== //
// Finalizador de ciclo de vida do objeto                                                 //
// ====================================================================================== //
void finalizar_pedido(PedidoAtivo *p, const char *resultado) {
    /* PASSO 1: PERSISTÊNCIA ETERNA
       Antes de apagar qualquer coisa, chamamos a função de arquivamento. 
       Isso garante que, mesmo que o pedido saia da tela, ele já está no seu banco de dados SSD.
    */
    salvar_banco_ssd(p, resultado);

    /* PASSO 2: ATUALIZAÇÃO DOS INDICADORES LÓGICOS
       Verificamos onde o pedido estava para dar "baixa" no contador correto.
       Se estava na rua, diminui 'qtdEntrega'. Se ainda estava na loja, diminui 'qtdCozinha'.
    */
    if (strcmp(p->status, "EM ROTA") == 0) {
        g_listaRAM.qtdEntrega--;
    } else {
        g_listaRAM.qtdCozinha--;
    }

    /* PASSO 3: CIRURGIA NA LISTA ENCADEADA (Desconexão)
       Aqui removemos o "vagão" do trem. Precisamos religar o vagão anterior ao próximo
       para que a corrente não se quebre.
    */
    
    // Se existir um pedido ANTES deste, o 'próximo' dele agora passa a ser o 'próximo' deste aqui.
    if (p->prev) {
        p->prev->prox = p->prox;
    } else {
        // Se NÃO existe um anterior, significa que este era o primeiro da lista.
        // Então, o novo início da lista passa a ser o segundo pedido.
        g_listaRAM.inicio = p->prox;
    }

    // Se existir um pedido DEPOIS deste, o 'anterior' dele agora passa a ser o 'anterior' deste aqui.
    if (p->prox) {
        p->prox->prev = p->prev;
    }

    /* PASSO 4: REGISTRO FINAL NO LOG
       Formatamos uma mensagem para confirmar que o ciclo de vida daquele ID terminou com sucesso.
    */
    char msg[100];
    sprintf(msg, "CICLO DE VIDA ENCERRADO: Pedido #%s (%s). Dados Salvos no SSD.", p->displayId, resultado);
    registrar_log(msg);
    
    /* PASSO 5: LIBERAÇÃO FÍSICA DE MEMÓRIA (O Fim do Memory Leak)
       O comando 'free' devolve os bytes que o 'malloc' pegou de volta para o Windows.
       Sem isso, seu programa "comeria" toda a memória RAM ao longo de horas de uso.
    */
    free(p); 
}





// ====================================================================================== //
// Confirmador de protocolo de recebimento                                                //
// ====================================================================================== //
void enviar_ack(const char *json_eventos) {
    /* PASSO 1: ANÁLISE DO LOTE RECEBIDO
       O iFood envia uma lista (array) de eventos. Primeiro, transformamos o texto 
       em um objeto cJSON para contar quantos eventos precisamos confirmar.
    */
    cJSON *root = cJSON_Parse(json_eventos);
    int n = cJSON_GetArraySize(root);
    
    // Segurança: Se não houver eventos na lista, limpamos a memória e saímos.
    if (n <= 0) { 
        cJSON_Delete(root); 
        return; 
    }
    
    /* PASSO 2: CONSTRUÇÃO DO PROTOCOLO DE RESPOSTA
       Criamos um novo array JSON (`ack_array`) que conterá apenas os IDs dos 
       eventos que processamos. É como devolver uma lista de "OK" para o iFood.
    */
    cJSON *ack_array = cJSON_CreateArray();
    for (int i = 0; i < n; i++) {
        cJSON *obj = cJSON_CreateObject();
        // Captura o "id" do evento original e adiciona ao nosso novo objeto de resposta
        cJSON_AddStringToObject(obj, "id", cJSON_GetObjectItem(cJSON_GetArrayItem(root, i), "id")->valuestring);
        cJSON_AddItemToArray(ack_array, obj);
    }
    
    /* PASSO 3: SERIALIZAÇÃO (Transformação para Texto)
       O cJSON_PrintUnformatted transforma o nosso objeto JSON em uma string compacta
       (sem espaços ou quebras de linha) para economizar banda de internet no envio.
    */
    char *payload = cJSON_PrintUnformatted(ack_array);
    
    /* PASSO 4: COMUNICAÇÃO COM O SERVIDOR (CURL)
       Preparamos a conexão HTTP POST para o endpoint de 'acknowledgment' do iFood.
    */
    CURL *curl = curl_easy_init();
    struct curl_slist *headers = NULL;
    char auth_h[2048];
    
    // Autenticação: Enviamos o Token que você já garantiu que é válido
    sprintf(auth_h, "Authorization: Bearer %s", g_accessToken);
    headers = curl_slist_append(headers, auth_h);
    // Informamos que o dado que estamos enviando é do tipo JSON
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    // Configurações Finais e Envio do "Recibo"
    curl_easy_setopt(curl, CURLOPT_URL, "https://merchant-api.ifood.com.br/order/v1.0/events/acknowledgment");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload); // Aqui enviamos a lista de IDs
    curl_easy_perform(curl); // Executa a "entrega" do recibo
    
    /* PASSO 5: LIMPEZA DE RASTROS (Memória RAM)
       Como bons programadores de Sistemas de Informação, liberamos toda a memória alocada.
       Isso evita que o programa "inche" no seu Acer Aspire após milhares de ACKs enviados.
    */
    curl_easy_cleanup(curl);
    cJSON_Delete(root);
    cJSON_Delete(ack_array); // Importante liberar o array criado
    free(payload);           // Libera a string gerada pelo PrintUnformatted
}








// ====================================================================================== //
// Padronizador de layout do topo                                                         //
// ====================================================================================== //
void desenhar_cabecalho(const char *titulo) {
    system("cls");
    printf(FUNDO_AMARELO COR_PRETO "================================================================================\n");
    printf(" ||         VITOR DELIVERY         || SGA v%s || %s \n", VERSAO, titulo);
    printf("================================================================================\n" COR_RESET);
}







// ====================================================================================== //
// MOTOR DE ANALYTICS - FILTROS POR PERÍODO                                               //
// ====================================================================================== //
void exibir_dashboard_vendas(int dias) {
    FILE *arq = fopen("historico_v7.txt", "r");
    if (!arq) {
        printf("\n [!] Erro: Banco de dados nao encontrado.\n");
        return;
    }

    char linha[8192];
    double faturamento = 0;
    int pedidos_concluidos = 0;
    int pedidos_cancelados = 0;
    time_t agora = time(NULL);
    long limite_segundos = (long)dias * 86400; // 86400 segundos = 1 dia

    desenhar_cabecalho("RELATORIO DE PERFORMANCE FINANCEIRA");
    printf(COR_BRANCO " Analisando os ultimos %d dias...\n\n" COR_RESET, dias);

    while (fgets(linha, sizeof(linha), arq)) {
        // Usamos uma técnica de contagem de separadores para achar as colunas
        char *token;
        char *colunas[16];
        int i = 0;
        
        // Copiamos a linha porque o strtok modifica a string original
        char copia_linha[8192];
        strcpy(copia_linha, linha);

        char *ptr = copia_linha;
        while ((token = strstr(ptr, "||")) && i < 15) {
            *token = '\0';
            colunas[i++] = ptr;
            ptr = token + 2;
        }
        colunas[i] = ptr; // Última coluna (Itens)

        // Captura o Timestamp (Coluna 12 - índice começa em 0) e Status (Coluna 2)
        long timestamp_pedido = atol(colunas[12]);
        char *status = colunas[2];

        // LOGICA DO FILTRO: Se estiver dentro do prazo
        if (difftime(agora, (time_t)timestamp_pedido) <= (double)limite_segundos) {
            if (strcmp(status, "CONCLUDED") == 0) {
                faturamento += atof(colunas[5]);
                pedidos_concluidos++;
            } else if (strcmp(status, "CANCELLED") == 0) {
                pedidos_cancelados++;
            }
        }
    }

    // Exibição do Resultado
    printf(COR_VERDE " [+] Faturamento Total: R$ %.2f\n" COR_RESET, faturamento);
    printf(" [+] Pedidos Entregues: %d\n", pedidos_concluidos);
    printf(COR_VERMELHO " [-] Pedidos Cancelados: %d\n" COR_RESET, pedidos_cancelados);
    
    if (pedidos_concluidos > 0) {
        printf(COR_CIANO " [*] Ticket Medio: R$ %.2f\n" COR_RESET, faturamento / pedidos_concluidos);
    }

    fclose(arq);
    printf("\n Pressione qualquer tecla para voltar...");
    _getch();
}





// ====================================================================================== //
// SANITIZADOR DE STRINGS - Protege a integridade do Banco de Dados                       //
// ====================================================================================== //
/* * TEORIA: Em bancos de dados baseados em arquivos de texto (Flat-files), 
 * caracteres de controle como \n (quebra de linha) ou \r (retorno) são fatais.
 * Esta função percorre a string e remove esses caracteres antes da gravação.
 */
void sanitizar_texto_para_banco(char *texto) {
    if (!texto) return;
    for (int i = 0; texto[i] != '\0'; i++) {
        // Se encontrar um "Enter" ou "Tab", troca por um espaço
        if (texto[i] == '\n' || texto[i] == '\r' || texto[i] == '\t') {
            texto[i] = ' '; 
        }
        // Opcional: Se o cliente usar o nosso separador "||" no comentário,
        // trocamos por "--" para não confundir o nosso leitor de colunas.
        if (texto[i] == '|' && texto[i+1] == '|') {
            texto[i] = '-';
            texto[i+1] = '-';
        }
    }
}





// ====================================================================================== //
// GRAVADOR SSD V5 - COM SUPORTE A INTELIGÊNCIA DE NEGÓCIOS (BI)                          //
// ====================================================================================== //
void salvar_banco_ssd(PedidoAtivo *p, const char *resultado) {
    
    // 1. FILTRO DE INTEGRIDADE: Evita gravar o mesmo pedido duas vezes no SSD
    FILE *leitura = fopen("historico_v7.txt", "r");
    if (leitura) {
        char buffer_busca[4096];
        while (fgets(buffer_busca, sizeof(buffer_busca), leitura)) {
            if (strstr(buffer_busca, p->orderId) != NULL) {
                fclose(leitura);
                return; // Pedido já existe, aborta gravação
            }
        }
        fclose(leitura);
    }

    // 2. ABERTURA PARA ESCRITA
    FILE *arq = fopen("historico_v7.txt", "a");
    if (!arq) return;

    // Prepara a data legível para humanos
    time_t agora = time(NULL);
    char *data_humanizada = ctime(&agora);
    data_humanizada[strcspn(data_humanizada, "\n")] = 0; 

    // Calcula performance (Diferença entre entrada e despacho)
    double minutos_preparo = 0;
    if (p->momentoDespacho > 0) {
        minutos_preparo = difftime(p->momentoDespacho, p->momentoEntrada) / 60;
    }

    /* 3. SERIALIZAÇÃO (O novo Layout de Colunas)
     * Coluna 13: Timestamp (Segundos puros para o filtro)
     * Coluna 14: Minutos de Preparo
     * Coluna 15: Itens (DNA)
     */
    fprintf(arq, "%s||%s||%s||%s||%s||%.2f||%.2f||%s||%s||%s||%s||%d||%ld||%.1f||%s\n", 
            p->displayId,
            p->orderId,
            resultado,
            data_humanizada,
            p->cliente.nome,
            p->financeiro.valorBruto,
            p->financeiro.taxaEntrega,
            p->financeiro.metodoPagamento,
            p->entrega.bairro,
            p->entrega.logradouro,
            p->entrega.numero,
            p->dna.quantidadeTotalItens,
            (long)p->momentoEntrada, // SALVAMOS O NÚMERO PURO DO TEMPO AQUI
            minutos_preparo,
            p->dna.resumoItens);

    fclose(arq);
    registrar_log("SSD: Dados persistidos com sucesso (V8).");
}







// ====================================================================================== //
// Modo Vigilante (Front End)                                                             //
// ====================================================================================== //
/* *
 * Esta função desenha a tabela. Agora ela possui um 'index' (contador).
 * Ele enumera as linhas de 1 até 9. Acima de 9, ele desenha apenas [-] 
 * porque o teclado não tem um botão único para o número 10.
 */
void desenhar_monitor_operacional_live() {
    desenhar_cabecalho("DASHBOARD OPERACIONAL V7 - MODO INTERATIVO");
    
    PedidoAtivo *atual = g_listaRAM.inicio;

    // Nova coluna TECLA adicionada ao cabeçalho visual
    printf(COR_BRANCO " %-7s | %-6s | %-10s\n", "TECLA", "ID", "STATUS");
    printf(COR_CINZA "---------|--------|------------\n" COR_RESET);

    if (!atual) printf(COR_CINZA "\n   [ NENHUM PEDIDO NA COZINHA ]\n" COR_RESET);

    int hotkey_index = 1; // Começamos a contar do botão 1

    while (atual) {
        // Se for de 1 a 9, pinta o botão de Amarelo para chamar atenção
        if (hotkey_index <= 9) {
            printf(COR_AMARELO "   [%d]   " COR_RESET "| %-6s | %-10s\n", 
                   hotkey_index, atual->displayId, atual->status);
        } else {
            // Se passar de 9 pedidos simultâneos, os atalhos não funcionam mais
            printf(COR_CINZA "   [-]   " COR_RESET "| %-6s | %-10s\n", 
                   atual->displayId, atual->status);
        }

        atual = atual->prox;
        hotkey_index++; // Pula para o próximo botão
    }
}

// ====================================================================================== //
// Aceitar pedido                                                                         //
// ====================================================================================== //
void aceitar_pedido_ifood(char *id_do_pedido, char *token_autenticacao) {
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    
    if(curl) {
        char url[256];
        // Endpoint oficial do iFood para confirmar o pedido (Muda o status para CONFIRMED)
        sprintf(url, "https://merchant-api.ifood.com.br/order/v1.0/orders/%s/confirm", id_do_pedido);

        struct curl_slist *headers = NULL;
        char auth_header[512];
        
        // Monta o cabeçalho com a sua chave de segurança
        sprintf(auth_header, "Authorization: Bearer %s", token_autenticacao);
        headers = curl_slist_append(headers, auth_header);
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        // É um POST vazio, o iFood só precisa que a gente chame a URL com o método correto
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");

        printf("\n[REDE] Enviando comando de ACEITE para o pedido #%s...\n", id_do_pedido);
        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            printf("[ERRO] Falha ao comunicar com o servidor iFood: %s\n", curl_easy_strerror(res));
        } else {
            printf("[SUCESSO] Pedido assumido pelo Data Center!\n");
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
}







// ====================================================================================== //
// MODO VIGILANTE INTERATIVO - BACK END                                                   //
// ====================================================================================== //
/* * TEORIA: Esta função implementa um Loop de Eventos (Event Loop). 
 * Ela coordena três frentes: Rede (API), Memória (RAM) e Usuário (Teclado).
 */
void modo_vigilante_live() {
    system("cls");
    registrar_log("MOTOR V7: Iniciando modo de segurança.");
    printf(COR_VERDE "[SISTEMA] Modo Vigilante Ativo. Monitorando iFood...\n" COR_RESET);

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
                if (n > 0) {
                    printf("\n[REDE] %d novos eventos detectados.\n", n);
                    for (int i = 0; i < n; i++) {
                        cJSON *ev = cJSON_GetArrayItem(eventos, i);
                        cJSON *oId = cJSON_GetObjectItem(ev, "orderId");
                        cJSON *fCode = cJSON_GetObjectItem(ev, "fullCode");

                        if (oId && fCode) {
                            // EXECUTA A MUDANÇA
                            // COMENTE ASSIM PARA TESTAR:
/* processar_mudanca_estado(oId->valuestring, fCode->valuestring); */
printf("[TESTE] Detectado Pedido ID: %s | Evento: %s\n", oId->valuestring, fCode->valuestring);
                        }
                    }
                    // CRÍTICO: Confirmar recebimento para o iFood não travar sua fila
                    enviar_ack(poll_chunk.memory);
                    
                    // ATUALIZAÇÃO DE INTERFACE
                    atualizar_dashboard_web();
                    desenhar_monitor_operacional_live();
                }
                cJSON_Delete(eventos);
            }
        } else {
            registrar_log("ERRO DE CONEXÃO: Verifique a internet.");
        }

        curl_easy_cleanup(curl_poll);
        free(poll_chunk.memory);

        // ESCUTA TECLADO
        if (_kbhit()) {
            char tecla = _getch();
            if (tecla == 13 || tecla == 27) return; // Sair
            
            // Atalhos 1-9 para despachar
            if (tecla >= '1' && tecla <= '9') {
                int idx = tecla - '0';
                PedidoAtivo *alvo = g_listaRAM.inicio;
                for(int k=1; alvo && k < idx; k++) alvo = alvo->prox;
                if(alvo) {
                    enviar_comando_ifood(alvo->orderId, "dispatch");
                    strcpy(alvo->status, "EM ROTA");
                    atualizar_dashboard_web();
                    desenhar_monitor_operacional_live();
                }
            }
        }
        
        // Sleep estratégico para não "fritar" o processador
        Sleep(2000); 
    }
}






// ====================================================================================== //
// GERADOR DE INTERFACE WEB (C para HTML)                                                //
// ====================================================================================== //
void atualizar_dashboard_web() {
    FILE *html = fopen("vitor_delivery_monitor.html", "w");
    if (!html) return;

    fprintf(html, "<!DOCTYPE html><html lang='pt-br'><head><meta charset='UTF-8'>");
    fprintf(html, "<meta http-equiv='refresh' content='2'>"); 
    fprintf(html, "<style>");
    fprintf(html, "@import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;600;800&display=swap');");
    fprintf(html, "body { font-family: 'Inter', sans-serif; background: transparent; color: #fff; margin: 0; padding: 20px; }");
    fprintf(html, ".pedido-card { background: rgba(255, 255, 255, 0.03); backdrop-filter: blur(10px); border: 1px solid rgba(255, 255, 255, 0.05); border-radius: 12px; padding: 20px; margin-bottom: 15px; display: flex; justify-content: space-between; align-items: center; border-left: 5px solid #ffcc00; }");
    fprintf(html, ".status-aceito { border-left-color: #00ff44; }");
    fprintf(html, "</style></head><body>");

    PedidoAtivo *atual = g_listaRAM.inicio;
    
    if (!atual) {
        fprintf(html, "<div style='text-align:center; margin-top:100px; opacity:0.5;'><h2>Nenhum pedido activo.</h2><p>O motor de busca está ligado.</p></div>");
    } else {
        while (atual) {
            fprintf(html, "<div class='pedido-card'>");
            fprintf(html, "  <div><span style='color:#ffcc00; font-weight:800;'>#%s</span>", atual->displayId);
            fprintf(html, "  <h2 style='margin:5px 0;'>%s</h2>", atual->cliente.nome);
            fprintf(html, "  <p style='color:#94a3b8;'>🛒 %s</p></div>", atual->dna.resumoItens);
            fprintf(html, "  <div style='color:#10b981; font-size:1.8em; font-weight:800;'>R$ %.2f</div>", atual->financeiro.valorBruto);
            fprintf(html, "</div>");
            atual = atual->prox; 
        }
    }
    fprintf(html, "</body></html>"); 
    fclose(html); 
}
  
// ====================================================================================== //
// Diretor de operações ( Função MAIN)                                                    //
// ====================================================================================== //

void exibir_relatorio_gerencial(int diasFiltro) {
    FILE *html = fopen("vitor_delivery_relatorio.html", "w");
    if (!html) return;

    // 1. CABEÇALHO HTML E CSS
    fprintf(html, "<!DOCTYPE html><html lang='pt-br'><head><meta charset='UTF-8'>");
    fprintf(html, "<title>Relatório Gerencial</title>");
    fprintf(html, "<style>");
    fprintf(html, "body { font-family: 'Segoe UI', sans-serif; background: transparent; color: #fff; margin: 0; padding: 20px; }");
    fprintf(html, ".filtro-bar { background: #121212; border: 1px solid #333; border-left: 5px solid #ffcc00; padding: 15px 20px; border-radius: 8px; display: flex; gap: 20px; align-items: center; margin-bottom: 25px; box-shadow: 0 4px 10px rgba(0,0,0,0.5); }");
    fprintf(html, ".input-data { background: #000; color: #fff; border: 1px solid #444; padding: 8px; border-radius: 4px; font-family: monospace; color-scheme: dark; }");
    fprintf(html, ".btn-filtrar { background: #ffcc00; color: #000; border: none; padding: 8px 20px; border-radius: 4px; font-weight: bold; cursor: pointer; transition: 0.2s; }");
    fprintf(html, ".btn-filtrar:hover { background: #e6b800; transform: scale(1.05); }");
    fprintf(html, ".card-pedido { background: #1a1a1a; padding: 15px 20px; margin-bottom: 10px; border-radius: 6px; display: flex; justify-content: space-between; align-items: center; border-left: 4px solid #4caf50; transition: 0.2s; }");
    fprintf(html, ".card-pedido:hover { background: #222; transform: translateX(5px); }");
    fprintf(html, ".id-destaque { color: #ffcc00; font-weight: bold; font-family: monospace; }");
    fprintf(html, ".valor-destaque { color: #4caf50; font-size: 1.4em; font-weight: bold; }");
    fprintf(html, "</style></head><body>");

    // 2. INTERFACE VISUAL (BARRA DE FILTRO)
    fprintf(html, "<div class='filtro-bar'>");
    fprintf(html, "  <div style='color:#ffcc00; font-weight:bold; font-size:1.2em;'>📅 PERÍODO:</div>");
    fprintf(html, "  <label>Início:</label> <input type='date' id='dtInicio' class='input-data'>");
    fprintf(html, "  <label>Fim:</label> <input type='date' id='dtFim' class='input-data'>");
    fprintf(html, "  <button class='btn-filtrar' onclick='renderizarPainel()'>FILTRAR DATA</button>");
    fprintf(html, "  <div style='margin-left:auto; text-align:right;'>");
    fprintf(html, "    <div style='font-size:0.8em; color:#888;'>FATURAMENTO DA TELA</div>");
    fprintf(html, "    <div id='totalFaturamento' style='font-size:1.8em; color:#4caf50; font-weight:bold;'>R$ 0.00</div>");
    fprintf(html, "  </div>");
    fprintf(html, "</div>");

    fprintf(html, "<div id='gridPedidos'></div>");

    // ==========================================================
    // 3. A TAG SCRIPT QUE FALTOU E O BANCO DE DADOS JS
    fprintf(html, "<script>\n");
    fprintf(html, "const bancoDeDadosWeb = [\n");

    FILE *txt = fopen("historico_v7.txt", "r");
    if (txt) {
        char linha[2048];
        while (fgets(linha, sizeof(linha), txt)) {
            linha[strcspn(linha, "\n")] = 0; 
            
            char *campos[25];
            int col = 0;
            char *pt = linha;
            
            campos[col++] = pt;
            while ((pt = strstr(pt, "||")) != NULL) {
                *pt = '\0'; 
                pt += 2;    
                campos[col++] = pt;
                if (col >= 25) break; 
            }
            
            if (col >= 6 && strcmp(campos[2], "CONCLUDED") == 0) {
                float valorFormatado = atof(campos[5]);
                // O C escreve cada linha no formato JSON perfeito
                fprintf(html, "  { id: '%s', cliente: '%s', valor: %.2f, dataRaw: '%s' },\n", 
                        campos[1], campos[4], valorFormatado, campos[3]);
            }
        }
        fclose(txt);
    }
    fprintf(html, "];\n\n");

    // 4. O MOTOR JAVASCRIPT (QUE MONTA TUDO SOZINHO)
    fprintf(html, "function renderizarPainel() {\n");
    fprintf(html, "  const dtInicio = document.getElementById('dtInicio').value;\n");
    fprintf(html, "  const dtFim = document.getElementById('dtFim').value;\n");
    fprintf(html, "  const grid = document.getElementById('gridPedidos');\n");
    fprintf(html, "  const txtTotal = document.getElementById('totalFaturamento');\n");
    fprintf(html, "  \n");
    fprintf(html, "  grid.innerHTML = '';\n"); 
    fprintf(html, "  let somaTotal = 0;\n");
    fprintf(html, "  let contagem = 0;\n");
    fprintf(html, "  \n");
    
    // Converte e Filtra
    fprintf(html, "  bancoDeDadosWeb.forEach(pedido => {\n");
    fprintf(html, "    let d = new Date(pedido.dataRaw);\n");
    fprintf(html, "    let ano = d.getFullYear();\n");
    fprintf(html, "    let mes = String(d.getMonth() + 1).padStart(2, '0');\n");
    fprintf(html, "    let dia = String(d.getDate()).padStart(2, '0');\n");
    fprintf(html, "    let dataFormatada = `${ano}-${mes}-${dia}`;\n");
    fprintf(html, "    \n");
    
    // A LÓGICA QUE VOCÊ PEDIU: Começa mostrando TUDO (mostrar = true)
    fprintf(html, "    let mostrar = true;\n");
    // Só oculta se o usuário TIVER digitado algo nos calendários
    fprintf(html, "    if (dtInicio && dataFormatada < dtInicio) mostrar = false;\n");
    fprintf(html, "    if (dtFim && dataFormatada > dtFim) mostrar = false;\n");
    fprintf(html, "    \n");
    
    // Se passou no filtro, desenha o HTML na tela
    fprintf(html, "    if (mostrar) {\n");
    fprintf(html, "      somaTotal += pedido.valor;\n");
    fprintf(html, "      contagem++;\n");
    fprintf(html, "      grid.innerHTML += `\n");
    fprintf(html, "        <div class='card-pedido'>\n");
    fprintf(html, "          <div>\n");
    fprintf(html, "            <span class='id-destaque'>#${pedido.id}</span> - <span style='color:#ddd;'>${pedido.cliente}</span>\n");
    fprintf(html, "            <div style='color:#666; font-size:0.85em; margin-top:5px;'>📅 Operado em: ${d.toLocaleString('pt-BR')}</div>\n");
    fprintf(html, "          </div>\n");
    fprintf(html, "          <div class='valor-destaque'>R$ ${pedido.valor.toFixed(2)}</div>\n");
    fprintf(html, "        </div>\n");
    fprintf(html, "      `;\n");
    fprintf(html, "    }\n");
    fprintf(html, "  });\n");
    fprintf(html, "  \n");
    // Atualiza o valor lá no topo
    fprintf(html, "  txtTotal.innerHTML = 'R$ ' + somaTotal.toFixed(2).replace('.', ',');\n");
    fprintf(html, "  if (contagem === 0) grid.innerHTML = '<div style=\"text-align:center; padding:50px; color:#555;\">Nenhum pedido encontrado neste período.</div>';\n");
    fprintf(html, "}\n");
    
    // MÁGICA FINAL: Roda a função sozinho assim que abre, para carregar TODOS os dados!
    fprintf(html, "window.onload = renderizarPainel;\n");
    
    // FECHAMENTO DA TAG SCRIPT E HTML
    fprintf(html, "</script></body></html>");
    // ==========================================================
    
    fclose(html);
}

void gerar_index_principal() {
    FILE *html = fopen("index_vitor_delivery.html", "w");
    if (!html) return;

    fprintf(html, "<!DOCTYPE html><html lang='pt-br'><head><meta charset='UTF-8'>");
    fprintf(html, "<title>Vitor Delivery | Data Center Premium</title>");
    fprintf(html, "<style>");
    fprintf(html, "@import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600;800&display=swap');\n");
    fprintf(html, "* { margin: 0; padding: 0; box-sizing: border-box; font-family: 'Inter', sans-serif; }\n");
    fprintf(html, "body { background: #070b14; color: #e2e8f0; height: 100vh; overflow: hidden; display: flex; flex-direction: column; }\n");
    fprintf(html, "body::before { content: ''; position: absolute; top: -50%%; left: -50%%; width: 200%%; height: 200%%; background: radial-gradient(circle, rgba(255,204,0,0.05) 0%%, rgba(0,0,0,0) 40%%), radial-gradient(circle, rgba(0,255,100,0.03) 0%%, rgba(0,0,0,0) 40%%); animation: rotateBg 40s linear infinite; z-index: -1; }\n");
    fprintf(html, "@keyframes rotateBg { 0%% { transform: rotate(0deg); } 100%% { transform: rotate(360deg); } }\n");
    fprintf(html, ".navbar { margin: 20px; padding: 15px 30px; background: rgba(255, 255, 255, 0.02); border: 1px solid rgba(255, 255, 255, 0.05); border-radius: 16px; backdrop-filter: blur(15px); display: flex; justify-content: space-between; align-items: center; box-shadow: 0 10px 30px rgba(0,0,0,0.5); z-index: 10; }\n");
    fprintf(html, ".brand { display: flex; align-items: center; gap: 12px; }\n");
    fprintf(html, ".brand-icon { width: 38px; height: 38px; background: linear-gradient(135deg, #ffcc00, #ff8800); border-radius: 10px; display: flex; align-items: center; justify-content: center; color: #000; font-weight: 900; font-size: 1.2rem; }\n");
    fprintf(html, ".brand-text { font-size: 1.3rem; font-weight: 800; background: linear-gradient(to right, #fff, #94a3b8); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }\n");
    fprintf(html, ".controls { display: flex; gap: 15px; }\n");
    fprintf(html, ".btn { background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.08); color: #94a3b8; padding: 10px 22px; border-radius: 8px; font-weight: 600; cursor: pointer; transition: all 0.3s; }\n");
    fprintf(html, ".btn:hover { background: rgba(255,255,255,0.08); color: #fff; transform: translateY(-2px); }\n");
    fprintf(html, ".btn.active { background: rgba(255,204,0,0.1); color: #ffcc00; border-color: rgba(255,204,0,0.3); }\n");
    fprintf(html, ".btn-danger { background: rgba(234,29,44,0.1); color: #ea1d2c; border-color: rgba(234,29,44,0.3); }\n");
    fprintf(html, ".main-area { flex: 1; margin: 0 20px 20px 20px; background: rgba(0,0,0,0.3); border-radius: 16px; overflow: hidden; position: relative; }\n");
    fprintf(html, "iframe { width: 100%%; height: 100%%; border: none; display: none; }\n");
    fprintf(html, ".hero { position: absolute; inset: 0; display: flex; flex-direction: column; justify-content: center; align-items: center; text-align: center; }\n");
    fprintf(html, ".hero h1 { font-size: 3.5rem; font-weight: 800; color: #fff; }\n");
    fprintf(html, "</style></head><body>");

    fprintf(html, "<nav class='navbar'>");
    fprintf(html, "  <div class='brand'><div class='brand-icon'>V</div><div class='brand-text'>VITOR DELIVERY CORE</div></div>");
    fprintf(html, "  <div class='controls'>");
    fprintf(html, "    <button id='btn1' class='btn' onclick=\"abrirAba('vitor_delivery_monitor.html', 1)\">📡 GESTOR DE PEDIDOS</button>");
    fprintf(html, "    <button id='btn2' class='btn' onclick=\"abrirAba('vitor_delivery_relatorio.html', 2)\">📊 RELATÓRIOS</button>");
    fprintf(html, "    <button id='btn3' class='btn btn-danger' onclick=\"window.open('https://www.ifood.com.br/delivery/bujari-ac/teste---vitor-raphael-petri-bujari/3d1dd524-cb3d-42fc-b1f8-c6f57d116eae', '_blank')\">🛵 LOJA DE TESTE</button>");
    fprintf(html, "  </div>");
    fprintf(html, "</nav>");

    fprintf(html, "<main class='main-area'>");
    fprintf(html, "  <div id='hero' class='hero'><h1>SISTEMA ONLINE</h1><p>Aguardando comando operacional.</p></div>");
    fprintf(html, "  <iframe id='frameCentral' src='about:blank'></iframe>");
    fprintf(html, "</main>");

    fprintf(html, "<script>");
    fprintf(html, "function abrirAba(url, btnId) {");
    fprintf(html, "  document.querySelectorAll('.btn').forEach(b => b.classList.remove('active'));");
    fprintf(html, "  document.getElementById('btn' + btnId).classList.add('active');");
    fprintf(html, "  document.getElementById('hero').style.display = 'none';");
    fprintf(html, "  let frame = document.getElementById('frameCentral');");
    fprintf(html, "  frame.style.display = 'block';");
    fprintf(html, "  frame.src = url;");
    fprintf(html, "}");
    fprintf(html, "</script></body></html>");
    
    fclose(html);
}

int main(void) {
    // 1. CONFIGURAÇÃO ÚNICA DO AMBIENTE (UTF-8 e Tamanho da Tela)
    system("mode con: cols=160 lines=40"); 
    SetConsoleOutputCP(65001); 
    curl_global_init(CURL_GLOBAL_ALL);

    // 2. SINCRONIZAÇÃO AUTOMÁTICA (O MÁGICO STARTUP)
    // Isso garante que, ao abrir o programa, o site já tenha os dados do SSD
    printf("[SISTEMA] Sincronizando Banco de Dados com a Web...\n");
    
    gerar_index_principal();       // Cria a carcaça e o menu do site
    atualizar_dashboard_web();     // Cria o monitor operacional (Live) vazio/zerado
    
    // Força o C a ler o historico_v7.txt e gerar o relatório do Mês (4) antes mesmo do menu aparecer!
    exibir_relatorio_gerencial(4); 

    printf("[SISTEMA] Dashboard Web pronto! Abra o arquivo 'index_vitor_delivery.html'\n");
    
    // 3. INTEGRAÇÃO IFOOD / LOJA
    abrirLojaManual(); 

    int opcao = 0;

    // 4. MENU DE COMANDO PRINCIPAL
    while(opcao != 3) {
        desenhar_cabecalho("CENTRO DE COMANDO PRINCIPAL");
        
        printf(COR_AMARELO " [1]" COR_RESET " Gestor de Pedidos (Vigilante Live)\n");
        printf(COR_AMARELO " [2]" COR_RESET " Banco de Dados (Relatórios e Histórico)\n");
        printf(COR_AMARELO " [3]" COR_RESET " Sair do Sistema\n\n");
        
        printf(COR_AMARELO " AÇÃO DE COMANDO: " COR_RESET);
        
        if (scanf("%d", &opcao) != 1) {
            while(getchar() != '\n'); 
            continue;
        }

        switch(opcao) {
            case 1: 
                modo_vigilante_live(); 
                break;
            case 2:
                desenhar_cabecalho("MENU DE BANCO DE DADOS");
                printf(" [1] Ver Tudo (Histórico Bruto no Terminal)\n");
                printf(" [2] Relatório de Hoje (Web + Terminal)\n");
                printf(" [3] Relatório da Semana\n");
                printf(" [4] Relatório do Mês\n");
                printf("\n ESCOLHA: ");
                
                int sub;
                scanf("%d", &sub);
                
                if (sub == 1) exibir_painel_bancos_ssd();
                if (sub == 2) exibir_relatorio_gerencial(1);
                if (sub == 3) exibir_relatorio_gerencial(2);
                if (sub == 4) exibir_relatorio_gerencial(4);
                break;

            case 3: 
                printf("\nDesligando Motores... Salve a firma do seu pai! Até logo, Vitor.\n"); 
                Sleep(2000);
                break;

            default: 
                printf("\n" COR_VERMELHO "Comando Inválido." COR_RESET "\n"); 
                Sleep(1000);
        }
    }

    // 5. ENCERRAMENTO SEGURO
    curl_global_cleanup();
    return 0;
}


// ====================================================================================== //
// EXIBIDOR DE HISTÓRICO SSD - VERSÃO V8 (COMPATÍVEL COM BI)                              //
// ====================================================================================== //
/* TEORIA: Esta função realiza a Leitura de um Arquivo Flat-File (arquivo de texto plano).
 * Ela atua como o "Select *" do SQL, buscando todos os registros salvos no disco.
 */
void exibir_painel_bancos_ssd() {
    // 1. TENTATIVA DE ACESSO AO DISCO:
    // Abrimos em modo "r" (read/leitura). Se o arquivo não existir, o ponteiro será NULL.
    FILE *arq = fopen("historico_v7.txt", "r");
    if (!arq) {
        printf(COR_VERMELHO "\n [!] Erro: Nenhum registro encontrado no SSD.\n" COR_RESET);
        _getch(); // Pausa para o usuário ler a mensagem de erro
        return;
    }

    // 2. CONSTRUÇÃO DA INTERFACE (UX):
    // Limpamos a tela e desenhamos o cabeçalho padronizado.
    desenhar_cabecalho("HISTORICO GERAL DE VENDAS (ARQUIVO SSD)");
    
    // Definimos as colunas com larguras fixas (%-15s) para o alinhamento ficar perfeito no terminal.
    printf(COR_BRANCO " %-6s | %-15s | %-12s | %-10s | %-20s\n", "ID", "CLIENTE", "STATUS", "VALOR", "DATA/HORA");
    printf(COR_CINZA "-------|-----------------|--------------|------------|----------------------\n" COR_RESET);

    char linha[8192]; // Buffer de 8KB para suportar o DNA (resumoItens) sem estourar a memória.

    // 3. LOOP DE VARREDURA (O "Coração" da Leitura):
    // fgets lê o arquivo linha por linha até chegar no final (EOF).
    while (fgets(linha, sizeof(linha), arq)) {
        char copia[8192];
        strcpy(copia, linha); // Criamos uma cópia para não "sujar" a string original durante o corte.
        
        char *col[16]; // Array de ponteiros para armazenar nossas 15 colunas.
        int i = 0;

        /* TEORIA: TOKENIZAÇÃO (Corte de Strings)
         * O strtok divide a linha toda vez que encontra nosso separador "||".
         * Ele substitui o "||" por um "\0" temporário, permitindo isolar cada dado.
         */
        char *token = strtok(copia, "||");
        while (token && i < 15) {
            col[i++] = token;
            token = strtok(NULL, "||"); // Continua cortando da onde parou.
        }

        // 4. FILTRAGEM E FORMATAÇÃO VISUAL:
        // Verificamos se a linha tem colunas suficientes (i >= 5) para evitar erros de leitura.
        if (i >= 5) {
            // Lógica de Cores Condicional: Verde para Concluído, Vermelho para Cancelado.
            char *cor_status = strstr(col[2], "CONCLUDED") ? COR_VERDE : COR_VERMELHO;
            
            // Impressão Formatada:
            // col[0] = displayId | col[4] = Nome Cliente | col[2] = Status | col[5] = Valor | col[3] = Data
            printf(" %-6s | %-15.15s | %s%-12s" COR_RESET " | R$ %-7.2f | %-20s\n", 
                   col[0],      // ID do pedido
                   col[4],      // Nome do Cliente (limitado a 15 chars para não quebrar a tabela)
                   cor_status,  // Injeta a cor baseada no status
                   col[2],      // O texto do Status
                   atof(col[5]),// Converte o texto do valor para número real (double)
                   col[3]);     // Data humanizada
        }
    }

    // 5. ENCERRAMENTO DE SESSÃO:
    // SEMPRE feche o arquivo para liberar o recurso para o Windows.
    fclose(arq);
    printf(COR_CINZA "\n [ Pressione qualquer tecla para voltar ao menu ]\n" COR_RESET);
    _getch(); 
}
