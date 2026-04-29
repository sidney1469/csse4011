/*********************************** */
/*             kalman.c              */
/*********************************** */
/* Authors                           */
/* Sidney Neil 47441952              */
/* Fiachra Richards  47450271        */
/*********************************** */

/********* Include Libraries ******* */
#include <string.h>
#include <math.h>
#include "kalman.h"
/********************************* */

/*********** Global Defines ********** */

/* Kalman filter tuning constants */
#define Q 0.01f // Process noise variance
#define R 4.0f  // Measurement noise variance

/* State vector: [x, y, vx, vy] */
static float X[4];

/* State covariance matrix */
static float P[4][4];

/*
 * Initialises the Kalman filter state.
 * The initial position is set from the first valid measurement, while velocity
 * starts at zero.
 */
void init_filter(float x0, float y0)
{
    memset(X, 0, sizeof(X));
    X[0] = x0;
    X[1] = y0;

    memset(P, 0, sizeof(P));

    /* Initial uncertainty for position and velocity estimates */
    P[0][0] = P[1][1] = 1.0f;
    P[2][2] = P[3][3] = 0.1f;
}

/*
 * Predicts the next filter state using a constant velocity motion model.
 * The covariance is also updated to account for increasing uncertainty over time.
 */
void kalman_predict(float dt)
{
    float F[4][4] = {
        {1, 0, dt, 0},
        {0, 1, 0, dt},
        {0, 0, 1, 0},
        {0, 0, 0, 1},
    };

    /* Predict next state: X = F * X */
    float X_temp[4];
    multiply_matrix((float *)F, X, X_temp, 4, 4, 1);
    for (int i = 0; i < 4; i++) {
        X[i] = X_temp[i];
    }

    /* Predict covariance: P = F * P * F^T */
    float F_T[4][4];
    float FP[4][4];
    float FPF_T[4][4];

    transpose_matrix((float *)F, (float *)F_T, 4, 4);

    multiply_matrix((float *)F, (float *)P, (float *)FP, 4, 4, 4);
    multiply_matrix((float *)FP, (float *)F_T, (float *)FPF_T, 4, 4, 4);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            P[i][j] = FPF_T[i][j];
        }
    }

    /* Add process noise based on elapsed time */
    float dt2 = dt * dt;
    float dt3 = dt2 * dt;
    float dt4 = dt2 * dt2;

    P[0][0] += Q * dt4 / 4.0f;
    P[1][1] += Q * dt4 / 4.0f;
    P[2][2] += Q * dt2;
    P[3][3] += Q * dt2;
    P[0][2] += Q * dt3 / 2.0f;
    P[2][0] += Q * dt3 / 2.0f;
    P[1][3] += Q * dt3 / 2.0f;
    P[3][1] += Q * dt3 / 2.0f;

    // Force symmetry in case of floating point difference
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            P[i][j] = P[j][i] = 0.5f * (P[i][j] + P[j][i]);
        }
    }
}

/*
 * Updates the predicted state using a new measured position.
 * The Kalman gain controls how strongly the filter trusts the measurement
 * compared with the predicted state.
 */
void kalman_update(float x_meas, float y_meas)
{
    /* Measurement matrix maps state [x, y, vx, vy] to measured position [x, y] */
    float H[2][4] = {
        {1, 0, 0, 0},
        {0, 1, 0, 0},
    };

    /* Measurement residual: y = z - H * X */
    float HX[2];
    multiply_matrix((float *)H, X, HX, 2, 4, 1);
    float y[2] = {
        x_meas - HX[0],
        y_meas - HX[1],
    };

    /* Innovation covariance: S = H * P * H^T + R */
    float H_T[4][2];
    float HP[2][4];
    float HPH_T[2][2];

    transpose_matrix((float *)H, (float *)H_T, 2, 4);
    multiply_matrix((float *)H, (float *)P, (float *)HP, 2, 4, 4);
    multiply_matrix((float *)HP, (float *)H_T, (float *)HPH_T, 2, 4, 2);

    HPH_T[0][0] += R;
    HPH_T[1][1] += R;

    /* Skip update if the innovation covariance cannot be inverted */
    float S_inv[2][2];
    if (invert_2x2_matrix(HPH_T, S_inv) != 0) {
        return;
    }

    /* Kalman gain: K = P * H^T * S^-1 */
    float PH_T[4][2];
    float K[4][2];

    multiply_matrix((float *)P, (float *)H_T, (float *)PH_T, 4, 4, 2);
    multiply_matrix((float *)PH_T, (float *)S_inv, (float *)K, 4, 2, 2);

    /* Correct state estimate: X = X + K * y */
    float Ky[4];
    multiply_matrix((float *)K, y, Ky, 4, 2, 1);
    for (int i = 0; i < 4; i++) {
        X[i] += Ky[i];
    }

    /* Update covariance using the Joseph form for improved numerical stability */
    float KH[4][4];
    multiply_matrix((float *)K, (float *)H, (float *)KH, 4, 2, 4);

    float I_KH[4][4];
    float I_KH_T[4][4];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            I_KH[i][j] = (i == j ? 1.0f : 0.0f) - KH[i][j];
        }
    }
    transpose_matrix((float *)I_KH, (float *)I_KH_T, 4, 4);

    float IKHP[4][4];
    float IKHPIKH_T[4][4];
    multiply_matrix((float *)I_KH, (float *)P, (float *)IKHP, 4, 4, 4);
    multiply_matrix((float *)IKHP, (float *)I_KH_T, (float *)IKHPIKH_T, 4, 4, 4);

    float K_T[2][4];
    transpose_matrix((float *)K, (float *)K_T, 4, 2);

    float KR[4][2];
    float KRK_T[4][4];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 2; j++) {
            KR[i][j] = K[i][j] * R;
        }
    }
    multiply_matrix((float *)KR, (float *)K_T, (float *)KRK_T, 4, 2, 4);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            P[i][j] = IKHPIKH_T[i][j] + KRK_T[i][j];
        }
    }

    // Force symmetry in case of floating point difference
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            P[i][j] = P[j][i] = 0.5f * (P[i][j] + P[j][i]);
        }
    }
}

/* Returns the current filtered position estimate */
void kalman_get_position(float *x, float *y)
{
    *x = X[0];
    *y = X[1];
}

/* Returns the current filtered velocity estimate */
void kalman_get_velocity(float *x, float *y)
{
    *x = X[2];
    *y = X[3];
}