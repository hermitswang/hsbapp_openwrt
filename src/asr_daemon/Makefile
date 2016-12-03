
TARGET=asr_daemon
SRC=$(wildcard *.c)
OBJS=${SRC:%.c=%.o}
NAME=${SRC:%.c=%}
DEPS=$(SRC:%.c=.dep/*.d)
CFLAGS+= -I./lib/inc
LDFLAGS+= -L./lib -lmsc

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
	@rm -fr msc

dep: 
	@mkdir -p .dep
	for i in ${NAME} ; do  \
	${CC} -MM $(CFLAGS) "$${i}".c > .dep/"$${i}".d ;\
	done

