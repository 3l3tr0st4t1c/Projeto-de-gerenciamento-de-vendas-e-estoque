#include "database.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>


int Inicializar_Seguranca(SessaoCripto *sessao) {
    if (sodium_init() < 0) return -1;

    crypto_kx_keypair(sessao->cliente_publica, sessao->cliente_secreta);
    crypto_kx_keypair(sessao->servidor_publica, sessao->servidor_secreta);

    if (crypto_kx_client_session_keys(sessao->chave_rx, sessao->chave_tx,
                                      sessao->cliente_publica, sessao->cliente_secreta, sessao->servidor_publica) != 0) {
        return -1;
    }

    if (crypto_kx_server_session_keys(sessao->chave_rx, sessao->chave_tx,
                                      sessao->servidor_publica, sessao->servidor_secreta, sessao->cliente_publica) != 0) {
        return -1;
    }

    crypto_aead_aes256gcm_keygen(sessao->chave_local);

    randombytes_buf(sessao->nonce, sizeof (sessao->nonce));
    return 0;
}


int Gravar_Buffer_Offline(uint8_t operacao, ColunaCliente *c, ColunaFinanceiro *f_fin, ColunaEntrega *e_ent, ColunaStatus *st, SessaoCripto *sessao) {
    const char *nome_arquivo_tmp = "buffer_offline.tmp";
    RegistroOffline registro;

    memset(&registro, 0, sizeof(RegistroOffline));
    registro.operacao = operacao;
    memcpy(&registro.cliente, c, sizeof(ColunaCliente));
    memcpy(&registro.financeiro, f_fin, sizeof(ColunaFinanceiro));
    memcpy(&registro.entrega, e_ent, sizeof(ColunaEntrega));
    memcpy(&registro.status, st, sizeof(ColunaStatus));

    FILE *f = fopen(nome_arquivo_tmp, "ab");

    if(!f) {
        printf("Erro Critico: Nao foi possivel abrir o buffer offline.\n");
        return -1;
    }

    unsigned char nonce_local[crypto_aead_aes256gcm_NPUBBYTES];
    randombytes_buf(nonce_local, sizeof(nonce_local));

    unsigned char ciphertext[sizeof(RegistroOffline) + crypto_aead_aes256gcm_ABYTES];
    unsigned long long ciphertext_len;

    crypto_aead_aes256gcm_encrypt(
        ciphertext, &ciphertext_len,
        (const unsigned char*)&registro, sizeof(RegistroOffline),
        NULL, 0, NULL, nonce_local, sessao->chave_local
                                  );

    fwrite(nonce_local, 1, sizeof(nonce_local), f);
    fwrite(ciphertext, 1, ciphertext_len, f);
    fclose(f);

    return 0;
}


void Preparar_Carga_Criptografada(){
    const char *arquivo_origem = "buffer_offline.tmp";
    const char *arquivo_destino = "carga_cripto.enc";
    SessaoCripto sessao;
    if (Inicializar_Seguranca(&sessao) != 0) return;

    if (access(arquivo_origem, F_OK) != 0) {
        printf("Nada a enviar. \n");
        return;
    }

    FILE *f = fopen(arquivo_origem, "rb");
    FILE *a = fopen(arquivo_destino, "wb");
    if (!f || !a) return;

    fwrite(sessao.nonce, 1, sizeof(sessao.nonce), a);


    unsigned char nonce_local[crypto_aead_aes256gcm_NPUBBYTES];
    unsigned char ciphertext_local[sizeof(RegistroOffline) + crypto_aead_aes256gcm_ABYTES];
    unsigned char decrypted[sizeof(RegistroOffline)];
    unsigned long long decrypted_len;
    unsigned char ciphertext_rede[sizeof(RegistroOffline) + crypto_aead_aes256gcm_ABYTES];
    unsigned long long ciphertext_rede_len;

    while (fread(nonce_local, 1, sizeof(nonce_local), f) == sizeof(nonce_local) &&
           fread(ciphertext_local, 1, sizeof(ciphertext_local), f) == sizeof(ciphertext_local)) {

        if (crypto_aead_aes256gcm_decrypt (
                decrypted, &decrypted_len, NULL,
                ciphertext_local, sizeof(ciphertext_local), NULL, 0,
                nonce_local, sessao.chave_local) != 0) {
            printf("ERRO: Buffer local corrompido ou chave invalida.\n");
            continue;
        }

        crypto_aead_aes256gcm_encrypt(
            ciphertext_rede, &ciphertext_rede_len, decrypted,
            decrypted_len, NULL, 0, NULL, sessao.nonce, sessao.chave_tx
);

        fwrite(ciphertext_rede, 1, ciphertext_rede_len, a);
        sodium_increment(sessao.nonce, sizeof(sessao.nonce));
    }

    fclose(f);
    fclose(a);

    remove(arquivo_origem);

    printf("Carga preparada e enviada para a fila de rede.\n");
}


int Desmonte(const char *arquivo_recebido_enc,const char *nome_banco_dat) {
    const char *arquivo_temp_dec = "buffer_recebido_dec.tmp";

    FILE *f = fopen(arquivo_recebido_enc, "rb");
    FILE *t = fopen(arquivo_temp_dec, "wb+");
    if (!f || !t) return -1;

    SessaoCripto sessao;
    if (fread(sessao.nonce, 1, sizeof(sessao.nonce), f) != sizeof(sessao.nonce)) {
        printf("ERRO CRITICO: Pacote de rede vazio ou incompleto. Descartando.\n");
        fclose(f); fclose(t);
        return -1;
    }


    unsigned char ciphertext[sizeof(RegistroOffline) + crypto_aead_aes256gcm_ABYTES];
    size_t bytes_lidos;

    unsigned char decrypted[sizeof(RegistroOffline)];
    unsigned long long decrypted_len;

    while ((bytes_lidos = fread(ciphertext, 1, sizeof(ciphertext), f)) > 0) {
        if (crypto_aead_aes256gcm_decrypt(
                decrypted, &decrypted_len,
                NULL,
                ciphertext, bytes_lidos,
                NULL, 0,
                sessao.nonce, sessao.chave_rx) != 0) {

            printf("ERRO CRÍTICO: Arquivo forjado ou corrompido! Descartando carga.\n");
            fclose(f); fclose(t);
            return -1;
        }

        fwrite(decrypted, 1, decrypted_len, t);
        sodium_increment(sessao.nonce, sizeof(sessao.nonce));
    }


    rewind(t);
    RegistroOffline registro;
    while (fread(&registro, sizeof(RegistroOffline), 1, t) == 1) {
        if (registro.operacao == 1) {
            Inserir_Pedido(nome_banco_dat, &registro.cliente, &registro.financeiro, &registro.entrega, &registro.status);
        } else if(registro.operacao == 2) {
            //colocar aqui a futura função de atualizar status de pedidos,
            //nós já temos ela? tem que ver com os cara lá. ah se lascar.
            }
    }

    fclose(f);
    fclose(t);

    return 0;
}
