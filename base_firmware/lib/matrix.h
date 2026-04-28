/*********************************** */
/*             matrix.h              */
/*********************************** */
/* Authors                           */
/* Sidney Neil 47441952              */
/* Fiachra Richards  47450271        */
/*********************************** */

#ifndef MATRIX_H
#define MATRIX_H

/* Transposes a row-major matrix A into At */
void transpose_matrix(float *A, float *At, int rowsA, int colsA);

/* Multiplies two row-major matrices: C = A * B */
void multiply_matrix(float *A, float *B, float *C, int rowsA, int colsA, int colsB);

/* Inverts a 2x2 matrix, returning -1 if the matrix is singular */
int invert_2x2_matrix(float M[2][2], float inv[2][2]);

#endif /* MATRIX_H */