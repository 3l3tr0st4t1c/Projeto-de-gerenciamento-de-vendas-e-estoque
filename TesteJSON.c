#include <stdio.h>
#include <cjson/cJSON.h>
#include <curl/curl.h>

int main() {
    // Teste cJSON: Criando um objeto de pedido
    cJSON *pedido = cJSON_CreateObject();
    cJSON_AddStringToObject(pedido, "restaurante", "Delivery do Pai");
    cJSON_AddNumberToObject(pedido, "total", 150.75);

    char *json_string = cJSON_Print(pedido);
    printf("JSON Gerado: %s\n", json_string);

    // Teste libcurl: Verificando a versão
    curl_version_info_data *vinfo = curl_version_info(CURLVERSION_NOW);
    printf("Versão do libcurl: %s\n", vinfo->version);

    cJSON_Delete(pedido);
    free(json_string);
    return 0;
}