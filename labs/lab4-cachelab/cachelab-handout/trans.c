/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    void trans_block_optm(int M, int N, int A[N][M], int B[M][N]);
    void trans_blocking(int M, int N, int A[N][M], int B[M][N]);
    void trans_small_block(int M, int N, int A[N][M], int B[M][N]);

    if (M == 32) {
        trans_block_optm(M, N, A, B);
    } else if (M == 64) {
        trans_small_block(M, N, A, B);
    } else {
        trans_blocking(M, N, A, B);
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * trans_blocking - Using Blocking to implement transpose function
 */
#define MIN(x, y)   ((x) <= (y) ? (x) : (y))
#define BLOCK_SIZE  8       /* the size of a cache block is 32 byte */
char trans_blocking_desc[] = "Using Blocking to transpose";
void trans_blocking(int M, int N, int A[N][M], int B[M][N]) {
    const int block_size = 17;
    for (int i = 0; i < N; i += block_size) 
        for (int j = 0; j < M; j += block_size)
            for (int ii = i; ii < MIN(i+block_size, N); ii += 1)
                for (int jj = j; jj < MIN(j+block_size, M); jj += 1)
                    B[jj][ii] = A[ii][jj];
}

/*
 * trans_small_block - Using a smaller block to implement transpose function, 
 * to prevent thrashing
 * 
 * The division of matrix:      | 1 | 2 |
 *                              | 3 | 4 |
 * relative page: https://www.cnblogs.com/liqiuhao/p/8026100.html
 */
#define SUB_BLOCK 4
char trans_small_block_desc[] = "Using a smaller block to transpose";
void trans_small_block(int M, int N, int A[N][M], int B[M][N]) {
    int temp_val0, temp_val1, temp_val2, temp_val3, 
        temp_val4, temp_val5, temp_val6, temp_val7;
    
    for (int i = 0; i < N; i += BLOCK_SIZE) 
        for (int j = 0; j < M; j += BLOCK_SIZE) {
            // Assume M = N = 64
            for (int ii = i; ii < i+SUB_BLOCK; ii += 1) {
                temp_val0 = A[ii][j+0];     temp_val1 = A[ii][j+1];
                temp_val2 = A[ii][j+2];     temp_val3 = A[ii][j+3];
                temp_val4 = A[ii][j+4];     temp_val5 = A[ii][j+5];
                temp_val6 = A[ii][j+6];     temp_val7 = A[ii][j+7];

                B[j+0][ii] = temp_val0;     B[j+1][ii] = temp_val1;
                B[j+2][ii] = temp_val2;     B[j+3][ii] = temp_val3;
                B[j+0][ii+4] = temp_val7;   B[j+1][ii+4] = temp_val6;
                B[j+2][ii+4] = temp_val5;   B[j+3][ii+4] = temp_val4;
            }

            for (int k = 0; k < SUB_BLOCK; k += 1) {
                temp_val0 = A[i+4][j+3-k];      temp_val4 = A[i+4][j+4+k];
                temp_val1 = A[i+5][j+3-k];      temp_val5 = A[i+5][j+4+k];
                temp_val2 = A[i+6][j+3-k];      temp_val6 = A[i+6][j+4+k];
                temp_val3 = A[i+7][j+3-k];      temp_val7 = A[i+7][j+4+k];

                B[j+4+k][i+0] = B[j+3-k][i+4];  B[j+4+k][i+1] = B[j+3-k][i+5];
                B[j+4+k][i+2] = B[j+3-k][i+6];  B[j+4+k][i+3] = B[j+3-k][i+7];

                B[j+3-k][i+4] = temp_val0;    B[j+4+k][i+4] = temp_val4;
                B[j+3-k][i+5] = temp_val1;    B[j+4+k][i+5] = temp_val5;
                B[j+3-k][i+6] = temp_val2;    B[j+4+k][i+6] = temp_val6;
                B[j+3-k][i+7] = temp_val3;    B[j+4+k][i+7] = temp_val7;
            }
        }
}

/*
 * trans_block_optm - Using a special variable to store the value may cause 
 * thrashing
 */
char trans_block_optm_desc[] = "Using a special variable to transpose with block";
void trans_block_optm(int M, int N, int A[N][M], int B[M][N]) {
    register int temp;
    
    for (int i = 0; i < N; i += BLOCK_SIZE) 
        for (int j = 0; j < M; j += BLOCK_SIZE)
            for (int ii = i; ii < MIN(i+BLOCK_SIZE, N); ii += 1) {
                temp = A[ii][ii-i+j];
                for (int jj = j; jj < MIN(j+BLOCK_SIZE, M); jj += 1) {
                    if (ii - i == jj - j) 
                        continue;
                    B[jj][ii] = A[ii][jj];
                }
                B[ii-i+j][ii] = temp;
            }
}


/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 
}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

