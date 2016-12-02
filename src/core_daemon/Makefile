
TARGET=core_daemon
SRC=$(wildcard *.c)
OBJS=${SRC:%.c=%.o}
NAME=${SRC:%.c=%}
DEPS=$(SRC:%.c=.dep/*.d)
CFLAGS+= `pkg-config --cflags libxml-2.0`
LDFLAGS+= `pkg-config --libs libxml-2.0` # -lm -lz -llzma -static

${TARGET}:  ${OBJS} $(HSB_LIBS)
	${CC}  -o $@ ${OBJS} ${LDFLAGS}

-include ${DEPS}

.PHONY: dep 

all :  ${TARGET} 
	cp $(TARGET) ${EXEDIR}
	
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

