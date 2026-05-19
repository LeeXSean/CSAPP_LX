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
    int i, j, k, v1, v2, v3, v4, v5, v6, v7, v8;
    if (M == 32 && N == 32)
    {
        for (i = 0; i < N; i += 8)
        {
            for (j = 0; j < M; j += 8)
            {
                for (k = i; k < i + 8; k++)
                {
                    v1 = A[k][j];     v2 = A[k][j+1];
                    v3 = A[k][j+2];   v4 = A[k][j+3];
                    v5 = A[k][j+4];   v6 = A[k][j+5];
                    v7 = A[k][j+6];   v8 = A[k][j+7];

                    B[j][k] = v1;     B[j+1][k] = v2;
                    B[j+2][k] = v3;   B[j+3][k] = v4;
                    B[j+4][k] = v5;   B[j+5][k] = v6;
                    B[j+6][k] = v7;   B[j+7][k] = v8;
                }
            }
        }
    }
    else if (M == 64 && N == 64)
    {
        for (i = 0; i < N; i += 8)
        {
            for (j = 0; j < M; j += 8)
            {
                for (k = i; k < i + 4; k++)
                {
                    v1 = A[k][j];      v2 = A[k][j+1];     v3 = A[k][j+2];     v4 = A[k][j+3];
                    v5 = A[k][j+4];    v6 = A[k][j+5];     v7 = A[k][j+6];     v8 = A[k][j+7];

                    B[j][k] = v1;       B[j][k+4] = v5;
                    B[j+1][k] = v2;     B[j+1][k+4] = v6;
                    B[j+2][k] = v3;     B[j+2][k+4] = v7;
                    B[j+3][k] = v4;     B[j+3][k+4] = v8;
                    
                }
            
                for (k = j; k < j + 4; k++)
                {
                    v1 = A[i+4][k];    v2 = A[i+5][k];     v3 = A[i+6][k];     v4 = A[i+7][k];
                    v5 = B[k][i+4];    v6 = B[k][i+5];     v7 = B[k][i+6];     v8 = B[k][i+7];

                    B[k][i+4] = v1;    B[k][i+5] = v2;     B[k][i+6] = v3;     B[k][i+7] = v4;
                    B[k+4][i] = v5;    B[k+4][i+1] = v6;   B[k+4][i+2] = v7;   B[k+4][i+3] = v8;
                }
                
                for (k = i + 4; k < i + 8; k++)
                {
                    v1 = A[k][j+4];    v2 = A[k][j+5];   v3 = A[k][j+6];   v4 = A[k][j+7];

                    B[j+4][k] = v1;
                    B[j+5][k] = v2;
                    B[j+6][k] = v3;
                    B[j+7][k] = v4;
                }
            }
        }
    }
    else
    {
        for (i = 0; i < N; i += 16)
        {
            for (j = 0; j < M; j += 16)
            {
                for (v1 = i; v1 < i + 16 && v1 < N; v1++)
                {
                    for (v2 = j; v2 < j + 16 && v2 < M; v2++)
                    {
                        B[v2][v1] = A[v1][v2];
                    }
                }
            }
        }
    }
}


/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 
char transpose_32_desc[] = "For 32 * 32 matrix";
void transpose_32(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, i1, j1, tmp;
    for (i = 0; i < N; i += 8)
    {
        for (j = 0; j < M; j += 8)
        {
            for (i1 = i; i1 < i + 8; i1++)
            {
                for (j1 = j; j1 < j + 8; j1++)
                {
                    if (j == i && j1 == i1)
                    {
                        tmp = A[j1][j1];
                    }
                    else
                    {
                        B[j1][i1] = A[i1][j1];
                    }
                }
                if (i == j)
                {
                    B[i1][i1] = tmp;
                }
            }
        }
    }
}

/*
 * 8x8 block diagram (each numbered sub-block is 4x4)
 *  __________________________________
 * |                |                 |
 * |        1       |        2        |
 * |________________|_________________|
 * |                |                 |
 * |        3       |        4        |
 * |________________|_________________|
 */
char transpose_64_desc[] = "For 64 * 64 matrix: ";
void transpose_64(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, k, v1, v2, v3, v4, v5, v6, v7, v8;
    for (i = 0; i < N; i += 8)
    {
        for (j = 0; j < M; j += 8)
        {
            /*
                Read blocks 1 and 2 of A, transpose them, and write into blocks 1 and 2 of B.
                Block 2 of B temporarily holds data that should eventually go into block 3.
            */
            for (k = i; k < i + 4; k++)
            {
                v1 = A[k][j];      v2 = A[k][j+1];     v3 = A[k][j+2];     v4 = A[k][j+3];
                v5 = A[k][j+4];    v6 = A[k][j+5];     v7 = A[k][j+6];     v8 = A[k][j+7];

                B[j][k] = v1;       B[j][k+4] = v5;
                B[j+1][k] = v2;     B[j+1][k+4] = v6;
                B[j+2][k] = v3;     B[j+2][k+4] = v7;
                B[j+3][k] = v4;     B[j+3][k+4] = v8;
                
            }
            /*
                1. Read block 3 of A, transpose it, and write into block 2 of B.
                2. Move the temporarily stored data from block 2 into block 3.
                   These two operations are interleaved.
            */
            for (k = j; k < j + 4; k++)
            {
                v1 = A[i+4][k];    v2 = A[i+5][k];     v3 = A[i+6][k];     v4 = A[i+7][k];
                v5 = B[k][i+4];    v6 = B[k][i+5];     v7 = B[k][i+6];     v8 = B[k][i+7];

                B[k][i+4] = v1;    B[k][i+5] = v2;     B[k][i+6] = v3;     B[k][i+7] = v4;
                B[k+4][i] = v5;    B[k+4][i+1] = v6;   B[k+4][i+2] = v7;   B[k+4][i+3] = v8;
            }
            /*
                Read block 4 of A, transpose it, and write into block 4 of B.
            */
            for (k = i + 4; k < i + 8; k++)
            {
                v1 = A[k][j+4];    v2 = A[k][j+5];   v3 = A[k][j+6];   v4 = A[k][j+7];

                B[j+4][k] = v1;
                B[j+5][k] = v2;
                B[j+6][k] = v3;
                B[j+7][k] = v4;
            }
        }
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

