CC=gcc
CFLAGS=-I$(IDIR)
OUTDIR_DEBUG=dist/Debug/GNU-Linux-x86/
OUTDIR_RELEASE=dist/Release/GNU-Linux-x86/
LDIR =/data/lib

LIBS= -lpthread -lgearman -lavcodec -lavformat -lavresample -lavutil -lswresample -lavfilter -lswscale -lm

all:
	mkdir -p $(OUTDIR_DEBUG)
	$(CC) -c ini.c -o ini.o
	$(CC) -c libftp/ftplib.c -o ftplib.o
	$(CC) -c mp4_encoder.c -o mp4_encoder.o
	$(CC) -c worker.c -o worker.o
	$(CC) worker.o ini.o mp4_encoder.o ftplib.o -o $(OUTDIR_DEBUG)/mp4encoder $(LIBS)
	rm -f *.o


debug:
	mkdir -p $(OUTDIR_DEBUG)
	$(CC) -ggdb -g3 -c ini.c -o ini.o
	$(CC) -ggdb -g3 -c mp4_encoder.c -o mp4_encoder.o
	$(CC) -ggdb -g3 -c worker.c -o worker.o
	$(CC) -ggdb -g3 worker.o ini.o mp4_encoder.o -o $(OUTDIR_DEBUG)/mp4encoder $(LIBS)
	rm -f *.o



clean:
	rm -f *.o
	rm -f $(OUTDIR)/mp4encoder 
