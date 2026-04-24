#ifndef MATRIX_H
#define MATRIX_H

void transpose_matrix(float *A, float *At, int rowsA, int colsA);
void multiply_matrix(float *A, float *B, float *C, int rowsA, int colsA, int colsB);
int invert_3x3_matrix(float M[3][3], float inv[3][3]);

#endif /* MATRIX_H*/
