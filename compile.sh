swig -python tracer.i
gcc -O2 -fPIC -c tracer.c
gcc -O2 -fPIC -c tracer_wrap.c -I/usr/include/python2.7
gcc -shared tracer.o tracer_wrap.o -o _tracer.so
