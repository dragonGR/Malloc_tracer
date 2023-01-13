# Malloc Tracer tool

malloc and free tracer to detect memory leaks

This is a tool purely written for my Thesis and what it does is to detect malloc, i highly doubt this piece of code will be ever used in a real-life scenario.

# How to use it?

``make clean``

``make all``

The **LD_PRELOAD** is:

``LD_PRELOAD=/$pwd/tracer.so target_executable``

That's all folks!
