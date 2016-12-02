
TARGET=un_send device_sim pad_sim smart_config udp_listen zigbee_sim unix_send serial_send # switch_probe

SRC=$(wildcard *.c)
OBJS=${SRC:%.c=%.o}
NAME=${SRC:%.c=%}
DEPS=$(SRC:%.c=.dep/*.d)


un_send : un_send.o $(HSB_LIBS)
	$(CC) -o $@ $< $(LDFLAGS) 

pad_sim : pad_sim.o $(HSB_LIBS)
	$(CC) -o $@ $< $(LDFLAGS) 

device_sim : device_sim.o $(HSB_LIBS)
	$(CC) -o $@ $< $(LDFLAGS) 

switch_probe : switch_probe.o $(HSB_LIBS)
	$(CC) -o $@ $< $(LDFLAGS) 

smart_config : smart_config.o $(HSB_LIBS)
	$(CC) -o $@ $< $(LDFLAGS)

udp_listen : udp_listen.o $(HSB_LIBS)
	$(CC) -o $@ $< $(LDFLAGS)

zigbee_sim : zigbee_sim.o $(HSB_LIBS)
	$(CC) -o $@ $< $(LDFLAGS)

unix_send : unix_send.o $(HSB_LIBS)
	$(CC) -o $@ $< $(LDFLAGS)

serial_send: serial_send.o $(HSB_LIBS)
	$(CC) -o $@ $< $(LDFLAGS)

-include ${DEPS}

.PHONY: dep  all

all :  ${TARGET} 
	cp ${TARGET} ${EXEDIR}
	
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

