#include "database.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sodium.h>

unsigned char CHAVE_MESTRA_SGBD[crypto_aead_aes256gcm_KEYBYTES] = {0};


int Gravar_Bloco(const char *nome_arquivo, uint64_t id_bloco, const void *dados) {
    FILE *f = fopen(nome_arquivo, "rb+");
    if (!f) return -1;

    unsigned char nonce[crypto_aead_aes256gcm_NPUBBYTES];
    randombytes_buf(nonce, sizeof(nonce));

    unsigned char ciphertext[TAMANHO_BLOCO + crypto_aead_aes256gcm_ABYTES];
    unsigned long long ciphertext_len;

    crypto_aead_aes256gcm_encrypt (
        ciphertext, &ciphertext_len,
        (const unsigned char*)dados, TAMANHO_BLOCO,
        NULL, 0, NULL, nonce, CHAVE_MESTRA_SGBD
);

    fseek(f, id_bloco * TAMANHO_BLOCO_CIFRADO, SEEK_SET);
    fwrite(nonce, 1, sizeof(nonce), f);
    fwrite(ciphertext, 1, ciphertext_len, f);

    fclose(f);
    return 0;
}


int Ler_Bloco(const char *nome_arquivo, uint64_t id_bloco, void *buffer){
    FILE *f = fopen(nome_arquivo, "rb");
    if (!f) return -1;

    fseek(f, id_bloco * TAMANHO_BLOCO_CIFRADO, SEEK_SET);

    unsigned char nonce[crypto_aead_aes256gcm_NPUBBYTES];
    unsigned char ciphertext[TAMANHO_BLOCO + crypto_aead_aes256gcm_ABYTES];

    if (fread(nonce, 1, sizeof(nonce), f) != sizeof(nonce)) {fclose(f); return -1;}
    if (fread(ciphertext, 1, sizeof(ciphertext), f) != sizeof(ciphertext)) {fclose(f); return -1;}

    unsigned long long decrypted_len;

    if (crypto_aead_aes256gcm_decrypt(
            (unsigned char*)buffer, &decrypted_len, NULL,
            ciphertext, sizeof(ciphertext), NULL, 0,
            nonce, CHAVE_MESTRA_SGBD) != 0) {
        printf("ERRO FATAL: Bloco %lu corrompido ou chave mestra incorreta.\n", id_bloco);
        fclose(f);
        return -1;
    }


    fclose(f);
    return 0;
}


No* Criar_no_Vazio(unsigned char eh_folha) {
    No *novo_no = (No*) malloc(sizeof(No));

    if (novo_no == NULL) {
        printf("Erro Critico: Falha na alocacao de memoria!");
        return NULL;
    }

    memset(novo_no, 0, sizeof(No));
    novo_no->EH_FOLHA = eh_folha;
    novo_no->qtd_chaves = 0;

    for (int i = 0; i < 44; i++) {
        novo_no->filhos[i] = OFFSET_NULL;
    }

    return novo_no;
}


int Inicializar_Banco(const char *nome_arquivo){
    if (access(nome_arquivo, F_OK) != 0) {
        FILE *f = fopen(nome_arquivo, "wb+");
        if (!f) return -1;
        fclose(f);

        header header;
        memset(&header, 0, sizeof(header));
        memcpy(header.assinatura, "MongolDB", 8);
        header.offset_raiz = OFFSET_NULL;

        No *raiz = Criar_no_Vazio(1);
        header.offset_raiz = 1;
        header.prox_bloco_livre = 2;

        Gravar_Bloco(nome_arquivo, 0, &header);
        Gravar_Bloco(nome_arquivo, 1, raiz);
        free(raiz);
        return 0;
    }
    return 1;
}

EnderecoColunar Persistir_Dados(const char *nome_arquivo, ColunaCliente *c, ColunaFinanceiro *f_fin, ColunaEntrega *e_ent, ColunaStatus *s) {
    EnderecoColunar ender;
    memset(&ender, 0, sizeof(EnderecoColunar));

    header h;
    if (Ler_Bloco(nome_arquivo, 0, &h) != 0) {
        printf("Erro ao ler o cabecalho.\n");
        return ender;
    }

    uint64_t base_id = h.prox_bloco_livre;

    Gravar_Bloco(nome_arquivo, (base_id), c);
    Gravar_Bloco(nome_arquivo, (base_id + 1), f_fin);
    Gravar_Bloco(nome_arquivo, (base_id + 2), e_ent);
    Gravar_Bloco(nome_arquivo, (base_id + 3), s);

    ender.col_cliente.bloco_inicio = base_id;    ender.col_cliente.qtd_blocos = 1;
    ender.col_financeiro.bloco_inicio = base_id + 1;    ender.col_financeiro.qtd_blocos = 1;
    ender.col_entrega.bloco_inicio = base_id + 2;    ender.col_entrega.qtd_blocos = 1;
    ender.col_status.bloco_inicio = base_id + 3;    ender.col_status.qtd_blocos = 1;

    h.prox_bloco_livre += 4;
    Gravar_Bloco(nome_arquivo, 0, &h);

    return ender;
}

EnderecoColunar Buscar_Pedido(const char *nome_arquivo, int id_procurado) {
        header h;
        EnderecoColunar erro_retorno;
        memset(&erro_retorno, 0, sizeof(EnderecoColunar));

        if (Ler_Bloco(nome_arquivo, 0, &h) != 0) return erro_retorno;
        if (h.offset_raiz == OFFSET_NULL) return erro_retorno;

        uint64_t bloco_atual = h.offset_raiz;
        No *no_temp = (No*) malloc(sizeof(No));

        while (bloco_atual != OFFSET_NULL) {
            Ler_Bloco(nome_arquivo, bloco_atual, no_temp);
            uint32_t i = 0;

            while (i < no_temp->qtd_chaves && id_procurado > no_temp->chaves[i]) {
                i++;
            }

            if (i < no_temp->qtd_chaves && no_temp->chaves[i] == id_procurado) {
                EnderecoColunar resultado = no_temp->enderecos[i];
                free(no_temp);
                return resultado;
            }

            if (no_temp->EH_FOLHA) {
                break;
            }

            bloco_atual = no_temp->filhos[i];
        }

    free(no_temp);
    return erro_retorno;
}


int Inserir_Pedido (const char *nome_arquivo, ColunaCliente *c, ColunaFinanceiro *f_fin, ColunaEntrega *e_ent, ColunaStatus *st) {
    int id_novo = st->displayID;
    EnderecoColunar e = Persistir_Dados(nome_arquivo, c, f_fin, e_ent, st);
    header h;

    if (Ler_Bloco(nome_arquivo, 0, &h) != 0) {
        printf("Erro ao ler o cabecalho.\n");
        return -1;
    }

    No *raiz = (No*) malloc(sizeof(No));

    if (Ler_Bloco(nome_arquivo , h.offset_raiz, raiz) != 0) {
        printf("Erro ao carregar a raiz.\n");
        free(raiz);
        return -1;
    }

    if (raiz->qtd_chaves == 43) {
        No *s = Criar_no_Vazio(0);
        uint64_t novo_id_de_s = h.prox_bloco_livre;
        h.prox_bloco_livre++;
        Gravar_Bloco(nome_arquivo, 0, &h);
        s->filhos[0] = h.offset_raiz;

        Dividir_No(nome_arquivo, novo_id_de_s, s, 0, h.offset_raiz, raiz);

        Ler_Bloco(nome_arquivo, 0, &h);

        h.offset_raiz = novo_id_de_s;
        Gravar_Bloco(nome_arquivo, 0, &h);

        Inserir_Nao_Cheio(nome_arquivo, novo_id_de_s, s, id_novo, e);
        free(s);
    } else {
        Inserir_Nao_Cheio(nome_arquivo, h.offset_raiz, raiz, id_novo, e);
    }

    Ler_Bloco(nome_arquivo, 0, &h);
    h.qtd_pedidos++;
    Gravar_Bloco(nome_arquivo, 0, &h);

    free(raiz);
    return 0;
}


void Dividir_No(const char *nome_arquivo, uint64_t id_pai, No *pai, uint32_t i, uint64_t id_filho_cheio, No *filho_cheio) {
    header h;
    Ler_Bloco(nome_arquivo, 0, &h);

    No *Novo_No = Criar_no_Vazio(filho_cheio->EH_FOLHA);
    uint64_t offset_Novo_No = h.prox_bloco_livre;
    h.prox_bloco_livre++;
    Gravar_Bloco(nome_arquivo, 0, &h);
    Novo_No->qtd_chaves = 21;

    for (int j = 0; j < 21; j++) {
        Novo_No->chaves[j] = filho_cheio->chaves[j + 22];
        Novo_No->enderecos[j] = filho_cheio->enderecos[j + 22];
    }

    if (filho_cheio->EH_FOLHA == 0) {
        for (int j = 0; j < 22; j++) {
            Novo_No->filhos[j] = filho_cheio->filhos[j + 22];
        }
    }

    filho_cheio->qtd_chaves = 21;

    for (int j = (int)pai->qtd_chaves; j >= (int)i + 1; j--) {
        pai->filhos[j + 1] = pai->filhos[j];
    }
    pai->filhos[i + 1] = offset_Novo_No;

    for (int j = (int)pai->qtd_chaves - 1; j >= (int)i; j--) {
        pai->chaves[j + 1] = pai->chaves[j];
        pai->enderecos[j + 1] = pai->enderecos[j];
    }

    pai->chaves[i] = filho_cheio->chaves[21];
    pai->enderecos[i] = filho_cheio->enderecos[21];
    pai->qtd_chaves++;

    Gravar_Bloco(nome_arquivo, (uint64_t)offset_Novo_No, Novo_No);
    Gravar_Bloco(nome_arquivo, id_filho_cheio, filho_cheio);
    Gravar_Bloco(nome_arquivo, id_pai, pai);

    free(Novo_No);
}


void Inserir_Nao_Cheio (const char *nome_arquivo, uint64_t id_no, No *no, int id_novo, EnderecoColunar e) {
    int i = no->qtd_chaves -1;

    if (no->EH_FOLHA == 1) {
        while (i >= 0 && id_novo < no->chaves[i]) {
            no->chaves[i + 1] = no->chaves[i];
            no->enderecos[i + 1] = no->enderecos[i];
            i--;
        }

        no->chaves[i + 1] = id_novo;
        no->enderecos[i + 1] = e;
        no->qtd_chaves++;

        Gravar_Bloco(nome_arquivo, id_no, no);
    } else {
        while (i >= 0 && id_novo < no->chaves[i]) {
            i--;
        }
        i++;

        No *filho = malloc(sizeof(No));
        uint64_t id_filho = no->filhos[i];
        Ler_Bloco(nome_arquivo, id_filho, filho);

        if (filho->qtd_chaves == 43) {
            Dividir_No(nome_arquivo, id_no, no, i, id_filho, filho);

            if (id_novo > no->chaves[i]) {
                i++;

                id_filho = no->filhos[i];
                Ler_Bloco(nome_arquivo, id_filho, filho);
            }
        }

        Inserir_Nao_Cheio(nome_arquivo, id_filho, filho, id_novo, e);
        free(filho);
    }
}
