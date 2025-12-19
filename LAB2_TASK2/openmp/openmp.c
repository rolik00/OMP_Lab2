#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <omp.h>

#define G 6.67430e-11
#define DIM 3
#define OUTPUT_EVERY 1000

typedef struct {
    double m;
    double r[DIM];
    double v[DIM];
    double a[DIM];
} Particle;

void calculate_forces(Particle* particles, int n) {
    #pragma omp parallel for
    for (int i = 0; i < n; i++) {
        for (int d = 0; d < DIM; d++) {
            particles[i].a[d] = 0.0;
        }
    }

    #pragma omp parallel for
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            double dx = particles[j].r[0] - particles[i].r[0];
            double dy = particles[j].r[1] - particles[i].r[1];
            double dz = particles[j].r[2] - particles[i].r[2];
            
            double r2 = dx*dx + dy*dy + dz*dz;
            double r = sqrt(r2);
            double r3 = r * r * r;
            double F = G * particles[i].m * particles[j].m / (r3 + 1e-10);

            double Fx = F * dx;
            double Fy = F * dy;
            double Fz = F * dz;
            
            // Ускорения (a = F/m)
            #pragma omp atomic
            particles[i].a[0] += Fx / particles[i].m;
            #pragma omp atomic
            particles[i].a[1] += Fy / particles[i].m;
            #pragma omp atomic
            particles[i].a[2] += Fz / particles[i].m;
            
            // Противоположная сила на j от i (3-й закон Ньютона)
            #pragma omp atomic
            particles[j].a[0] -= Fx / particles[j].m;
            #pragma omp atomic
            particles[j].a[1] -= Fy / particles[j].m;
            #pragma omp atomic
            particles[j].a[2] -= Fz / particles[j].m;
        }
    }
}

void euler_step(Particle* particles, int n, double dt) {
    #pragma omp parallel for
    for (int i = 0; i < n; i++) {
        double vx_old = particles[i].v[0];
        double vy_old = particles[i].v[1];
        double vz_old = particles[i].v[2];

        particles[i].v[0] += particles[i].a[0] * dt;
        particles[i].v[1] += particles[i].a[1] * dt;
        particles[i].v[2] += particles[i].a[2] * dt;

        particles[i].r[0] += vx_old * dt;
        particles[i].r[1] += vy_old * dt;
        particles[i].r[2] += vz_old * dt;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <t_end> <input_file>\n", argv[0]);
        return 1;
    }
    double start_time, end_time;
    double t_end = atof(argv[1]);
    char* input_file = argv[2];
    
    FILE* fin = fopen(input_file, "r");
    if (!fin) {
        fprintf(stderr, "Cant open input file: %s\n", input_file);
        return 1;
    }
    
    int n;
    if (fscanf(fin, "%d", &n) != 1) {
        fprintf(stderr, "Error reading number of particles\n");
        fclose(fin);
        return 1;
    }
    
    Particle* particles = (Particle*)malloc(n * sizeof(Particle));
    
    for (int i = 0; i < n; i++) {
        if (fscanf(fin, "%lf", &particles[i].m) != 1) {
            fprintf(stderr, "Error reading mass for particle %d\n", i+1);
            fclose(fin);
            free(particles);
            return 1;
        }
        
        for (int d = 0; d < DIM; d++) {
            if (fscanf(fin, "%lf", &particles[i].r[d]) != 1) {
                fprintf(stderr, "Error reading position for particle %d\n", i+1);
                fclose(fin);
                free(particles);
                return 1;
            }
        }
        
        for (int d = 0; d < DIM; d++) {
            if (fscanf(fin, "%lf", &particles[i].v[d]) != 1) {
                fprintf(stderr, "Error reading velocity for particle %d\n", i+1);
                fclose(fin);
                free(particles);
                return 1;
            }
        }
    }
    fclose(fin);
    
    double dt = 0.01;
    printf("N-body simulation with OpenMP\n");
    printf("Number of particles: %d\n", n);
    printf("Simulation time: 0 to %.2f\n", t_end);
    printf("Time step: %.6f\n", dt);
    printf("Number of steps: %.0f\n", t_end/dt);
    
    FILE* fout = fopen("trajectories.csv", "w");
    if (!fout) {
        fprintf(stderr, "Cant create output file\n");
        free(particles);
        return 1;
    }
    
    fprintf(fout, "t");
    for (int i = 0; i < n; i++) {
        fprintf(fout, ",x%d,y%d,z%d", i+1, i+1, i+1);
    }
    fprintf(fout, "\n");
    fprintf(fout, "0.000000");
    for (int i = 0; i < n; i++) {
        fprintf(fout, ",%.6f,%.6f,%.6f", particles[i].r[0], particles[i].r[1],particles[i].r[2]);
    }
    fprintf(fout, "\n");
    double t = 0.0;
    long step = 0;
    start_time = omp_get_wtime();

    while (t < t_end) {
        calculate_forces(particles, n);
        euler_step(particles, n, dt);
        
        t += dt;
        step++;
        
        if (step % OUTPUT_EVERY == 0) {
            fprintf(fout, "%.6f", t);
            for (int i = 0; i < n; i++) {
                fprintf(fout, ",%.6f,%.6f,%.6f", particles[i].r[0], particles[i].r[1], particles[i].r[2]);
            }
            fprintf(fout, "\n");
        }
    }
    
    end_time = omp_get_wtime();
    printf("Total simulation time: %.2f seconds\n", end_time - start_time);
    if ((step-1) % OUTPUT_EVERY != 0) {
        fprintf(fout, "%.6f", t);
        for (int i = 0; i < n; i++) {
            fprintf(fout, ",%.6f,%.6f,%.6f", particles[i].r[0], particles[i].r[1],particles[i].r[2]);
        }
        fprintf(fout, "\n");
    }
    
    fclose(fout);
    free(particles);
    
    printf("Simulation completed. Results saved to trajectories.csv\n");
    printf("Total steps: %ld\n", step);
    return 0;
}