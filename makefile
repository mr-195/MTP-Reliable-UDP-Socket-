RM = rm -f
LIBNAME = libmsock.a
OBJFILES = mysock.o
SOURCEFILES = mysock.c

library: $(OBJFILES)
	ar rcs $(LIBNAME) $(OBJFILES)
$(OBJFILES): mysock.c mysock.h
	gcc -c $(SOURCEFILES) -lpthread
clean:
	-$(RM) $(OBJFILES)
distclean:
	-$(RM) $(OBJFILES) $(LIBNAME)