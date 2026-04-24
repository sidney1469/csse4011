#ifndef MATRIX_H
#define MATRIX_H

void transpose_matrix(float *A, float *At, int rowsA, int colsA);
void multiply_matrix(float *A, float *B, float *C, int rowsA, int colsA, int colsB);
int invert_2x2_matrix(float M[2][2], float inv[2][2]);

#endif /* MATRIX_H*/
