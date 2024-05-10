#include "motor.h"
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

void enviaSinal(int pid, int sinal, int valor)
{
	union sigval sv;
	sv.sival_int = valor;
	sigqueue(pid, sinal, sv);
}
void *iniciabot(void *parametros)
{
	tDadosIniciaBot *tD = (tDadosIniciaBot *)parametros;
	int res, tam, status;
	int canal[2];
	char dados[200];
	pipe(canal);

	res = fork();
	if (res == 0)
	{
		fflush(stdout);
		close(canal[0]);
		dup2(canal[1], STDOUT_FILENO);
		execl("./bot", "bot", tD->intervalos.intervalo, tD->intervalos.duracaoBot, NULL);
		exit(7);
	}

	close(canal[1]);
	while (tD->intervalos.duracaoBot)
	{
		tam = read(canal[0], dados, sizeof(dados));
		if (tam > 0)
		{
			if (tD->motor->terminaThread == 0)
			{
				enviaSinal(res, SIGINT, 0);
				break;
			}
			dados[tam] = '\0';

			pthread_mutex_lock(tD->motor->mexerMapaUtilizador);
			Obstaculo o;
			o.temMovimento = false;
			sscanf(dados, "%d %d", &o.posicao->posX, &o.posicao->posY);
			if (tD->motor->mapa.mapaLab[o.posicao->posX][o.posicao->posY] == ' ')
			{
				for (int i = 0; tD->motor->mapa.pedras < MAX_PEDRAS; i++)
				{
					if (tD->motor->mapa.obstaculo + i == NULL)
					{
						tD->motor->mapa.mapaLab[o.posicao->posX][o.posicao->posY] = 'P';
						tD->motor->mapa.obstaculo[i] = o;
						tD->motor->mapa.pedras++;
					}
				}
			}
			enviaTodos(tD->motor, "Mapa Atualizado");

			pthread_mutex_unlock(tD->motor->mexerMapaUtilizador);
		}
	}
	waitpid(res, &status, 0);
	if (WIFEXITED(status))
	{
		printf("TERMINOU O BOT, CODIGO DE RETORNO: %d\n", WEXITSTATUS(status));
		fflush(stdout);
	}
}

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- VERIFCACAO PIPE CRIADO -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

void motorAberto(Motor *motor, char mapaLab[LINHASMAPA][COLUNASMAPA])
{

	int fifo_res;
	if (access(PIPESERVER, F_OK) == 0)
	{
		printf("[alarme||erro] - MOTOR JA ABERTO||OCORREU UM ERRO NA CRIACAO DO PIPE. NAO INICIE MAIS MOTORES\n");
		exit(5);
	}

	fifo_res = mkfifo(PIPESERVER, 0666);
	if (fifo_res == -1)
	{
		printf("[alarme||erro] - MOTOR JA ABERTO||OCORREU UM ERRO NA CRIACAO DO PIPE. NAO INICIE MAIS MOTORES\n");
		exit(5);
	}
	motor->fd_PIPESERVER = open(PIPESERVER, O_RDWR);
	if (motor->fd_PIPESERVER == -1)
	{
		printf("[alarme||erro] - MOTOR JA ABERTO||OCORREU UM ERRO NA CRIACAO DO PIPE. NAO INICIE MAIS MOTORES\n");
		exit(5);
	}
	ovar(&motor->v);

	for (int i = 0; i < LINHASMAPA; ++i)
	{
		for (int j = 0; j < COLUNASMAPA; ++j)
		{
			motor->mapa.mapaLab[i][j] = mapaLab[i][j];
		}
	}
	for (int i = 0; i < MAX_UTILIZADORES; i++)
	{
		motor->utilizadores[i].flagVazio = -1;
	}

	motor->mapa.bloqmovel = 0;
	motor->mapa.pedras = 0;

	motor->continua = 1;
	motor->flagTerminaAtualizacao = 1;

	pthread_mutex_t trinco;
	pthread_mutex_init(&trinco, NULL);
	motor->mexerMapaUtilizador = &trinco;

	printMapa(motor->mapa.mapaLab);
}

// = -= -= -= -= -= -= -= -= -= -= -FIM VERIFICACAO PIPE CRIADO -= -= -= -= -= -= -= -= -= -= -= -= -= -= - -
void readLab(char mapaLab[LINHASMAPA][COLUNASMAPA])
{
	char *file = "labirinto.txt";
	int fd;
	char c;
	int i = 0;
	int num_lines = 0;
	int bytes_read;
	int j;
	fd = open(file, O_RDONLY);
	if (fd == -1)
	{
		printf("[erro] - abertura do ficheiro do mapa\n");
		return;
	}

	while ((bytes_read = read(fd, &c, 1)) > 0 && c != EOF)
	{
		if (bytes_read != 0)
		{
			if (i == COLUNASMAPA && c != '\n')
			{
				printf("[erro] - abertura do ficheiro do mapa - Formato Errado\n");
				exit(12);
			}
			if (i == COLUNASMAPA + 1)
			{
				i = 0;
				num_lines++;
			}
			if (num_lines == LINHASMAPA)
			{
				printf("[info] - labirinto lido\n");
				break;
			}
		}
		if (c != ' ' && c != 'x' && c != '\n')
		{
			printf("[erro] - abertura do ficheiro do mapa - Formato Errado\n");
			exit(12);
		}
		mapaLab[num_lines][i] = c;
		i++;
	}
	close(fd);
}
void printMapa(char mapaLab[LINHASMAPA][COLUNASMAPA])
{
	for (int i = 0; i < LINHASMAPA; i++)
	{
		for (int j = 0; j < COLUNASMAPA; j++)
		{
			printf("%c", mapaLab[i][j]);
		}
		printf("\n");
	}
}

void ovar(VarAmbiente *vars)
{
	// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- VARIAVEIS DE AMBIENTE -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

	char *inscricao = getenv("INSCRICAO");
	char *nplayers = getenv("NPLAYERS");
	char *duracao = getenv("DURACAO");
	char *decremento = getenv("DECREMENTO");
	bool varOk = true;

	if (inscricao == NULL || (vars->inscricao = atoi(inscricao)) == 0)
	{
		printf("[erro variavel] - Erro na variavel de ambiente - INSCRICAO. Impossivel iniciar o motor\n");
		varOk = false;
	}
	if (nplayers == NULL || (vars->nplayers = atoi(nplayers)) == 0)
	{
		printf("[erro variavel] - Erro na variavel de ambiente - NPLAYERS. Impossivel iniciar o motor\n");
		varOk = false;
	}
	if (duracao == NULL || (vars->duracao = atoi(duracao)) == 0)
	{
		printf("[erro variavel] - Erro na variavel de ambiente - DURACAO. Impossivel iniciar o motor\n");
		varOk = false;
	}
	if (decremento == NULL || (vars->decremento = atoi(decremento)) == 0)
	{
		printf("[erro variavel] - Erro na variavel de ambiente - DECREMENTO. Impossivel iniciar o motor\n");
		varOk = false;
	}
	if (!varOk)
	{
		unlink(PIPESERVER);
		exit(7);
	}

	// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- FIM VARIAVEIS DE AMBIENTE -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
}

void atualizaMapaJogador(Utilizador utilizador, Motor *motor)
{

	motor->mapa.mapaLab[utilizador.posicao->posX][utilizador.posicao->posY] = utilizador.letraR;
	for (int i = 0; i < motor->lastIndexAdd; i++)
	{
		if (motor->utilizadores[i].nome == utilizador.nome)
		{
			motor->utilizadores[i].posicao->posX = utilizador.posicao->posX;
			motor->utilizadores[i].posicao->posY = utilizador.posicao->posY;
		}
	}
}

void mexeMapaBMeP(Motor *motor)
{
	Obstaculo o;
	pthread_mutex_lock(motor->mexerMapaUtilizador);

	for (int i = 0; i < sizeof(motor->mapa.obstaculo); i++)
	{
		if (motor->mapa.obstaculo[i].temMovimento)
		{
			o = motor->mapa.obstaculo[i];
			switch (rand() % 4 + 1)
			{
			case 1: // up
				o.posicao->posY = o.posicao->posY + 1;
				break;
			case 2: // down
				o.posicao->posY = o.posicao->posY - 1;
				break;
			case 3: // left
				o.posicao->posX = o.posicao->posX - 1;
				break;
			case 4: // right
				o.posicao->posX = o.posicao->posX + 1;
				break;
			default:
				break;
			}
			if (motor->mapa.mapaLab[o.posicao->posX][o.posicao->posY] == ' ')
			{
				motor->mapa.mapaLab[motor->mapa.obstaculo[i].posicao->posX][motor->mapa.obstaculo[i].posicao->posY] = ' ';
				motor->mapa.obstaculo[i] = o;
				motor->mapa.mapaLab[motor->mapa.obstaculo[i].posicao->posX][motor->mapa.obstaculo[i].posicao->posY] = 'B';
			}
		}
		else
		{
			if (--motor->mapa.obstaculo[i].duracao == 0)
			{
				motor->mapa.mapaLab[motor->mapa.obstaculo[i].posicao->posX][motor->mapa.obstaculo[i].posicao->posY] = ' ';
			}
		}
	}
	enviaTodos(motor, "Mapa Atualizado");

	pthread_mutex_unlock(motor->mexerMapaUtilizador);
}
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- TRATAMENTO DO THREAD -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

void *gestaoMapa(void *dados)
{
	Motor *motor = (Motor *)dados;
	while (motor->flagTerminaAtualizacao == 1)
	{
		sleep(5);
		mexeMapaBMeP(motor);
	}
}

void enviaTodos(Motor *motor, const char *message)
{

	for (int i = 0; i < motor->lastIndexAdd; i++)
	{
		int fd_client = open(motor->utilizadores[i].pipename, O_WRONLY);
		if (fd_client != -1)
		{
			strcpy(motor->utilizadores[i].msg, message);

			write(fd_client, &motor->utilizadores[i], sizeof(Utilizador));

			close(fd_client);
		}
	}
}

bool giro = true;
int alarmNumber = 0;

void sig_handler(int signum)
{
	giro = false;
	printf("[aviso] - tempo de inscricao acabou\n");
	fflush(stdout);
}

void *trataThread(void *dados)
{
	int nbytes, fd, fd_client;
	char mapaLab[LINHASMAPA][COLUNASMAPA];
	Motor *motor = (Motor *)dados;
	Utilizador utilizador = {};
	Obstaculo o;

	Intervalos pB[] = {
		("30", "10"),
		("25", "5"),
		("30", "15"),
		("25", "10"),
		("20", "5"),
		("30", "20"),
		("25", "15"),
		("20", "10"),
		("15", "5"),
	};

	tDadosIniciaBot tD[9];
	for (int i = 0; i < sizeof(tD); i++)
	{
		tD[i].intervalos = pB[i];
		tD[i].motor = motor;
	}

	signal(SIGALRM, sig_handler);
	alarm(motor->v.inscricao);
	do
	{
		// ENQUANTO O TEMPO DE INSCRIÇÃO NÃO ACABAR ACRESCENTA JOGADORES
		if (giro)
		{
			nbytes = read(motor->fd_PIPESERVER, &utilizador, sizeof(Utilizador));
			if (nbytes > 0)
			{
				int i;

				for (i = 0; i < MAX_UTILIZADORES; i++)
				{
					if (motor->utilizadores[i].flagVazio == -1 && giro)
					{
						printf("NOME: %s\n%s\n%c\n", utilizador.nome, utilizador.pipename, utilizador.letraR);
						printf("INSCREVI O JOGADOR: %s\n", utilizador.nome);
						fflush(stdout);

						utilizador.mapa = motor->mapa;

						fd = open(utilizador.pipename, O_WRONLY);
						write(fd, &utilizador, sizeof(Utilizador));
						close(fd);
						motor->utilizadores[i] = utilizador;
						break;
					}
				}
				if (i >= MAX_UTILIZADORES)
				{
					fd = open(utilizador.pipename, O_WRONLY);
					strcpy(utilizador.msg, "SERVIDOR CHEIO");
					write(fd, &utilizador, sizeof(Utilizador));
					close(fd);
				}
			}
		}
		else
		{
			alarmNumber++;
			// TODO: MANDAR MENSAGEM PARA TODOS OS JOGADORES A DIZER SEI LA
			for (int i = 0; i < sizeof(motor->utilizadores); i++)
			{
				int fd_client = open(motor->utilizadores[i].pipename, O_WRONLY);
				if (fd_client != -1)
				{
					strcpy(motor->utilizadores[i].msg, "LANÇAMENTO DE BOTS\n");
					write(fd_client, &motor->utilizadores[i], sizeof(Utilizador));
					close(fd_client);
				}
			}

			/*
			Cada nível tem um tempo máximo para jogar. O primeiro jogador a chegar ao ponto de chegada é o vencedor. Se nenhum
			jogador atingir esse ponto dentro do tempo especificado para esse nível, todos os jogadores perdem, terminando o jogo, sendo
			acionadas as ações associadas ao fim de jogo

			O primeiro nível demora uma quantidade de segundos indicada na variável de ambiente DURACAO. Cada nível
			seguinte demora menos DECREMENTO (outra variável de ambiente) segundos que o anterior.

			Nível 1 – Existem 2 bots:
				Bot 1 - Envios de coordenadas a cada 30 segundos e duração de permanência de pedras de 10 segundos;
				Bot 2 - Envios de coordenadas a cada 25 segundos e duração de permanência de pedras de 5 segundos.

			Nível 2 – Existem 3 bots:
				Bot 1 - Envios de coordenadas a cada 30 segundos e duração de permanência de pedras de 15 segundos;
				Bot 2 - Envios de coordenadas a cada 25 segundos e duração de permanência de pedras de 10 segundos;
				Bot 3 - Envios de coordenadas a cada 20 segundos e duração de permanência de pedras de 5 segundos.

			Nível 3 – Existem 4 bots:
				Bot 1 - Envios de coordenadas a cada 30 segundos e duração de permanência de pedras de 20 segundos;
				Bot 2 - Envios de coordenadas a cada 25 segundos e duração de permanência de pedras de 15 segundos;
				Bot 3 - Envios de coordenadas a cada 20 segundos e duração de permanência de pedras de 10 segundos;
				Bot 4 - Envios de coordenadas a cada 15 segundos e duração de permanência de pedras de 5 segundos;

			No final de cada nível, os bots devem ser terminados pelo motor, e novos bots devem ser lançados, seguindo a distribuição
			previamente mencionada.
			*/

			// JOGO / TRATAMERNTO DOS COMANDOS

			if (motor->nivelAtual == 1)
			{
				// SETAR ALARM SEMPRE MAL COMEÇA A PREPARAR OS BOTS

				// BOT 1
				if (pthread_create(&motor->threadsBot[motor->lastIndexAdd++], NULL, iniciabot, &tD[0]))
				{
					printf("OCOREU UM ERRO AO CRIAR A THREAD");
					return 0;
				}
				// BOT 2
				if (pthread_create(&motor->threadsBot[motor->lastIndexAdd++], NULL, iniciabot, &tD[1]))
				{
					printf("OCOREU UM ERRO AO CRIAR A THREAD");
					return 0;
				}
			}

			else if (motor->nivelAtual == 2)
			{
				// BOT 1
				if (pthread_create(&motor->threadsBot[motor->lastIndexAdd++], NULL, iniciabot, &tD[2]))
				{
					printf("OCOREU UM ERRO AO CRIAR A THREAD");
					return 0;
				}

				// BOT 2
				if (pthread_create(&motor->threadsBot[motor->lastIndexAdd++], NULL, iniciabot, &tD[3]))
				{
					printf("OCOREU UM ERRO AO CRIAR A THREAD");
					return 0;
				}

				// BOT 3
				if (pthread_create(&motor->threadsBot[motor->lastIndexAdd++], NULL, iniciabot, &tD[4]))
				{
					printf("OCOREU UM ERRO AO CRIAR A THREAD");
					return 0;
				}
			}
			else if (motor->nivelAtual == 3)
			{
				// BOT 1
				if (pthread_create(&motor->threadsBot[motor->lastIndexAdd++], NULL, iniciabot, &tD[5]))
				{
					printf("OCOREU UM ERRO AO CRIAR A THREAD");
					return 0;
				}

				// BOT 2
				if (pthread_create(&motor->threadsBot[motor->lastIndexAdd++], NULL, iniciabot, &tD[6]))
				{
					printf("OCOREU UM ERRO AO CRIAR A THREAD");
					return 0;
				}

				// BOT 3
				if (pthread_create(&motor->threadsBot[motor->lastIndexAdd++], NULL, iniciabot, &tD[7]))
				{
					printf("OCOREU UM ERRO AO CRIAR A THREAD");
					return 0;
				}

				// BOT 4
				if (pthread_create(&motor->threadsBot[motor->lastIndexAdd++], NULL, iniciabot, &tD[8]))
				{
					printf("OCOREU UM ERRO AO CRIAR A THREAD");
					return 0;
				}
			}
			alarm(motor->v.duracao);

			if (pthread_create(&motor->threadsBot[motor->lastIndexAdd++], NULL, gestaoMapa, &motor))
			{
				printf("OCOREU UM ERRO AO CRIAR A THREAD");
				return 0;
			}

			while (alarm(0) != 0)
			{
				int bytesRead = read(fd_client, &utilizador, sizeof(Utilizador));
				if (bytesRead > 0)
				{
					if (strcasecmp(utilizador.msg, "players\n") == 0)
					{
						for (int i = 0; i < motor->lastIndexAdd + 1; i++)
						{
							if (strcmp(utilizador.nome, "") != 0)
							{
								strcat(utilizador.msg, strcat(utilizador.nome, "\n"));
							}
						}
						fd_client = open(utilizador.pipename, O_WRONLY);
						for (int i = 0; i < sizeof(motor->utilizadores); i++)
						{
							write(fd_client, &motor->utilizadores[i], sizeof(Utilizador));
						}
						close(fd_client);
					}
					else if (strcasecmp(utilizador.msg, "q") == 0)
					{
						for (int i = 0; i < motor->lastIndexAdd + 1; i++)
						{
							if (strcasecmp(utilizador.nome, motor->utilizadores[i].nome) == 0) // checka se o destinatario existe
							{
								pthread_mutex_lock(motor->mexerMapaUtilizador);
								motor->utilizadores[i].flagVazio = -1;
								pthread_mutex_unlock(motor->mexerMapaUtilizador);
							}
						}
					}
					else if (strcasecmp(utilizador.msg, "msg") == 0)
					{
						char *secondWord, *thirdWord;
						secondWord = strtok(NULL, " ");
						thirdWord = strtok(NULL, "\n");

						if (secondWord != NULL && thirdWord != NULL)
						{
							for (int i = 0; i < motor->lastIndexAdd + 1; i++)
							{
								if (strcasecmp(secondWord, motor->utilizadores[i].nome) == 0) // checka se o destinatario existe
								{
									snprintf(motor->utilizadores[i].msg, sizeof(Utilizador), "<%s> <%s>: %s\n", motor->utilizadores->nome, secondWord, thirdWord); // constroi a mensagem

									fd_client = open(motor->utilizadores[i].pipename, O_WRONLY);
									if (fd_client != -1)
									{
										for (int i = 0; i < sizeof(motor->utilizadores); i++)
										{
											write(fd_client, &motor->utilizadores[i], sizeof(Utilizador));
										}

										close(fd_client);
									}
									else
									{
										printf("[erro] - Impossivel enviar a mensagem. Pipe não encontrado\n");
									}
								}
							}
						}
						else
						{
							write(fd_client, "[erro] - Syntax: msg <destinatario> <mensagem>\n", sizeof("[erro] - Syntax: msg <destinatario> <mensagem>\n"));
							close(fd_client);
						}
					}

					fd_client = open(utilizador.pipename, O_WRONLY);
					if (strcmp(utilizador.msg, "u") == 0) // u = up
					{
						if (mapaLab[utilizador.posicao->posX][utilizador.posicao->posY + 1] != ' ')
						{
							continue;
						}
						else
						{
							utilizador.posicao->posY = utilizador.posicao->posY + 1;

							pthread_mutex_lock(motor->mexerMapaUtilizador);

							atualizaMapaJogador(utilizador, motor);
							utilizador.mapa = motor->mapa;
							for (int i = 0; i < sizeof(motor->utilizadores); i++)
							{
								write(fd_client, &motor->utilizadores[i], sizeof(Utilizador));
							}

							pthread_mutex_unlock(motor->mexerMapaUtilizador);
							close(fd_client);
						}
					}
					else if (strcmp(utilizador.msg, "d") == 0) // d = down
					{
						if (mapaLab[utilizador.posicao->posX][utilizador.posicao->posY - 1] != ' ')
						{
							continue;
						}
						else
						{
							utilizador.posicao->posY = utilizador.posicao->posY - 1;

							pthread_mutex_lock(motor->mexerMapaUtilizador);

							atualizaMapaJogador(utilizador, motor);
							utilizador.mapa = motor->mapa;
							for (int i = 0; i < sizeof(motor->utilizadores); i++)
							{
								write(fd_client, &motor->utilizadores[i], sizeof(Utilizador));
							}

							pthread_mutex_unlock(motor->mexerMapaUtilizador);
							close(fd_client);
						}
					}
					else if (strcmp(utilizador.msg, "l") == 0) // l = left
					{
						if (mapaLab[utilizador.posicao->posX - 1][utilizador.posicao->posY] != ' ')
						{
							continue;
						}
						else
						{
							utilizador.posicao->posX = utilizador.posicao->posX - 1;

							pthread_mutex_lock(motor->mexerMapaUtilizador);

							atualizaMapaJogador(utilizador, motor);
							utilizador.mapa = motor->mapa;
							write(fd_client, &utilizador, sizeof(Utilizador));

							pthread_mutex_unlock(motor->mexerMapaUtilizador);
							close(fd_client);
						}
					}
					else if (strcmp(utilizador.msg, "r") == 0) // r = right
					{
						if (mapaLab[utilizador.posicao->posX + 1][utilizador.posicao->posY] != ' ')
						{
							continue;
						}
						else
						{
							utilizador.posicao->posX = utilizador.posicao->posX + 1;

							pthread_mutex_lock(motor->mexerMapaUtilizador);

							atualizaMapaJogador(utilizador, motor);
							utilizador.mapa = motor->mapa;
							for (int i = 0; i < sizeof(motor->utilizadores); i++)
							{
								write(fd_client, &motor->utilizadores[i], sizeof(Utilizador));
							}

							pthread_mutex_unlock(motor->mexerMapaUtilizador);
							close(fd_client);
						}
					}
				}
			}
			motor->terminaThread = 0;
			motor->flagTerminaAtualizacao = 0;
			for (int i = 0; i < sizeof(motor->lastIndexAdd); i++)
			{
				pthread_join(motor->threadsBot[i], NULL);
			}
			motor->nivelAtual++;
		}
	} while (motor->continua);
	return NULL;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- FIM TRATAMENTO DO THREAD -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

int main(int argc, char *argv[])
{
	char comando[TAMCOMANDOS];
	char *firstWord, *secondWord;
	char *intervalo;
	char *duracaoBot;
	bool correuBem;
	int fd;
	pthread_t tid;

	Motor motor;
	Utilizador utilizador;
	ParametrosBot pB;
	char mapaLab[LINHASMAPA][COLUNASMAPA];

	//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- TRATAMENTO DO MAPA -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	readLab(mapaLab);
	//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- FIM TRATAMENTO MAPA -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

	motorAberto(&motor, mapaLab);

	if (pthread_create(&tid, NULL, trataThread, &motor) != 0)
	{
		printf("[erro] - ocorreu um erro ao criar a thread\n");
		unlink(PIPESERVER);
		exit(3);
	}
	//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- COMANDOS -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	do
	{
		correuBem = false;
		firstWord = NULL;
		secondWord = NULL;

		printf("_>");
		fgets(comando, sizeof(comando), stdin);
		firstWord = strtok(comando, " ");
		// KICK DE JOGADORES
		if (strcasecmp(firstWord, "kick") == 0)
		{
			secondWord = strtok(NULL, "\n");
			if (firstWord == NULL || secondWord == NULL || strcmp(secondWord, " ") == 0)
			{
				printf("COMANDO MAL ESCRITO\n");
				correuBem = false;
			}
			else
				correuBem = true;

			if (correuBem)
			{
				for (int i = 0; i < sizeof(motor.utilizadores); i++)
				{
					if (strcasecmp(secondWord, motor.utilizadores[i].nome) == 0)
					{
						strcpy(motor.utilizadores[i].msg, "KICKADO");
						fd = open(motor.utilizadores[i].pipename, O_WRONLY);
						write(fd, &motor.utilizadores[i], sizeof(Utilizador));
						close(fd);
						motor.utilizadores[i].flagVazio == -1;
						break;
					}
				}
				enviaTodos(&motor, strcat(secondWord, " FOI BANIDO"));
			}
		}
		// LISTAR JOGADORES
		// SUBSTITUIR POR FOR A PRECORRER LISTA DE UTILIZADORES E COBRIR COM MUTEX ATÈ AO FIM DO COMANDO
		if (strcasecmp(firstWord, "users\n") == 0)
		{

			if (firstWord == NULL)
			{
				correuBem = false;
			}
			else
				correuBem = true;
			if (correuBem)
			{
				pthread_mutex_lock(motor.mexerMapaUtilizador);

				printf("LISTA DE PLAYERS: \n");
				for (int i = 0; i < motor.lastIndexAdd; i++)
				{
					printf("%s: %s\n", motor.utilizadores[i].nome, motor.utilizadores[i].pipename);
				}

				pthread_mutex_unlock(motor.mexerMapaUtilizador);
			}
		}

		if (strcasecmp(firstWord, "begin\n") == 0)
		{

			if (firstWord == NULL)
			{
				correuBem = false;
			}
			else
				correuBem = true;
			if (correuBem)
			{
				pthread_mutex_lock(motor.mexerMapaUtilizador);

				if (motor.v.nplayers >= 0)
				{
					printf("[aviso] - jogo ja iniciado\n");
				}
				else
				{
					for (int i = 0; i < sizeof(motor.utilizadores); i++)
					{
						if (strcasecmp(secondWord, motor.utilizadores[i].nome) == 0)
						{
							fd = open(motor.utilizadores[i].pipename, O_WRONLY);
							trataThread(&motor);
							write(fd, &motor.utilizadores[i], sizeof(Utilizador));
							close(fd);
							motor.utilizadores[i].flagVazio == -1;
							break;
						}
					}

					enviaTodos(&motor, "[info] - jogo iniciado manualmente");
				}

				pthread_mutex_unlock(motor.mexerMapaUtilizador);
			}
		}

		// LISTAR BOTS
		if (strcasecmp(firstWord, "bots\n") == 0)
		{

			if (firstWord == NULL)
			{
				correuBem = false;
			}
			else
				correuBem = true;
			if (correuBem)
			{
				printf("LISTA DE BOTS: \n");
				for (int i = 0; i < sizeof(motor.mapa.obstaculo); i++)
				{
					if (motor.mapa.obstaculo[i].temMovimento)
					{
						printf("B.Mov na posicao: %d %d\n", motor.mapa.obstaculo[i].posicao->posX, motor.mapa.obstaculo[i].posicao->posY);
					}
					else
					{
						printf("Pedra na posicao: %d %d\n", motor.mapa.obstaculo[i].posicao->posX, motor.mapa.obstaculo[i].posicao->posY);
					}
				}
			}
		}
		// INSERIR BLOQUEIO MOVEL
		// INSERIR NO MAPA BLOQUEI MOVEL COM CONDIÇOES QUE EXISTEM NO ENUNCIADO MUTEX PARA COBRIR INSERt
		if (strcasecmp(firstWord, "bmov\n") == 0)
		{
			if (firstWord == NULL)
			{
				correuBem = false;
			}
			else
				correuBem = true;
			if (correuBem)
			{

				printf("BLOQUEIO MOVEL - INSERIDO\n");

				pthread_mutex_lock(motor.mexerMapaUtilizador);
				Obstaculo o;
				o.temMovimento = true;
				o.duracao = 0;
				o.flagVazio = 1;
				do
				{
					o.posicao->posX = rand() % LINHASMAPA;
					o.posicao->posY = rand() % COLUNASMAPA;
				} while (motor.mapa.mapaLab[o.posicao->posX][o.posicao->posY] != ' ');

				for (int i = 0; motor.mapa.bloqmovel < MAX_BLOQMOVEL; i++)
				{
					if (motor.mapa.obstaculo[i].flagVazio == -1)
					{
						motor.mapa.mapaLab[o.posicao->posX][o.posicao->posY] = 'B';
						motor.mapa.obstaculo[i] = o;
						motor.mapa.bloqmovel++;
					}
				}

				pthread_mutex_unlock(motor.mexerMapaUtilizador);
			}
		}
		// REMOVER BLOQUEIO MOVEL
		// REMOVER NO MAPA BLOQUEI MOVEL COM CONDIÇOES QUE EXISTEM NO ENUNCIADO MUTEX PARA COBRIR INSERt
		if (strcasecmp(firstWord, "rbm\n") == 0)
		{
			if (firstWord == NULL)
			{
				correuBem = false;
			}
			else
				correuBem = true;
			if (correuBem)
			{
				pthread_mutex_lock(motor.mexerMapaUtilizador);
				for (int i = 0; i < MAX_BLOQMOVEL; i++)
				{
					motor.mapa.obstaculo[i].flagVazio = -1;

					motor.mapa.bloqmovel--;
					break;
				}
				pthread_mutex_unlock(motor.mexerMapaUtilizador);
				printf("BLOQUEIO MOVEL - REMOVIDO\n");
			}
		}
	} while (strcasecmp(comando, "end\n") != 0);

	pB.flagContinua = 0;

	for (int i = 0; i < NUM_THREADS; i++) //	ESPERAR PELO ENCERRAR DE TODAS AS THREADS
	{
		pthread_join(motor.threadsBot[i], NULL);
	}

	//============================================================================

	enviaTodos(&motor, "JOGO TERMINADO");
	motor.continua = 0;
	pthread_join(tid, NULL);

	//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- FIM COMANDOS -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	unlink(PIPESERVER);
	printf("\t\t[aviso]\n JOGO TERMINADO - PROGRAMA VAI ENCERRAR\n");
	return 0;
}