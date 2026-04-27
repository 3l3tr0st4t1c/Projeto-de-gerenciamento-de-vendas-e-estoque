#include <stdio.h>
#include <curl/curl.h> // Biblioteca para fazer requisições HTTP (internet)
#include "status_loja.h"

/* * A palavra 'extern' avisa ao C que essas variáveis e funções já existem 
 * no seu arquivo principal. Assim não precisamos criá-las de novo aqui.
 */
extern char g_accessToken[2048];       // O token que você já capturou
extern void registrar_log(const char *msg); // Sua função de log bonitona
extern void garantir_token_valido();   // Sua função que chega se o token expirou

void abrirLojaManual() {
    // PASSO 1: Segurança em primeiro lugar. 
    // Garante que temos uma "chave" válida antes de tentar abrir a porta da loja.
    garantir_token_valido();

    // PASSO 2: Inicializa o motor do cURL para esta tarefa específica
    CURL *curl = curl_easy_init();
    
    if(curl) {
        struct curl_slist *headers = NULL;
        char auth_h[2048];

        // PASSO 3: Configuramos o destino (URL). 
        // O ID 3693587 é o "RG" da sua loja de teste no iFood.
        curl_easy_setopt(curl, CURLOPT_URL, "https://merchant-api.ifood.com.br/merchant/v1.0/merchants/3693587/status");
        
        // PASSO 4: Definimos o método como PUT. 
        // Na linguagem de APIs: GET é ler, POST é criar e PUT é atualizar (mudar status).
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");

        // PASSO 5: Criamos os Cabeçalhos (Headers).
        // É aqui que provamos quem somos (Bearer Token) e o que estamos enviando (JSON).
        sprintf(auth_h, "Authorization: Bearer %s", g_accessToken);
        headers = curl_slist_append(headers, auth_h);
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // PASSO 6: O "Payload" ou Corpo da mensagem.
        // Enviamos um mini-JSON dizendo: "Na operação de entrega, fique DISPONÍVEL".
        const char *json_data = "[{\"operation\": \"DELIVERY\", \"status\": \"AVAILABLE\"}]";
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);

        // PASSO 7: Executa a missão! 
        // Se a resposta for CURLE_OK (sucesso na conexão), registramos no seu log.
        if(curl_easy_perform(curl) == CURLE_OK) {
            registrar_log("COMANDO ENVIADO: Loja de teste aberta com sucesso!");
        } else {
            registrar_log("ERRO: Falha de conexão ao tentar abrir a loja.");
        }

        // PASSO 8: Limpeza de memória. 
        // Como você aprendeu, em C o que você abre, você tem que fechar para não dar "leak".
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
}

void injetarCardapioTeste() {
    garantir_token_valido();

    CURL *curl = curl_easy_init();
    if(curl) {
        struct curl_slist *headers = NULL;
        char auth_h[2048];

        // Endpoint da API de Catálogo para criar uma Categoria
        // Atenção ao final da URL: /categories
        curl_easy_setopt(curl, CURLOPT_URL, "https://merchant-api.ifood.com.br/catalog/v1.0/merchants/3693587/categories");
        
        // Método POST: Usado para INSERIR dados no banco
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");

        sprintf(auth_h, "Authorization: Bearer %s", g_accessToken);
        headers = curl_slist_append(headers, auth_h);
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // O Corpo do Texto (Payload)
        // Estamos dizendo ao banco: Crie a categoria "Lanches V7" e deixe Disponível
        const char *json_categoria = "{\"name\": \"Lanches V7\", \"status\": \"AVAILABLE\", \"template\": \"DEFAULT\"}";
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_categoria);

        printf("\nInjetando Categoria no Banco de Dados do iFood...\n");

        if(curl_easy_perform(curl) == CURLE_OK) {
            registrar_log("SUCESSO: Categoria de teste injetada via API!");
        } else {
            registrar_log("ERRO: Falha ao injetar cardapio.");
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
}