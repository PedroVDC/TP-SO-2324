#ifndef JOGOUI_H
#define JOGOUI_H

#include <stdbool.h>
#include <pthread.h>
#include <ncurses.h>
#define LINHASMAPA 16
#define COLUNASMAPA 40
#define TAMCOMANDOS 200
#define PIPESERVER "Pipesrv"

typedef struct Utilizador Utilizador, *p_utilizador;

typedef struct Mapa
{
    char mapaLab[LINHASMAPA][COLUNASMAPA];
    int pedras;
    int bloqmovel;
} Mapa;

typedef struct Posicao
{
    int posX;
    int posY;
} Posicao;

struct Utilizador
{
    int flagVazio;
    char letraR;
    char nome[50];
    char pipename[150];
    char msg[300];
    Mapa *mapa;
    Posicao posicao[5]; // 1 = LEFT // 2 = RIGHT // 3 = UP // 4 = DOWN ///0 = POSICAO ATUAL
};

typedef struct EstruturaAux
{
    WINDOW *janelaTopo;
    WINDOW *janelaBaixo;
    WINDOW *janelaLado;
    int represent;
    char shutdown;
    int fd_retorno;
    int flag;
    pthread_t threads[2];
    pthread_mutex_t *ptrinco;
    pthread_t continua;
    char parceiro[20];
}EstruturaAux;

typedef struct Tdados{
    Utilizador *utilizador;
    EstruturaAux *eAux;
}Tdados;



#endif