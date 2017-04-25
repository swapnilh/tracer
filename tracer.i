/* File: tracer.i */
%module tracer

%{
#define SWIG_FILE_WITH_INIT
#include "tracer.h"
%}

void start_pin_tracing();
void stop_pin_tracing();
void start_perf_tracing();
void stop_perf_tracing();

