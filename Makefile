# various options to control compilation
#  you can combine these for different results

# clear any existing CFLAGS first
CFLAGS=
# architecture of machine to run on
CFLAGS+=-march=native
# optimization (-O2, -Ofast, etc)
CFLAGS+=-O2
# loop vectorization with reports
#CFLAGS+=-Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize
# link time optimization
#CFLAGS+=-flto
# strip result
#CFLAGS+=-s

all:	main.c
	$(CC) $(CFLAGS) -Wall -o integer-scaling main.c

source:	main.c
	$(CC) $(CFLAGS) -S main.c

clean:
	rm -f integer-scaling *.o *.s
