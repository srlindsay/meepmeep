CPPFLAGS=-I.
CFLAGS=-g -I.
LDFLAGS=-levent

SRCS=logging.c \
	 buf.c \
	 http.c \
	 req.c \
	 parser.c \
	 proxy.c \
	 conn.c

.phony: all clean tags depend

all: meepmeep

tags:
	ctags -R *

meepmeep: ${SRCS:.c=.o}

depend: ${SRCS:.c=.d}

%.d: %.c
	@$(SHELL) -ec 'gcc -MM $(CPPFLAGS) $< | sed "s/$*\.o[ :]*/$@ &/g" > $@'

clean:
	rm *.o
	rm meepmeep

include $(SRCS:.c=.d)
