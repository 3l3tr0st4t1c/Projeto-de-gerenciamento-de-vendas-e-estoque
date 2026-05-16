#ifndef DATABASE_H
#define DATABASE_H
#define OFFSET_NULL ((uint64_t)-1)
#include <stdint.h>
#include <sodium.h>
#include <time.h>
#define TAMANHO_BLOCO_CIFRADO (TAMANHO_BLOCO + crypto_aead_aes256gcm_NPUBBYTES + crypto_aead_aes256gcm_ABYTES)
#define TAMANHO_BLOCO 4096
extern unsigned char CHAVE_MESTRA_SGBD[crypto_aead_aes256gcm_KEYBYTES];

#pragma pack(push, 1)

typedef struct {
    uint8_t assinatura[8];
    uint64_t offset_raiz;
    uint64_t prox_bloco_livre;
    uint64_t head_free_list;
    uint32_t qtd_pedidos;
    uint8_t padding[4060];
} header;

typedef struct {
    uint32_t idCliente;
    char nome[100];
    char telefone[20];
    uint8_t padding[3972];
}ColunaCliente;

typedef struct {
    int64_t valorBruto;
    int64_t taxaEntrega;
    uint8_t metodoPagamento;
    uint8_t padding[4079];
}ColunaFinanceiro;

typedef struct {
    char bairro[50];
    char logradouro[150];
    char numero[20];
    char complemento[100];
    char referencia[150];
    uint8_t padding[3626];
}ColunaEntrega;

typedef struct {
    int displayID;
    uint8_t status;
    int64_t momentoEntrada;
    int64_t momentoDespacho;
    uint8_t padding[4075];
}ColunaStatus;

typedef struct {
    uint64_t bloco_inicio;
    uint64_t qtd_blocos;
} Extent;

typedef struct {
    Extent col_cliente;
    Extent col_financeiro;
    Extent col_entrega;
    Extent col_status;
    Extent col_dna;
} EnderecoColunar;

typedef struct {
    unsigned char EH_FOLHA;
    uint32_t qtd_chaves;
    int chaves[43];
    uint64_t filhos[44];
    EnderecoColunar enderecos[43];
   uint8_t padding[127];
} No;

typedef struct {
    ColunaCliente cliente;
    ColunaFinanceiro financeiro;
    ColunaEntrega entrega;
    ColunaStatus status;
    uint8_t operacao;
} RegistroOffline;

typedef struct {
    unsigned char cliente_publica[crypto_kx_PUBLICKEYBYTES];
    unsigned char cliente_secreta[crypto_kx_SECRETKEYBYTES];
    unsigned char servidor_publica[crypto_kx_PUBLICKEYBYTES];
    unsigned char servidor_secreta[crypto_kx_SECRETKEYBYTES];

    unsigned char chave_tx[crypto_kx_SESSIONKEYBYTES];
    unsigned char chave_rx[crypto_kx_SESSIONKEYBYTES];
    unsigned char nonce[crypto_aead_aes256gcm_NPUBBYTES];

    unsigned char chave_local[crypto_aead_aes256gcm_KEYBYTES];
}SessaoCripto;

#pragma pack(pop)

int Inicializar_Banco(const char *nome_arquivo);
int Gravar_Bloco(const char *nome_arquivo, uint64_t id_bloco, const void *dados);
int Ler_Bloco(const char *nome_arquivo, uint64_t id_bloco, void *buffer);
EnderecoColunar Buscar_Pedido(const char *nome_arquivo, int id_procurado);
EnderecoColunar Persistir_Dados(const char *nome_arquivo, ColunaCliente *c, ColunaFinanceiro *f_fin, ColunaEntrega *e_ent, ColunaStatus *s);
int Inserir_Pedido(const char *nome_arquivo, ColunaCliente *c, ColunaFinanceiro *f_fin, ColunaEntrega *e_ent, ColunaStatus *st);
void Dividir_No(const char *nome_arquivo, uint64_t id_pai, No *pai, uint32_t i, uint64_t id_filho_cheio, No *filho_cheio);
void Inserir_Nao_Cheio(const char *nome_arquivo, uint64_t id_no,No *no, int id_novo, EnderecoColunar e);
int Gravar_Buffer_Offline (uint8_t operacao, ColunaCliente *c, ColunaFinanceiro *f_fin, ColunaEntrega *e_ent, ColunaStatus *st, SessaoCripto *sessao);
int Desmonte(const char *arquivo_recebido_enc, const char *nome_banco_dat);
int Inicializar_Seguranca (SessaoCripto *sessao);

#endif
