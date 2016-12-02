
SRC=$(wildcard *.c)
OBJS=${SRC:%.c=%.obj}
NAME=${SRC:%.c=%}
DEPS=$(SRC:%.c=.dep/%.d)

-include ${DEPS}

.PHONY: dep  all

all :  $(OBJS)
	
%.obj: %.c
	${CC} ${CFLAGS} -c $< -o $@
	@mkdir -p .dep
	${CC} -MM $(CFLAGS) $*.c > .dep/$*.d
	@mv -f .dep/$*.d  .dep/$*.d.tmp
	@sed -e 's/\.o/.obj/' .dep/$*.d.tmp > .dep/$*.d 
	rm -f .dep/$*.d.tmp

clean:
	@rm -fr *.o core .dep *.obj

dep: 
	@mkdir -p .dep
	for i in ${NAME} ; do  \
	${CC} -MM $(CFLAGS) "$${i}".c > .dep/"$${i}".d ;  \
	mv -f .dep/$${i}.d  .dep/$${i}.d.tmp ;\
	sed -e 's/\.o/.obj/' .dep/$${i}.d.tmp > .dep/$${i}.d ; \
	rm -f .dep/$${i}.d.tmp ;\
	done

