mpicc -std=gnu99 -Wall -fopenmp -I../include -o test_234byte  test_234byte.c  -lm -ljpeg ../lib/lib234comp.a
mpicc -std=gnu99 -Wall -fopenmp -I../include -o test_234float test_234float.c -lm -ljpeg ../lib/lib234comp.a
