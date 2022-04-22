#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <getopt.h>
#include <pthread.h>
#include <cblas.h>
#include <omp.h>

#define WALLTIME(t) ((double)(t).tv_sec + 1e-6 * (double)(t).tv_usec)

void options(int argc, char **argv);

const char *usage =
    "\t-k <int>\n"
    "\t-m <int>\n"
    "\t-n <int>\n"
    "\t-t <int>\n";

//Global variables and definitions
int num_threads = 4;

double
    *A,
    *B,
    *C_serial, *C_openmp, *C_pthreads, *C_blas,
    alpha = 1.0, beta = 0.0;

int m = 1024, n = 1024, k = 1024;

/**
 * given a thread_num between 0 and num_threads - 1, it computes its part of the output matrix `C_pthreads`. 
 * 
 * @param thread_num_p this specifics threads index
 */
void *function_pthread(void *thread_num_p)
{
    // unravel this threads index
    int thread_index = *(int *)thread_num_p;

    // How many rows to calculate for each thread
    int thread_m = m / num_threads;

    // Start row for this specific thread
    int start = thread_m * thread_index;

    // the last thread handles overflow (for simplicity)
    int overflow = m - (num_threads * thread_m);
    if (overflow != 0 && thread_index == num_threads - 1)
    {
        thread_m += overflow;
    }

    // End row for this specific thread
    int end = start + thread_m;

    for (int i = start; i < end; i++)
    {
        for (int j = 0; j < n; j++)
        {
            double sum = 0;
            for (int p = 0; p < k; p++)
            {
                sum += A[i * k + p] * B[p * n + j];
            }
            C_pthreads[i * n + j] = sum;
        }
    }
    return 0;
}

///////////////////////////////////////////////////////////////////
// Serial                                                        //
///////////////////////////////////////////////////////////////////
void manual()
{
    for (int i = 0; i < m; i++)
    {
        for (int j = 0; j < n; j++)
        {
            double sum = 0;
            for (int p = 0; p < k; p++)
            {
                sum += A[i * k + p] * B[p * n + j];
            }
            C_serial[i * n + j] = sum;
        }
    }
}

///////////////////////////////////////////////////////////////////
// OpenMP                                                        //
///////////////////////////////////////////////////////////////////
void openmp()
{
    // Setting number of threads for OpenMP
    omp_set_num_threads(num_threads);
    // Parrallelize using compiler directives with OpenMP
#pragma omp parallel for
    for (int i = 0; i < m; i++)
    {
        for (int j = 0; j < n; j++)
        {
            double sum = 0;
            for (int p = 0; p < k; p++)
            {
                sum += A[i * k + p] * B[p * n + j];
            }
            C_openmp[i * n + j] = sum;
        }
    }
}

///////////////////////////////////////////////////////////////////
// Pthreads                                                      //
///////////////////////////////////////////////////////////////////
void pthreads()
{
    // Create array for thread ids
    pthread_t thread_id[num_threads];
    // Create array to place thread number/index in (to use as parameter)
    int thread_num[num_threads];

    // Create the threads and make them perform the function we created
    for (int i = 0; i < num_threads; i++)
    {
        thread_num[i] = i; // 0 -> num_threads - 1
        pthread_create(&thread_id[i], NULL, &function_pthread, &thread_num[i]);
    }

    // Join the threads (wait until everyone is finished)
    for (int i = 0; i < num_threads; i++)
    {
        pthread_join(thread_id[i], NULL);
    }
}

///////////////////////////////////////////////////////////////////
// BLAS                                                          //
///////////////////////////////////////////////////////////////////
void blas()
{
    // Documentation for GEMM:
    // https://software.intel.com/content/www/us/en/develop/documentation/onemkl-developer-reference-c/top/blas-and-sparse-blas-routines/blas-routines/blas-level-3-routines/cblas-gemm.html
    cblas_dgemm(
        CblasRowMajor, // Formating of the matrix, RowMajor or ColMajor
        CblasNoTrans,  // The form of Matrix A
        CblasNoTrans,  // The form of Matrix B
        m,             // Number of rows in A and in C
        n,             // Number of columns in B and C
        k,             // Number of columns in A and rows of B
        alpha,         // Scalar alpha of multiplication with A
        A,             // The A matrix
        k,             // Leading dimension of A, k if CblasRowMajor and NoTrans
        B,             // The B matrix
        n,             // Leading dimension of B, n if CblasRowMajor and NoTrans
        beta,          // Scalar beta of multiplication with C
        C_blas,        // The C matrix, this is also the output matrix
        n              // Leading dimension of C, n if CblasRowMajor
    );
}

int main(int argc, char **argv)
{
    options(argc, argv);

    printf("\nNumber of Threads:\t%d\n", num_threads);

    A = (double *)malloc(m * k * sizeof(double));
    B = (double *)malloc(k * n * sizeof(double));
    C_serial = (double *)malloc(m * n * sizeof(double));
    C_openmp = (double *)malloc(m * n * sizeof(double));
    C_pthreads = (double *)malloc(m * n * sizeof(double));
    C_blas = (double *)malloc(m * n * sizeof(double));

    int i, j;

    /* Initialize with dummy data */
    for (i = 0; i < (m * k); i++)
        A[i] = (double)(i + 1);
    for (i = 0; i < (k * n); i++)
        B[i] = (double)(-i - 1);
    for (i = 0; i < (m * n); i++)
    {
        C_serial[i] = 0.0;
        C_openmp[i] = 0.0;
        C_pthreads[i] = 0.0;
        C_blas[i] = 0.0;
    }

    struct timeval start, end;

    double total_time_manual = 0;
    double total_time_openmp = 0;
    double total_time_pthreads = 0;
    double total_time_blas = 0;

    /*
     * DGEMM (Double-precision GEneral Matrix Multiply)
     * Example: A general multiplication between two matricies A and B, accumulating in C.
     */
    // A has m rows and k columns
    // B has k rows and n columns
    // C has m rows and n columns

    // Perform the calculation using the various methods

    // Manual
    gettimeofday(&start, NULL);
    manual();
    gettimeofday(&end, NULL);
    total_time_manual = (WALLTIME(end) - WALLTIME(start));

    // OpenMP
    gettimeofday(&start, NULL);
    openmp();
    gettimeofday(&end, NULL);
    total_time_openmp = WALLTIME(end) - WALLTIME(start);

    // Pthreads
    gettimeofday(&start, NULL);
    pthreads();
    gettimeofday(&end, NULL);
    total_time_pthreads = WALLTIME(end) - WALLTIME(start);

    // BLAS
    gettimeofday(&start, NULL);
    blas();
    gettimeofday(&end, NULL);
    total_time_blas = WALLTIME(end) - WALLTIME(start);

    free(A);
    free(B);

    bool openmp_correct = true;
    bool pthreads_correct = true;
    bool blas_correct = true;

    for (i = 0; i < m; i++)
    {
        for (j = 0; j < n; j++)
        {
            if (C_openmp[i * n + j] != C_serial[i * n + j])
                openmp_correct = false;
            if (C_pthreads[i * n + j] != C_serial[i * n + j])
                pthreads_correct = false;
            if (C_blas[i * n + j] != C_serial[i * n + j])
                blas_correct = false;
        }
    }

    free(C_serial);
    free(C_openmp);
    free(C_pthreads);
    free(C_blas);

    printf("Manual\t\t\tTime:\t%es\n", total_time_manual);

    printf("OpenMP:\t  %s\t", openmp_correct ? "CORRECT" : "INCORRECT");
    printf("Time:\t%es\t", total_time_openmp);
    printf("Speedup: %.2lfx\n", total_time_manual / total_time_openmp);

    printf("Pthreads: %s\t", pthreads_correct ? "CORRECT" : "INCORRECT");
    printf("Time:\t%es\t", total_time_pthreads);
    printf("Speedup: %.2lfx\n", total_time_manual / total_time_pthreads);

    printf("BLAS:\t  %s\t", blas_correct ? "CORRECT" : "INCORRECT");
    printf("Time:\t%es\t", total_time_blas);
    printf("Speedup: %.2lfx\n", total_time_manual / total_time_blas);

    exit(EXIT_SUCCESS);
}

void options(int argc, char **argv)
{
    int o;
    while ((o = getopt(argc, argv, "k:m:n:t:h")) != -1)
        switch (o)
        {
        case 'k':
            k = strtol(optarg, NULL, 10);
            break;
        case 'm':
            m = strtol(optarg, NULL, 10);
            break;
        case 'n':
            n = strtol(optarg, NULL, 10);
            break;
        case 't':
            num_threads = strtol(optarg, NULL, 10);
            break;
        case 'h':
            fprintf(stderr, "%s", usage);
            exit(EXIT_FAILURE);
            break;
        }
}
