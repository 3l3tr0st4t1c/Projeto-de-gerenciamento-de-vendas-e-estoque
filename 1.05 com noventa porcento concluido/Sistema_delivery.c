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

// Procure a lista de includes no topo do seu main.c e adicione esta linha:
#include "status_loja.h"

#include <conio.h> // Permite capturar o teclado sem travar o sistema (kbhit), o modo vigilante é um loop infinito enquanto o banco de dados é finito, no infinito
// criamos um loop sem parar, criando assimo "ao vivo", e atráves dessa biblioteca iremos conseguir criar uma para sem enterronper esse infinito.





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

/* * 5. TRADUTORA (JSON -> STRUCT)
 * Pega o texto bruto do iFood e "injeta" as informações nos campos 
 * da sua Super Struct PedidoAtivo (nome, bairro, valor, etc).
 */
void parse_detalhes_pedido(PedidoAtivo *novo, const char *json_string);

/* * 6. CRIADORA DE VAGÕES (LISTA RAM)
 * Quando um ID de pedido novo aparece, ela cria o espaço na memória RAM, 
 * chama a tradutora e "pendura" esse novo pedido na sua lista encadeada.
 */
void buscar_detalhes_e_adicionar_ram(const char *orderId);

/* * 7. MONITOR DE STATUS (IFood Events)
 * Percebe se o pedido mudou de status (ex: de 'PREPARANDO' para 'DESPACHADO') 
 * e decide o que o programa deve fazer em seguida.
 */
void processar_mudanca_estado(const char *orderId, const char *fullCode);

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
    /* PASSO 1: CONVERSÃO DE TEXTO PARA ESTRUTURA DE ÁRVORE
       O cJSON_Parse transforma a string longa que veio da internet em uma estrutura 
       que o C consegue navegar. Sem isso, o C veria apenas uma "sopa de letras".
    */
    cJSON *det = cJSON_Parse(json_string);
    if (!det) return; // Segurança: Se o iFood mandar um dado inválido, o programa não trava.

    /* PASSO 2: IDENTIFICAÇÃO ÚNICA (O RG do Pedido)
       O 'displayId' é o número que seu pai vê na tela (Ex: #1234). 
       O operador ternário ( ? : ) garante que se o campo não existir, o sistema não dê erro de ponteiro.
    */
    cJSON *dId = cJSON_GetObjectItem(det, "displayId");
    strcpy(novo->displayId, dId ? dId->valuestring : "0000");

    /* PASSO 3: MAPEAMENTO DO CLIENTE (Inteligência de CRM)
       Aqui capturamos quem está comprando. Guardar o 'id' do cliente é o que 
       permitirá, no futuro, você saber quantas vezes o "Vitor" comprou na loja.
    */
    cJSON *customer = cJSON_GetObjectItem(det, "customer");
    if (customer) {
        cJSON *cName = cJSON_GetObjectItem(customer, "name");
        cJSON *cId = cJSON_GetObjectItem(customer, "id");
        // O iFood coloca o telefone dentro de outro objeto 'phone'. 
        // Fazemos um "mergulho duplo" para chegar no número.
        cJSON *cPhone = cJSON_GetObjectItem(cJSON_GetObjectItem(customer, "phone"), "number");
        
        strcpy(novo->cliente.nome, cName ? cName->valuestring : "CLIENTE NAO IDENTIFICADO");
        strcpy(novo->cliente.idCliente, cId ? cId->valuestring : "ID_NULL");
        strcpy(novo->cliente.telefone, cPhone ? cPhone->valuestring : "SEM_NUMERO");
    }

    /* PASSO 4: MAPEAMENTO FINANCEIRO (Cálculo de Lucro)
       Capturamos o valor bruto e a taxa separadamente. Isso é vital para você 
       calcular quanto do faturamento vai para o entregador e quanto fica na loja.
    */
    cJSON *total = cJSON_GetObjectItem(det, "total");
    if (total) {
        cJSON *orderAmt = cJSON_GetObjectItem(total, "orderAmount"); // Valor dos produtos
        cJSON *delivFee = cJSON_GetObjectItem(total, "deliveryFee"); // Valor da entrega
        novo->financeiro.valorBruto = orderAmt ? orderAmt->valuedouble : 0.0;
        novo->financeiro.taxaEntrega = delivFee ? delivFee->valuedouble : 0.0;
    }

    /* PASSO 5: LOGÍSTICA DE PAGAMENTO
       Verificamos como o cliente pagou. Isso ajuda a prever se você precisa de 
       troco (Dinheiro) ou se o pagamento já caiu na conta (Cartão/iFood).
    */
    cJSON *payments = cJSON_GetObjectItem(det, "payments");
    if (payments) {
        cJSON *methods = cJSON_GetObjectItem(payments, "methods");
        if (methods && cJSON_IsArray(methods)) {
            // Pegamos o primeiro método de pagamento da lista (índice 0)
            cJSON *method0 = cJSON_GetArrayItem(methods, 0); 
            cJSON *type = cJSON_GetObjectItem(method0, "method");
            strcpy(novo->financeiro.metodoPagamento, type ? type->valuestring : "DESCONHECIDO");
        }
    }

    /* PASSO 6: GEOLOCALIZAÇÃO URBANA (Inteligência de Entrega)
       Guardar o bairro é o segredo para sua "IA" futura. Com o tempo, você 
       poderá dizer: "Pai, o bairro X faz 40% dos nossos pedidos na sexta".
    */
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

    /* PASSO 7: O DNA DO PEDIDO (Lista de Compras)
       Um pedido pode ter vários itens. Usamos um loop 'for' para percorrer a lista 
       de itens que o iFood mandou e montar uma frase resumida para o seu banco de dados.
    */
    cJSON *items = cJSON_GetObjectItem(det, "items");
    novo->dna.quantidadeTotalItens = 0; // Zeramos para começar a contagem
    strcpy(novo->dna.resumoItens, ""); // Limpamos a string de resumo
    
    if (items && cJSON_IsArray(items)) {
        int arrSize = cJSON_GetArraySize(items); // Descobre quantos itens vieram
        for (int i = 0; i < arrSize; i++) {
            cJSON *item = cJSON_GetArrayItem(items, i);
            cJSON *qtd = cJSON_GetObjectItem(item, "quantity");
            cJSON *nomeItem = cJSON_GetObjectItem(item, "name");
            
            if (qtd && nomeItem) {
                // Soma a quantidade ao total do banco DNA
                novo->dna.quantidadeTotalItens += qtd->valueint;
                
                char bufferItem[150];
                // Formata o item: "2x Hamburguer, "
                sprintf(bufferItem, "%dx %s, ", qtd->valueint, nomeItem->valuestring);
                
                // SEGURANÇA DE MEMÓRIA: Verifica se ainda há espaço na string (limite de 300)
                // antes de "colar" (strcat) o novo item.
                if (strlen(novo->dna.resumoItens) + strlen(bufferItem) < 1450) {
                    strcat(novo->dna.resumoItens, bufferItem);
                } else if (strstr(novo->dna.resumoItens, "...") == NULL) {
                    // Se estourar o espaço, coloca reticências para indicar corte
                    strcat(novo->dna.resumoItens, "..."); 
                }
            }
        }
    }
    
    /* PASSO 8: SANITIZAÇÃO FINAL
       Chamamos a função 'limpar_string' para garantir que não existam caracteres 
       como '|' ou 'Enter' que possam corromper o arquivo quando salvarmos no SSD.
    */
    limpar_string(novo->cliente.nome);
    limpar_string(novo->entrega.bairro);
    limpar_string(novo->entrega.logradouro);
    limpar_string(novo->dna.resumoItens);

    /* PASSO 9: LIBERAÇÃO DE RECURSOS (Anti-Travamento)
       IMPORTANTE: O cJSON aloca memória na RAM. Se você não usar o cJSON_Delete, 
       o programa vai "comer" toda a memória do seu Acer conforme os pedidos chegam.
    */
    cJSON_Delete(det);
}











// ====================================================================================== //
// Recepcionista do hotel (Memoria RAM)                                                   //
// ====================================================================================== //
void buscar_detalhes_e_adicionar_ram(const char *orderId) {
    /* PASSO 1: PREPARAÇÃO DA CONEXÃO DE ELITE
       Inicializamos o cURL e preparamos o 'chunk' (nosso balde elástico) para 
       receber os detalhes técnicos do pedido vindo da API.
    */
    CURL *curl = curl_easy_init();
    struct MemoryStruct chunk = {malloc(1), 0}; // Começa com 1 byte e cresce conforme necessário
    struct curl_slist *headers = NULL;
    char url[256], auth_h[2048];

    /* PASSO 2: ENDEREÇAMENTO E AUTENTICAÇÃO
       Montamos a URL específica do pedido usando o 'orderId' recebido.
       Injetamos o nosso 'g_accessToken' no cabeçalho (Header) para o iFood saber que temos permissão.
    */
    sprintf(url, "https://merchant-api.ifood.com.br/order/v1.0/orders/%s", orderId);
    sprintf(auth_h, "Authorization: Bearer %s", g_accessToken);
    headers = curl_slist_append(headers, auth_h);

    // Configurações do cURL para baixar os dados
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback); // Nossa função de "esticar" memória
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    /* PASSO 3: O GRANDE SALTO (EXECUÇÃO)
       Se a conexão for bem-sucedida (CURLE_OK), começamos a construção do objeto na RAM.
    */
    if (curl_easy_perform(curl) == CURLE_OK) {
        
        /* ALOCAÇÃO DINÂMICA: Aqui o sistema reserva um espaço permanente na RAM 
           para este pedido específico. O 'malloc' garante que os dados não sumam 
           quando a função terminar.
        */
        PedidoAtivo *novo = malloc(sizeof(PedidoAtivo));
        memset(novo, 0, sizeof(PedidoAtivo)); // Limpa o "lixo" de memória para evitar bugs
        
        // Preenchemos os dados básicos de controle interno
        strcpy(novo->orderId, orderId);
        strcpy(novo->status, "PREPARO");
        novo->momentoEntrada = time(NULL); // Carimba a hora que o pedido "nasceu" no seu sistema
        novo->momentoDespacho = 0;

        /* PASSO 4: O MOTOR DE EXTRAÇÃO
           Chamamos a função anterior (parse_detalhes_pedido) para traduzir o JSON 
           que está em 'chunk.memory' para dentro da nossa struct 'novo'.
        */
        parse_detalhes_pedido(novo, chunk.memory);

        /* PASSO 5: ENGENHARIA DE LISTA (O GANCHO DUPLO)
           Aqui inserimos o novo "vagão" no início do nosso "trem" (Lista Encadeada).
           Fazemos o novo apontar para o antigo início, e o antigo início apontar de volta para o novo.
        */
        novo->prox = g_listaRAM.inicio; // O próximo do novo é o que era o primeiro
        novo->prev = NULL;              // Como ele é o novo primeiro, não tem ninguém antes dele
        
        if (g_listaRAM.inicio) {
            g_listaRAM.inicio->prev = novo; // O antigo primeiro agora sabe que tem um novo "mestre" antes dele
        }
        g_listaRAM.inicio = novo; // O Gerente Global agora aponta para este novo pedido
        
        /* PASSO 6: ATUALIZAÇÃO DO DASHBOARD LIVE
           Incrementamos os contadores globais que seu pai verá na tela em tempo real.
        */
        g_listaRAM.qtdCozinha++;
        
        // REGISTRO DE SUCESSO: Cria uma mensagem amigável para o Log
        char msg[150];
        sprintf(msg, "NOVO PEDIDO CAPTURADO! Display #%s | Valor: R$%.2f", novo->displayId, novo->financeiro.valorBruto);
        registrar_log(msg);
    }

    /* PASSO 7: FAXINA TÉCNICA
       Limpamos a conexão e o balde temporário. Note que NÃO liberamos o 'novo', 
       pois ele precisa continuar vivo na lista RAM!
    */
    curl_easy_cleanup(curl);
    free(chunk.memory);
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
    // 1. BUSCA LINEAR: Percorre a memória RAM procurando se o pedido já existe
    PedidoAtivo *atual = g_listaRAM.inicio;
    while (atual && strcmp(atual->orderId, orderId) != 0) {
        atual = atual->prox; 
    }

    /* PASSO 2: FILTRO DE ENTRADA (O Nascimento e Aceite Automático) */
    if (strcmp(fullCode, "PLACED") == 0 && !atual) {
        // A. Aloca o espaço na memória e processa o JSON do pedido
        buscar_detalhes_e_adicionar_ram(orderId);
        
        // B. AUTOMAÇÃO DE PRODUÇÃO: Aceite instantâneo via API
        enviar_comando_ifood(orderId, "confirm");
    } 
    /* PASSO 3: GESTÃO DE CICLO DE VIDA (Mutação do Objeto) */
    else if (atual) {
        // Finalização: Arquivamento e limpeza de RAM
        if (strcmp(fullCode, "CONCLUDED") == 0 || strcmp(fullCode, "CANCELLED") == 0) {
            finalizar_pedido(atual, fullCode); 
        } 
        // Logística: Transição para o modo de entrega
        else if (strcmp(fullCode, "DISPATCHED") == 0) {
            strcpy(atual->status, "EM ROTA");
            atual->momentoDespacho = time(NULL); 
            g_listaRAM.qtdCozinha--;
            g_listaRAM.qtdEntrega++;
            registrar_log("MOTOBOY LIBERADO: Pedido saiu para entrega.");
        }
        // Confirmação: Registro visual de que o iFood processou o aceite
        else if (strcmp(fullCode, "CONFIRMED") == 0) {
            strcpy(atual->status, "ACEITO");
            registrar_log("Confirmacao automatica via API registrada.");
        }
    }
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
// MODO VIGILANTE INTERATIVO - BACK END                                                   //
// ====================================================================================== //
/* * TEORIA: Esta função implementa um Loop de Eventos (Event Loop). 
 * Ela coordena três frentes: Rede (API), Memória (RAM) e Usuário (Teclado).
 */
void modo_vigilante_live() {
    // LOG DE AUDITORIA: Registra o início do processo para depuração futura.
    registrar_log("Motor V7 engatado. Iniciando Polling iFood.");

    // WHILE(1): Cria um loop infinito. O sistema só para se houver um 'return' ou 'break'.
    while(1) {
        
        // SEGURANÇA: O token do iFood expira a cada 1 hora. Esta função checa o cronômetro
        // global e renova a chave antes de qualquer tentativa de conexão.
        garantir_token_valido();

        // 1. PREPARAÇÃO DA ESTRUTURA DE DADOS PARA RECEBIMENTO (Buffer)
        // MemoryStruct aloca 1 byte inicial que crescerá dinamicamente via realloc().
        struct MemoryStruct poll_chunk = {malloc(1), 0};
        CURL *curl_poll = curl_easy_init();
        struct curl_slist *headers = NULL;
        char auth_h[2048];

        // Montagem do Header de Autorização Bearer Token (Padrão OAuth 2.0)
        sprintf(auth_h, "Authorization: Bearer %s", g_accessToken);
        headers = curl_slist_append(headers, auth_h);

        // Configuração do Endpoint de Polling: Onde "perguntamos" se há novidades.
        curl_easy_setopt(curl_poll, CURLOPT_URL, "https://merchant-api.ifood.com.br/order/v1.0/events:polling");
        curl_easy_setopt(curl_poll, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl_poll, CURLOPT_WRITEFUNCTION, WriteMemoryCallback); // Função de "esticar" RAM
        curl_easy_setopt(curl_poll, CURLOPT_WRITEDATA, (void *)&poll_chunk);

        // 2. EXECUÇÃO DA CONSULTA (Networking)
        if (curl_easy_perform(curl_poll) == CURLE_OK) {
            
            // PARSING: O JSON recebido é convertido em uma estrutura de árvore na RAM.
            cJSON *eventos = cJSON_Parse(poll_chunk.memory);
            
            if (eventos && cJSON_IsArray(eventos)) {
                int n = cJSON_GetArraySize(eventos); 

                // ITERAÇÃO: Percorre a lista de eventos (ex: Novos pedidos, cancelamentos).
                for (int i = 0; i < n; i++) {
                    cJSON *ev = cJSON_GetArrayItem(eventos, i);
                    cJSON *oId = cJSON_GetObjectItem(ev, "orderId");
                    cJSON *fCode = cJSON_GetObjectItem(ev, "fullCode");

                    if (oId && fCode) {
                        // DISPACHO PARA O CÉREBRO: O orquestrador decide o que fazer.
                        // Se fCode == "PLACED", ele aceita o pedido automaticamente.
                        processar_mudanca_estado(oId->valuestring, fCode->valuestring);
                    }
                }

                // PROTOCOLO ACK: Se processamos eventos, enviamos o "recebido" (Acknowledgment).
                // Isso limpa a fila no servidor do iFood para não recebermos os mesmos dados de novo.
                if (n > 0) enviar_ack(poll_chunk.memory);
                cJSON_Delete(eventos); // Libera a memória da árvore JSON (Evita Memory Leak)
            }
        }

        // 3. LIMPEZA DA SESSÃO ATUAL
        // IMPORTANTE: Liberamos o curl e a memória do chunk ANTES de entrar na espera do teclado.
        curl_easy_cleanup(curl_poll);
        free(poll_chunk.memory);
        
        // 4. ATUALIZAÇÃO DA INTERFACE VISUAL (UX)
        // Redesenha a tabela com os pedidos que estão na RAM agora.
        desenhar_monitor_operacional_live();
        printf(COR_CINZA "\n [ '1'-'9': Despachar Linha | '0': Buscar ID | 'ESC' ou 'ENTER': Menu ]\n" COR_RESET);

        // 5. LOOP DE ESPERA ATIVA (ASYNCHRONOUS INPUT)
        /* TEORIA: Em vez de parar o programa com um Sleep(7000), dividimos o tempo
           em pequenos ciclos. Isso permite que o sistema responda ao seu pai instantaneamente. */
        int tempo_passado = 0;
        int limite_tempo = INTERVALO_POLLING * 1000; 

        while (tempo_passado < limite_tempo) {
            
            // _kbhit(): Função não-bloqueante que verifica se o buffer do teclado tem dados.
            if (_kbhit()) {
                char tecla = _getch(); // Captura o caractere sem exigir que aperte 'Enter'
                
                // COMANDO DE SAÍDA: ESC (27) ou ENTER (13)
                if (tecla == 13 || tecla == 27) { 
                    registrar_log("Motor V7 desengatado manualmente. Retornando...");
                    return; // Encerra a função e volta para o menu principal
                }

                // ATALHOS RÁPIDOS (Teclas 1 a 9): Baseado na posição visual da lista.
                if (tecla >= '1' && tecla <= '9') {
                    // Conversão de caractere ASCII para inteiro matemático.
                    int indice_alvo = tecla - '0'; 
                    PedidoAtivo *alvo = g_listaRAM.inicio;
                    int contador = 1;

                    // Busca Linear na Lista Encadeada até encontrar a linha correspondente.
                    while (alvo && contador < indice_alvo) {
                        alvo = alvo->prox;
                        contador++;
                    }

                    // Se o pedido for encontrado e não estiver na rua, despacha.
                    if (alvo && strcmp(alvo->status, "EM ROTA") != 0) {
                        enviar_comando_ifood(alvo->orderId, "dispatch"); // Manda comando POST
                        
                        // Atualização Otimista da Interface
                        strcpy(alvo->status, "EM ROTA");
                        alvo->momentoDespacho = time(NULL);
                        g_listaRAM.qtdCozinha--;
                        g_listaRAM.qtdEntrega++;
                        registrar_log("MOTOBOY LIBERADO: Atalho de teclado rápido.");
                        
                        desenhar_monitor_operacional_live();
                        printf(COR_CINZA "\n [ '1'-'9': Despachar Linha | '0': Buscar ID | 'ESC' ou 'ENTER': Menu ]\n" COR_RESET);
                    }
                }

                // BUSCA MANUAL (Tecla 0): Permite furar a fila digitando o ID.
                if (tecla == '0') {
                    char idBuscado[20];
                    printf(COR_AMARELO "\n [ BUSCA MANUAL ] Digite o ID do pedido e aperte ENTER: " COR_RESET);
                    
                    // scanf() aqui é seguro pois o usuário intencionalmente parou para digitar.
                    if (scanf("%19s", idBuscado) == 1) {
                        // Limpa o caractere '\n' que sobra no buffer após o scanf.
                        while(getchar() != '\n'); 

                        PedidoAtivo *alvo = g_listaRAM.inicio;
                        // Busca exaustiva por ID (O(n))
                        while (alvo) {
                            if (strcmp(alvo->displayId, idBuscado) == 0 && strcmp(alvo->status, "EM ROTA") != 0) {
                                break; 
                            }
                            alvo = alvo->prox;
                        }

                        if (alvo) {
                            enviar_comando_ifood(alvo->orderId, "dispatch");
                            strcpy(alvo->status, "EM ROTA");
                            alvo->momentoDespacho = time(NULL);
                            g_listaRAM.qtdCozinha--;
                            g_listaRAM.qtdEntrega++;
                            registrar_log("MOTOBOY LIBERADO: Busca manual por ID.");
                        } else {
                            printf(COR_VERMELHO " [!] Erro: Pedido #%s nao encontrado ou ja despachado!\n" COR_RESET, idBuscado);
                            Sleep(1500); // Pausa curta para leitura do erro
                        }
                    }
                    // Redesenha para limpar as mensagens de busca da tela
                    desenhar_monitor_operacional_live();
                    printf(COR_CINZA "\n [ '1'-'9': Despachar Linha | '0': Buscar ID | 'ESC' ou 'ENTER': Menu ]\n" COR_RESET);
                }
            }
            
            // Pausa de 100ms para evitar uso excessivo de CPU.
            Sleep(100); 
            tempo_passado += 100; 
        } // Fim da Espera Ativa
    } // Fim do While Principal
}






// ====================================================================================== //
// SISTEMA DE INTELIGÊNCIA DE VENDAS - FILTROS TEMPORAIS                                   //
// ====================================================================================== //
void exibir_relatorio_gerencial(int tipo_filtro) {
    /* TIPOS: 1-Hoje, 2-Semana, 3-Quinzena, 4-Mes */
    
    FILE *arq = fopen("historico_v7.txt", "r");
    if (!arq) return;

    char linha[8192];
    double total_vendas = 0;
    int total_pedidos = 0;
    time_t agora = time(NULL);
    double segundos_no_periodo = 0;

    // Define o "corte" de tempo em segundos
    switch(tipo_filtro) {
        case 1: segundos_no_periodo = 86400; break;       // 1 dia
        case 2: segundos_no_periodo = 604800; break;      // 7 dias
        case 3: segundos_no_periodo = 1296000; break;     // 15 dias
        case 4: segundos_no_periodo = 2592000; break;     // 30 dias
    }

    system("cls");
    printf(COR_AMARELO "=== RELATORIO DE VENDAS ===\n\n" COR_RESET);

    while (fgets(linha, sizeof(linha), arq)) {
        // Tokenização: Vamos separar as colunas por "||"
        // Precisamos capturar: Status (#2), Data (#3) e Valor (#5)
        
        char *campos[15];
        int c = 0;
        char *token = strtok(linha, "||");
        while (token && c < 15) {
            campos[c++] = token;
            token = strtok(NULL, "||");
        }

        // 1. Só conta pedidos CONCLUÍDOS
        if (strcmp(campos[2], "CONCLUDED") == 0) {
            
            // 2. Converte a string de data de volta para tempo (Teoria: Parsing de Data)
            struct tm tm_pedido;
            // Exemplo de string: "Tue Apr 14 15:30:00 2026"
            // (Aqui precisaríamos de uma função auxiliar para converter a string do ctime)
            long timestamp_pedido = atol(campos[12]); 
            time_t tempo_pedido = (time_t)timestamp_pedido;

            // 3. O FILTRO: Se a diferença for menor que o período escolhido...
            if (difftime(agora, tempo_pedido) <= segundos_no_periodo) {
                total_vendas += atof(campos[5]); // Soma o valor bruto
                total_pedidos++;
            }
        }
    }

    printf(" Periodo selecionado: %s\n", (tipo_filtro == 1 ? "HOJE" : (tipo_filtro == 2 ? "SEMANA" : "MES")));
    printf(" Total de Pedidos: %d\n", total_pedidos);
    printf(" Faturamento: R$ %.2f\n", total_vendas);
    printf(" Ticket Medio: R$ %.2f\n", total_pedidos > 0 ? total_vendas/total_pedidos : 0);
    
    fclose(arq);
    printf("\n Pressione qualquer tecla para voltar...");
    _getch();
}







  

// ====================================================================================== //
// Diretor de operações ( Função MAIN)                                                    //
// ====================================================================================== //
int main(void) {
    // Configurações de tela que você já tem
    system("mode con: cols=160 lines=40"); 
    SetConsoleOutputCP(65001); 
    
    // Inicializa a biblioteca de rede
    curl_global_init(CURL_GLOBAL_ALL);

    // ==========================================================
    // AQUI ENTRA A MÁGICA:
    // Chamamos a função para garantir que a loja abra no iFood
    // assim que o seu sistema do VS Code começar a rodar.
    // ==========================================================
    abrirLojaManual(); 

    // Força o terminal a abrir com 160 colunas de largura e 40 de altura
    system("mode con: cols=160 lines=40"); 
    SetConsoleOutputCP(65001);
    /* PASSO 1: CALIBRAÇÃO DO AMBIENTE
       SetConsoleOutputCP(65001) configura o terminal do seu Acer para UTF-8. 
       Isso permite que o sistema exiba acentos e símbolos (como o R$ ou o ç) 
       sem bugar as fontes. 
       'curl_global_init' prepara os motores de rede para qualquer conexão futura.
    */
    SetConsoleOutputCP(65001); 
    curl_global_init(CURL_GLOBAL_ALL);

    int opcao = 0;

    /* PASSO 2: O MENU DE NAVEGAÇÃO (Interface Homem-Máquina)
       Enquanto o usuário não digitar '3', o sistema permanece rodando em loop.
       Isso cria a experiência de um software profissional que só fecha quando você quer.
    */
    while(opcao != 3) {
        // Redesenha o cabeçalho azul e branco sempre que voltamos ao menu
        desenhar_cabecalho("CENTRO DE COMANDO PRINCIPAL");
        
        printf(COR_AMARELO " [1]" COR_RESET " Gestor de Pedidos\n");
        printf(COR_AMARELO " [2]" COR_RESET " Banco de Dados\n");
        printf(COR_AMARELO " [3]" COR_RESET " Sair\n\n");
        
        printf(COR_AMARELO " ACAO DE COMANDO: " COR_RESET);
        
        /* PASSO 3: TRATAMENTO DE ERRO DE ENTRADA
           Se o usuário digitar uma letra em vez de número, o 'scanf' falharia e 
           o programa entraria em loop infinito. O 'while(getchar() != '\n')' 
           limpa esse lixo do teclado e protege o sistema.
        */
        if (scanf("%d", &opcao) != 1) {
            while(getchar() != '\n'); 
            continue;
        }

        /* PASSO 4: O SELETOR DE DEPARTAMENTO (Switch Case)
           Aqui é onde você, como comandante, escolhe o que o sistema deve fazer.
        */
        switch(opcao) {
            case 1: 
                // Ativa o monitoramento constante do iFood (seu motor principal)
                modo_vigilante_live(); 
                break;
            case 2:
    desenhar_cabecalho("MENU DE BANCO DE DADOS");
    printf(" [1] Ver Tudo (Historico Bruto)\n");
    printf(" [2] Relatorio de Hoje\n");
    printf(" [3] Relatorio da Semana (7 dias)\n");
    printf(" [4] Relatorio do Mes (30 dias)\n");
    printf("\n ESCOLHA: ");
    int sub;
    scanf("%d", &sub);
    if (sub == 1) exibir_painel_bancos_ssd();
    if (sub == 2) exibir_dashboard_vendas(1);
    if (sub == 3) exibir_dashboard_vendas(7);
    if (sub == 4) exibir_dashboard_vendas(30);
    break;
            case 3: 
                // Procedimento de encerramento seguro
                printf("\nDesligando Motores... Salve a firma do seu pai! Ate logo, Vitor.\n"); 
                Sleep(2000); // Pausa para o usuário ler a mensagem de despedida
                break;
            default: 
                // Tratamento para quando você aperta o botão errado
                printf("\n" COR_VERMELHO "Comando Invalido." COR_RESET "\n"); 
                Sleep(1000);
        }
    }

    /* PASSO 5: DESLIGAMENTO TOTAL
       Libera os recursos globais de rede e devolve o controle total para o Windows.
    */
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