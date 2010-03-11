CFLAGS=-g -I.
LDFLAGS=-levent

SRCS=logging.c \
	 buf.c \
	 req.c \
	 parser.c \
	 conn.c

OBJS=$(SRCS:.c=.o)

.phony: all clean

all: meepmeep

meepmeep: ${OBJS}

clean:
	rm *.o
	rm meepmeep

include $(SRCS:.c=.d)
