FROM gcc:latest

RUN apt-get update && apt-get install -y libomp-dev

WORKDIR /app

COPY openmp.c .
COPY input.txt .

RUN gcc -fopenmp -O3 -o openmp openmp.c -lm

CMD ["./openmp", "1000.0", "input.txt"]