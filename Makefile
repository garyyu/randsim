  ############################################################################
  ## 
  ##  Makefile  
  ## 
  ##########################################################################*

include   makefile.pre

EXECUTABLE = randsim-sse

OBJS = \
 os_wrapper.o  \
 SFMT.o \
 main.o

ifeq (a$(sse), a)
	SSE2 = $(SSE2FLAGS)
endif

ifeq (a$(sse), a1)
	SSE2 = $(SSE2FLAGS)
endif

ifeq (a$(sse), a0)
	SSE2 = 
endif

all:	$(EXECUTABLE)

clean: 
	@echo Build clean, all the object files are removed successfully.
	@$(RM) *.o $(OBJPATH)/*.o *~

cleanall:
	@echo Build clean all, all the object files and binaries are removed successfully.
	@$(RM) randsim-* *.o $(OBJPATH)/*.o *~

randsim-sse:  $(OBJS) 
		$(CPP) $(CFLAGS) $(SSE2) -DSFMT_MEXP=19937  $(OBJS) $(LIBS) -o $@
		make clean
		
randsim-std:  $(OBJS) 
		$(CPP) $(CFLAGS) $(SSE2) -DSFMT_MEXP=19937  $(OBJS) $(LIBS) -o $@
		make clean	

%.o:	 %.cpp $(HEADERS)
		@$(RM) $@
		@echo $(shell pwd)/$<
		@$(CPP) $(CFLAGS) $(SSE2) -DSFMT_MEXP=19937 $(INCLUDE) $(CLINK) $@ $(DPARAM) $(INCLUDE) $<

