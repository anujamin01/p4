CC     := gcc
CFLAGS := -Werror -g

SRCS   := client.c \
	server.c \

#\
	mfs.c \
	mkfs.c \
	udp.c\

OBJS   := ${SRCS:c=o}
PROGS  := ${SRCS:.c=}

.PHONY: all
all: ${PROGS}

${PROGS} : % : %.o Makefile
	${CC} $< -o $@ udp.c mfs.c


lib:
	gcc -fPIC -c mfs.c -o mfs.o
	gcc -shared mfs.o udp.h udp.c -o libmfs.so

clean:
	rm -f ${PROGS} ${OBJS}

%.o: %.c Makefile
	${CC} ${CFLAGS} -c $<

#TODO: Makefile should look like this by the end of project \
\
all: mkfs server client client_lib \
\
mkfs: mkfs.c ufs.h \
>---gvv mkfs.c -o mkfs \
\
server: server.c ufs.h udp.h udp.c \
>--gvv server.c udp.c -o server \
\
client_lib: mfs_client.c udp.h udp.c \
>--gcc -fPIC -shared mfs_client.c udp.c -o libmfs.so \
\
client: client.c mfs.h \
>---gcc client.c libmfs.so -o client \
