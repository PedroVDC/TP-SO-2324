#ifndef MOTOR_H
#define MOTOR_H

#include <stdbool.h>
#include <pthread.h>

#define TAMCOMANDOS 2048
#define LINHASMAPA 16
#define COLUNASMAPA 40
#define PIPESERVER "Pipesrv"
#define NUM_THREADS 10
#define MAX_UTILIZADORES 5
#define MAX_PROCESSBOT 10
#define MAX_PEDRAS 50
#define MAX_NIVEIS 3
#define MAX_BLOQMOVEL 5
#define MSG_SIZE 100

#define SRV_FIFO "tubo"

typedef struct VarAmbiente
{
    int inscricao;
    int nplayers;
    int duracao;
    int decremento;
} VarAmbiente;

typedef struct Utilizador Utilizador, *p_utilizador;
typedef struct Motor Motor, *p_motor;

typedef struct Posicao
{
    int posX;
    int posY;
} Posicao;

typedef struct Obstaculo
{
    Posicao posicao[5]; // 1 = LEFT // 2 = RIGHT // 3 = UP // 4 = DOWN ///0 = POSICAO ATUAL
    int flagVazio;
    bool temMovimento;
    int duracao;

} Obstaculo;

typedef struct Mapa
{
    char mapaLab[LINHASMAPA][COLUNASMAPA];
    int pedras;
    int bloqmovel;
    Obstaculo obstaculo[55];
} Mapa;

struct Utilizador
{
    int flagVazio;
    char represent;
    char letraR;
    char nome[50];
    char pipename[150];
    char msg[300];
    Mapa mapa;
    Posicao posicao[5]; // 1 = LEFT // 2 = RIGHT // 3 = UP // 4 = DOWN ///0 = POSICAO ATUAL
};

typedef struct ParametrosBot
{
    int flagContinua;
    char *duracaoBot;
    char *intervalo;
    Motor *motor;
    pthread_mutex_t *mexerMapaUtilizador;
    int terminaThread;
} ParametrosBot;

typedef struct Intervalos
{
    char *duracaoBot;
    char *intervalo;
}Intervalos;

typedef struct tDadosIniciaBot{
    Intervalos intervalos;
    Motor *motor;
}tDadosIniciaBot;


struct Motor
{

    /*VARIAVEIS PARA: iniciaBot*/
    Intervalos *intervalos;
    int lastIndexAdd; // ultimo index que criei bot
    pthread_t threadsBot[NUM_THREADS];

    /*VARIAVEIS PARA: jogo*/
    /*DADOS REFERENTES A: estruturas*/
    Utilizador utilizadores[MAX_UTILIZADORES];
    VarAmbiente v;
    Mapa mapa;

    /*VARIAVEIS */
    int nivelAtual;    // variaveis para mudar de nivel
    int fd_PIPESERVER; // variavel para leituras

    /*VARIAVEIS PARA: Terminar threads*/
    int continua;
    int terminaThread;
    int flagTerminaAtualizacao;

    /*mutex para bloquear acesso ao mapa*/
    pthread_mutex_t *mexerMapaUtilizador;
};

void enviaTodos(Motor *motor, const char *message);
void ovar(VarAmbiente *vars);
void printMapa(char mapaLab[LINHASMAPA][COLUNASMAPA]);
#endif
