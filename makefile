CC=gcc
CFLAGS=-lncurses
all: jogoUI.c motor.c bot.c
	$(CC) jogoUI.c -o jogoUI $(CFLAGS)
	$(CC) motor.c -o motor -lpthread
	$(CC) bot.c -o bot $(CFLAGS)

jogoUI: jogoUI.c jogoUI.h
	$(CC) jogoUI.c -o jogoUI $(CFLAGS) -lpthread

motor: motor.c motor.h
	$(CC) motor.c -o motor -lpthread

bot: bot.c
	$(CC) bot.c -o bot $(CFLAGS)

clean:
	rm -f all jogoUI motor bot
