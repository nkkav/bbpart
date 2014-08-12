PASS =		bbpart

OBJS =		bbpart.o suif_pass.o
MAIN_OBJ =	suif_main.o
CPPS =		$(OBJS:.o=.cpp) $(MAIN_OBJ:.o=.cpp)
HDRS =		$(OBJS:.o=.h)

#NWHDRS =
#NWCPPS =

#LIBS =		-lmachine -lcfg -lbvd -larm
LIBS =		-lmachine -lcfg -lbvd

include $(MACHSUIFHOME)/Makefile.common

