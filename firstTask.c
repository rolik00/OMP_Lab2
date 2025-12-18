#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>

// Максимальное количество итераций для проверки ограниченности
#define MAX_ITERATIONS 1000

// Границы области на комплексной плоскости
#define MIN_X -2.0
#define MAX_X 1.0
#define MIN_Y -1.5
#define MAX_Y 1.5

typedef struct {
    double x;
    double y;
} Point;

char mandelbrotContains(double real, double img) {
    double z_real = 0.0;
    double z_img = 0.0;

    // Цикл: zn+1 = zn^2 + c
    // Условие выхода: |z|^2 >= 4 (то есть |z| >= 2) или превышен лимит итераций
    for (int i = 0; i < MAX_ITERATIONS; i++) {
        double z_real_sq = z_real * z_real;
        double z_img_sq = z_img * z_img;

        if (z_real_sq + z_img_sq >= 4.0) {
            return 0;
        }

        double new_z_real = z_real_sq - z_img_sq + real;
        double new_z_img = 2.0 * z_real * z_img + img;

        z_real = new_z_real;
        z_img = new_z_img;
    }

    // Если цикл дошел до MAX_ITERATIONS, считаем, что точка принадлежит множеству
    return 1;
}

int main(int argc, char* argv[]) {

    if (argc != 3) {
        printf("Usage: %s nthreads npoints\n", argv[0]);
        return 1;
    }

    int nthreads = atoi(argv[1]);
    int npoints = atoi(argv[2]);

    if (npoints <= 0 || nthreads <= 0) {
        printf("Error: nthreads and npoints must be positive integers.\n");
        return 1;
    }

    omp_set_num_threads(nthreads);

    Point* points = (Point*)malloc(sizeof(Point) * npoints);
    if (points == NULL) {
        printf("Error: Memory allocation failed.\n");
        return 1;
    }

    long count = 0;

    double start_time = omp_get_wtime();

#pragma omp parallel
    {
        srand(time(NULL) ^ omp_get_thread_num());

        while (1) {
            long current_count;

#pragma omp atomic read
            current_count = count;
            if (current_count >= npoints) {
                break;
            }

            double x = (double)rand() / RAND_MAX * (MAX_X - MIN_X) + MIN_X;
            double y = (double)rand() / RAND_MAX * (MAX_Y - MIN_Y) + MIN_Y;

            if (mandelbrotContains(x, y)) {
                long current_index;

#pragma omp atomic capture
                current_index = count++;

                if (current_index < npoints) {
                    points[current_index].x = x;
                    points[current_index].y = y;
                }
                else {
                    break;
                }
            }
        }
    }

    double end_time = omp_get_wtime();
    printf("Calculation completed in %f seconds using %d threads.\n", end_time - start_time, nthreads);

    FILE* fp = fopen("mandelbrot.csv", "w");
    if (fp == NULL) {
        printf("Error: Could not open file for writing.\n");
        free(points);
        return 1;
    }

    fprintf(fp, "x,y\n");

    for (int i = 0; i < npoints; i++) {
        fprintf(fp, "%.6f,%.6f\n", points[i].x, points[i].y);
    }

    fclose(fp);
    free(points);

    printf("Results written to mandelbrot.csv\n");

    return 0;

}
