
TARGET=xml_parse xml_create

SRC=$(wildcard *.c)
OBJS=${SRC:%.c=%.o}
NAME=${SRC:%.c=%}
DEPS=$(SRC:%.c=.dep/*.d)
CFLAGS= `pkg-config --cflags libxml-2.0`
LDFLAGS= `pkg-config --libs libxml-2.0`

-include ${DEPS}

.PHONY: dep  all

all :  ${TARGET} 
	#cp ${TARGET} ${EXEDIR}

xml_parse : xml_parse.o
	$(CC) -o $@ $< $(LDFLAGS)

xml_create : xml_create.o
	$(CC) -o $@ $< $(LDFLAGS)

%.o: %.c
	${CC} ${CFLAGS} -c $<
	@mkdir -p .dep
	${CC} -MM $(CFLAGS) $*.c > .dep/$*.d 

clean:
	@rm -fr ${TARGET} *.o core .dep


dep: 
	@mkdir -p .dep
	for i in ${NAME} ; do  \
	${CC} -MM $(CFLAGS) "$${i}".c > .dep/"$${i}".d ;\
	done

