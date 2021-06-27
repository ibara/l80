# l80 Makefile

all:
	${MAKE} -C source

c:
	${CC} -O2 -pipe -o l80 l80.c

cpm:
	ack -mcpm -O6 -o ld.com l80.c

dos:
	wcl -0 -ox -mt l80.c

clean:
	${MAKE} -C source clean
	rm -f ld.com l80.o l80.obj
