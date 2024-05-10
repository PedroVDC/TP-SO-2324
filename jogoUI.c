#include "jogoUI.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>

void desenhaMapa(WINDOW *janela, int tipo, Utilizador *utilizador, EstruturaAux *eAux)
{
    Utilizador user;
    read(eAux->fd_retorno, &user, sizeof(Utilizador));
    if (tipo == 1)
    {
        scrollok(janela, TRUE);
    }
    else
    {
        keypad(janela, TRUE);
        wclear(janela);
        for (int i = 0; i < LINHASMAPA; i++)
        {
            wprintw(janela, "%s\n", user.mapa->mapaLab[i]);
        }
    }
    refresh();
    wrefresh(janela);
    wmove(janela, 1, 1);
}

void *readerThread(void *arg)
{
    Tdados *dados = (Tdados *)arg;
    Utilizador user;
    EstruturaAux eAux;

    while (dados->eAux->continua == 0)
    {
        int bytesRead = read(dados->eAux->fd_retorno, &user, sizeof(Utilizador));
        /*============================v================================================*/
        if (bytesRead == -1)
        {
            printf("[erro] - impossivel ler o pipe");
            break;
        }
        /*============================^================================================*/
        if (bytesRead > 0)
        {

            if (strcmp(user.msg, "e") == 0) // e = exit
            {
                printf("JOGO TERMINADO");
                dados->eAux->shutdown = 'q';
                break;
            }
            else if (strcmp(user.msg, "k") == 0) // k = kick
            {
                printf("FOSTE KICKADO");
                dados->eAux->shutdown = 'q';
                break;
            }
            else if (strcmp(user.msg, "am") == 0) // am = atualiza mapa
            {
                // UTILIZAR POR COPIA
                // read para ler o desenhaMapa
                // logica para atualizar o mapa no ecra

                bytesRead = read(dados->eAux->fd_retorno, &user, sizeof(Utilizador));

                dados->utilizador->posicao->posX = user.posicao->posX;
                dados->utilizador->posicao->posY = user.posicao->posY;

                wmove(dados->eAux->janelaTopo, dados->utilizador->posicao->posX, dados->utilizador->posicao->posY);
                desenhaMapa(dados->eAux->janelaTopo, 1, dados->utilizador, dados->eAux);
            }
            wprintw(dados->eAux->janelaLado, "%s", user.msg);
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{

    char comando[TAMCOMANDOS];
    Utilizador utilizador;
    EstruturaAux eAux;
    int fd, nbytes, fd_retorno;
    char nome[30];
    pthread_t readerThreadID;
    char *firstWord, *secondWord, *thirdWord;

    bool correuBem;

    if (argc < 2)
    {
        printf("[erro] - Verificar Comandos de Entrada, Insira o seu Nome\n");
        exit(7);
    }
    if (access(PIPESERVER, F_OK) != 0)
    {
        printf("[erro] - servidor nao esta aberto\n");
        exit(1);
    }
    strcpy(utilizador.nome, argv[1]);

    sprintf(nome, "%d", getpid());

    char *nomeAux = strcat(nome, utilizador.nome);
    strcpy(utilizador.pipename, nomeAux);

    utilizador.letraR = utilizador.nome[0];

    printf("%s\n", utilizador.nome);

    mkfifo(utilizador.pipename, 0666);
    fd_retorno = open(utilizador.pipename, O_RDWR);
    fd = open(PIPESERVER, O_WRONLY);

    if (fd == -1)
    {
        perror("[erro] - abrir o fifo para leitura nao foi possivel");
        exit(EXIT_FAILURE);
    }

    nbytes = write(fd, &utilizador, sizeof(Utilizador));
    close(fd);
    // printf("Manda dados: \n%s\n%s\n", utilizador.nome, utilizador.pipename);
    eAux.fd_retorno = fd;

    initscr();
    noecho();
    keypad(stdscr, TRUE);

    mvprintw(1, 1, "COMANDOS JANELA CIMA: Setas do Teclado || SAIR - [q]\n");
    mvprintw(2, 1, "MODO DE ESCRITA - [space]");
    mvprintw(3, 41, "CHAT LOG:");
    mvprintw(3, 1, "LABIRINTO:");

    eAux.janelaTopo = newwin(16, 40, 4, 1);
    eAux.janelaBaixo = newwin(8, 80, 20, 1);
    eAux.janelaLado = newwin(16, 60, 5, 42);
    desenhaMapa(eAux.janelaTopo, 2, &utilizador, &eAux);
    refresh();
    Tdados dados;
    dados.eAux = &eAux;
    dados.utilizador = &utilizador;
    pthread_create(&readerThreadID, NULL, readerThread, (void *)&dados);

    // INTERAÇÔES DO JOGADOR COMANDOS QUE PODE INSERIR
    char tecla = wgetch(eAux.janelaTopo);
    while (tecla != 113 || eAux.shutdown != 'q')
    {

        // TRATAR KEYS PARA WRITES PARA O SERV E ATUALIZAR O USER SEMPRE QUE CLICK ON KEY
        if (tecla == KEY_UP)
        {
            strcpy(utilizador.msg, "u");
            write(fd_retorno, &utilizador, sizeof(utilizador));
            close(fd_retorno);
        }
        else if (tecla == KEY_RIGHT)
        {
            strcpy(utilizador.msg, "r");
            write(fd_retorno, &utilizador, sizeof(utilizador));
            close(fd_retorno);
        }
        else if (tecla == KEY_LEFT)
        {
            strcpy(utilizador.msg, "l");
            write(fd_retorno, &utilizador, sizeof(utilizador));
            close(fd_retorno);
        }
        else if (tecla == KEY_DOWN)
        {
            strcpy(utilizador.msg, "d");
            write(fd_retorno, &utilizador, sizeof(utilizador));
            close(fd_retorno);
        }
        else if (tecla == ' ')
        {
            echo();
            while (1)
            {
                wprintw(eAux.janelaBaixo, "<%s>", utilizador.nome);
                wgetstr(eAux.janelaBaixo, comando);
                firstWord = strtok(comando, " ");
                if (strcasecmp(comando, "players") == 0)
                {
                    wprintw(eAux.janelaLado, "LISTA DE PLAYERS:\n");
                    wprintw(eAux.janelaLado, "%s", utilizador.nome);
                    wrefresh(eAux.janelaLado);
                    wclear(eAux.janelaBaixo);
                }
                if (strcasecmp(comando, " ") == 0)
                {
                    break;
                }
                if (strcasecmp(comando, "") == 0)
                {
                    write(fd_retorno, &utilizador, sizeof(utilizador));
                    close(fd_retorno);
                    continue;
                }

                if (strcasecmp(firstWord, "msg") == 0)
                {
                    secondWord = strtok(NULL, " ");
                    thirdWord = strtok(NULL, "\n");
                    if (firstWord == NULL || secondWord == NULL || thirdWord == NULL || strcmp(thirdWord, " ") == 0)
                    {
                        wprintw(eAux.janelaBaixo, "[erro] - Syntax: msg <destinatario> <mensagem>\n");
                        correuBem = false;
                    }
                    else
                        correuBem = true;

                    if (correuBem)
                    {
                        strcpy(eAux.parceiro, secondWord); // ?????????????????????????
                        strcpy(utilizador.msg, thirdWord);

                        wprintw(eAux.janelaLado, "<%s>[%s]: %s", utilizador.nome, secondWord, thirdWord);

                        write(fd_retorno, &utilizador, sizeof(utilizador));
                        close(fd_retorno);

                        wrefresh(eAux.janelaLado);
                        wclear(eAux.janelaLado);
                    }
                }
            }
        }
        wmove(eAux.janelaTopo, 1, 1);
        tecla = wgetch(eAux.janelaTopo);
    }

    eAux.flag = 0;
    nbytes = write(fd, "e", sizeof(char)); // escrever para fechar a thread
    pthread_join(readerThreadID, NULL);
    close(fd);
    close(fd_retorno);
    unlink(utilizador.pipename);

    wclear(eAux.janelaTopo);
    wrefresh(eAux.janelaTopo);
    delwin(eAux.janelaTopo);

    wclear(eAux.janelaBaixo);
    wrefresh(eAux.janelaBaixo);
    delwin(eAux.janelaBaixo);

    wclear(eAux.janelaLado);
    delwin(eAux.janelaLado);

    endwin();

    return 0;
}
