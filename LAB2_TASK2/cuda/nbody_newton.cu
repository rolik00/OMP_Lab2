#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cuda_runtime.h>

#define G 6.67430e-11f
#define DT 0.1f
#define SOFTENING 1e-5f
#define OUTPUT_EVERY 10

typedef struct {
    float x, y, z;
} Vector3f;

__global__ void compute_forces_newton3(float* masses, float* positions, float* accelerations, int n) {
    int tid = threadIdx.x + blockIdx.x * blockDim.x;
    int total_pairs = n * (n - 1) / 2;

    if (tid >= total_pairs) return;
    int i = 0, j = 0;
    int remaining = tid;

    for (i = 0; i < n - 1; i++) {
        int pairs_in_row = n - i - 1;
        if (remaining < pairs_in_row) {
            j = i + 1 + remaining;
            break;
        }
        remaining -= pairs_in_row;
    }

    Vector3f ri = {
        positions[i * 3],
        positions[i * 3 + 1],
        positions[i * 3 + 2]
    };

    Vector3f rj = {
        positions[j * 3],
        positions[j * 3 + 1],
        positions[j * 3 + 2]
    };

    float dx = rj.x - ri.x;
    float dy = rj.y - ri.y;
    float dz = rj.z - ri.z;

    float dist2 = dx * dx + dy * dy + dz * dz + SOFTENING;
    float dist = sqrtf(dist2);
    float invDist3 = 1.0f / (dist * dist2);

    float force_mag = G * masses[i] * masses[j] * invDist3;
    float fx = force_mag * dx / dist;
    float fy = force_mag * dy / dist;
    float fz = force_mag * dz / dist;

    atomicAdd(&accelerations[i * 3], fx / masses[i]);
    atomicAdd(&accelerations[i * 3 + 1], fy / masses[i]);
    atomicAdd(&accelerations[i * 3 + 2], fz / masses[i]);

    atomicAdd(&accelerations[j * 3], -fx / masses[j]);
    atomicAdd(&accelerations[j * 3 + 1], -fy / masses[j]);
    atomicAdd(&accelerations[j * 3 + 2], -fz / masses[j]);
}

__global__ void clear_accelerations(float* accelerations, int n) {
    int idx = threadIdx.x + blockIdx.x * blockDim.x;

    if (idx >= n) return;

    accelerations[idx * 3] = 0.0f;
    accelerations[idx * 3 + 1] = 0.0f;
    accelerations[idx * 3 + 2] = 0.0f;
}

__global__ void euler_integrate(float* positions, float* velocities, float* accelerations, int n, float dt) {
    int idx = threadIdx.x + blockIdx.x * blockDim.x;
    
    if (idx >= n) return;
    
    float vx_old = velocities[idx * 3];
    float vy_old = velocities[idx * 3 + 1];
    float vz_old = velocities[idx * 3 + 2];
    
    // Формула: v_{n} = v_{n-1} + a_{n-1} * Δt
    velocities[idx * 3] += accelerations[idx * 3] * dt;
    velocities[idx * 3 + 1] += accelerations[idx * 3 + 1] * dt;
    velocities[idx * 3 + 2] += accelerations[idx * 3 + 2] * dt;
    
    // Формула: r_{n} = r_{n-1} + v_{n-1} * Δt
    positions[idx * 3] += vx_old * dt;   
    positions[idx * 3 + 1] += vy_old * dt;
    positions[idx * 3 + 2] += vz_old * dt;
}

int main() {
    int deviceCount;
    cudaGetDeviceCount(&deviceCount);
    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);

    int n = 3;
    float* h_masses = (float*)malloc(n * sizeof(float));
    float* h_positions = (float*)malloc(n * 3 * sizeof(float));
    float* h_velocities = (float*)malloc(n * 3 * sizeof(float));
    float* h_accelerations = (float*)malloc(n * 3 * sizeof(float));
    h_masses[0] = 1.0e6f;
    h_masses[1] = 1.0e3f;
    h_masses[2] = 1.0e3f;

    h_positions[0] = 0.0f; h_positions[1] = 0.0f; h_positions[2] = 0.0f;
    h_positions[3] = 5.0f; h_positions[4] = 0.0f; h_positions[5] = 0.0f;
    h_positions[6] = 0.0f; h_positions[7] = 5.0f; h_positions[8] = 0.0f;
    float v_orbit = sqrtf(G * h_masses[0] / 5.0f);

    h_velocities[0] = 0.0f; h_velocities[1] = 0.0f; h_velocities[2] = 0.0f;
    h_velocities[3] = 0.0f; h_velocities[4] = v_orbit; h_velocities[5] = 0.0f;
    h_velocities[6] = -v_orbit; h_velocities[7] = 0.0f; h_velocities[8] = 0.0f; 

    for (int i = 0; i < n * 3; i++) {
        h_accelerations[i] = 0.0f;
    }

    printf("Initial conditions:\n");
    printf("Particles: %d\n", n);
    printf("Central mass: %.2e kg\n", h_masses[0]);
    printf("Orbital radius: 5.0 m\n");
    printf("Orbital velocity: %.6f m/s\n", v_orbit);
    printf("Orbital period: %.2f s\n", 2.0f * 3.1415926535f * 5.0f / v_orbit);

    printf("\nParticle details:\n");
    for (int i = 0; i < n; i++) {
        printf("Particle %d: m=%.2e kg, r=(%.3f, %.3f, %.3f) m, v=(%.6f, %.6f, %.6f) m/s\n",
               i, h_masses[i],
               h_positions[i*3], h_positions[i*3+1], h_positions[i*3+2],
               h_velocities[i*3], h_velocities[i*3+1], h_velocities[i*3+2]);
    }
    float *d_masses, *d_positions, *d_velocities, *d_accelerations;
    cudaMalloc(&d_masses, n * sizeof(float));
    cudaMalloc(&d_positions, n * 3 * sizeof(float));
    cudaMalloc(&d_velocities, n * 3 * sizeof(float));
    cudaMalloc(&d_accelerations, n * 3 * sizeof(float));
    cudaMemcpy(d_masses, h_masses, n * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_positions, h_positions, n * 3 * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_velocities, h_velocities, n * 3 * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_accelerations, h_accelerations, n * 3 * sizeof(float), cudaMemcpyHostToDevice);

    int block_size = 256;
    int grid_size_forces = (n + block_size - 1) / block_size;
    int total_pairs = n * (n - 1) / 2;
    int grid_size_pairs = (total_pairs + block_size - 1) / block_size;
    float t = 0.0f;
    float t_end = 100.0f; 
    int step = 0;

    printf("\nSimulation parameters:\n");
    printf("Time step (dt): %.3f s\n", DT);
    printf("Total simulation time: %.1f s\n", t_end);
    printf("Number of steps: %.0f\n", t_end / DT);
    printf("Output every %d steps\n", OUTPUT_EVERY);
    printf("Using Newton's 3rd Law: YES\n");
    printf("Integration method: Explicit Euler\n");
    FILE* fout = fopen("trajectories.csv", "w");
    if (!fout) {
        printf("Error: Cannot open trajectories.csv for writing\n");
        return 1;
    }
    fprintf(fout, "t");
    for (int i = 0; i < n; i++) {
        fprintf(fout, ",x%d,y%d,z%d", i+1, i+1, i+1);
    }
    fprintf(fout, "\n");

    fprintf(fout, "%.6f", t);
    for (int i = 0; i < n; i++) {
        fprintf(fout, ",%.6f,%.6f,%.6f",
                h_positions[i*3], h_positions[i*3+1], h_positions[i*3+2]);
    }
    fprintf(fout, "\n");

    while (t < t_end) {
        clear_accelerations<<<grid_size_forces, block_size>>>(d_accelerations, n);
        cudaDeviceSynchronize();
        if (total_pairs > 0) {
            compute_forces_newton3<<<grid_size_pairs, block_size>>>(d_masses, d_positions, d_accelerations, n);
        }
        cudaDeviceSynchronize();
        euler_integrate<<<grid_size_forces, block_size>>>(d_positions, d_velocities, d_accelerations, n, DT);
        cudaDeviceSynchronize();

        t += DT;
        step++;
        if (step % OUTPUT_EVERY == 0) {
            cudaMemcpy(h_positions, d_positions, n * 3 * sizeof(float), cudaMemcpyDeviceToHost);
            fprintf(fout, "%.6f", t);
            for (int i = 0; i < n; i++) {
                fprintf(fout, ",%.6f,%.6f,%.6f",
                        h_positions[i*3], h_positions[i*3+1], h_positions[i*3+2]);
            }
            fprintf(fout, "\n");
            printf("%6.1f  (%7.3f,%7.3f)   (%7.3f,%7.3f)   %5.1f%%\n",
                   t,
                   h_positions[3], h_positions[4], 
                   h_positions[6], h_positions[7],
                   (t / t_end) * 100.0f);

            fflush(fout);
        }
    }

    cudaMemcpy(h_positions, d_positions, n * 3 * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_velocities, d_velocities, n * 3 * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_accelerations, d_accelerations, n * 3 * sizeof(float), cudaMemcpyDeviceToHost);

    fprintf(fout, "%.6f", t);
    for (int i = 0; i < n; i++) {
        fprintf(fout, ",%.6f,%.6f,%.6f",
                h_positions[i*3], h_positions[i*3+1], h_positions[i*3+2]);
    }
    fprintf(fout, "\n");
    fclose(fout);

    printf("Total steps: %d\n", step);
    printf("Final time: %.3f s\n", t);
    printf("Results saved to: trajectories.csv\n");
    free(h_masses);
    free(h_positions);
    free(h_velocities);
    free(h_accelerations);

    cudaFree(d_masses);
    cudaFree(d_positions);
    cudaFree(d_velocities);
    cudaFree(d_accelerations);
    return 0;
}