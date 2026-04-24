#include <string.h>
#include <math.h>
#include "kalman.h"

#define N_BEACONS 13
#define Q_POS     0.1f  // higher = trust motion model less
#define Q_VEL     0.01f // lower = trust motion model more
#define R         2.0f  // measurement noise variance (units: m²)

static float X[6];
static float P[6][6];

void init_filter(float x0, float y0, float z0)
{
    memset(X, 0, sizeof(X));
    X[0] = x0;
    X[1] = y0;
    X[2] = z0;

    memset(P, 0, sizeof(P));

    // Set initial uncertainty (Can be toggled)
    P[0][0] = P[1][1] = P[2][2] = 1.0f;  // position
    P[3][3] = P[4][4] = P[5][5] = 10.0f; // velocity
}

void kalman_predict(float dt)
{
    float F[6][6] = {
        {1, 0, 0, dt, 0, 0}, {0, 1, 0, 0, dt, 0}, {0, 0, 1, 0, 0, dt},
        {0, 0, 0, 1, 0, 0},  {0, 0, 0, 0, 1, 0},  {0, 0, 0, 0, 0, 1},
    };

    // State transition: F @ x
    float X_temp[6];
    multiply_matrix((float *)F, X, X_temp, 6, 6, 1);
    for (int i = 0; i < 6; i++) {
        X[i] = X_temp[i];
    }
    // P = F * P * F_T + Q
    float F_T[6][6];
    float FP[6][6];
    float FPF_T[6][6];

    transpose_matrix((float *)F, (float *)F_T, 6, 6);

    multiply_matrix((float *)F, (float *)P, (float *)FP, 6, 6, 6);
    multiply_matrix((float *)FP, (float *)F_T, (float *)FPF_T, 6, 6, 6);

    // Write back into P and add Q
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            P[i][j] = FPF_T[i][j];
        }
    }

    // Add process noise Q
    for (int i = 0; i < 3; i++) {
        P[i][i] += Q_POS;
        P[i + 3][i + 3] += Q_VEL;
    }
}

void kalman_update(float x_meas, float y_meas, float z_meas)
{
    // H: state-to-measurement matrix (3x6)
    float H[3][6] = {
        {1, 0, 0, 0, 0, 0},
        {0, 1, 0, 0, 0, 0},
        {0, 0, 1, 0, 0, 0},
    };

    // Innovation: y = z - H*x
    float HX[3];
    multiply_matrix((float *)H, X, HX, 3, 6, 1);
    float y[3] = {
        x_meas - HX[0],
        y_meas - HX[1],
        z_meas - HX[2],
    };

    // S = H*P*Hᵀ + R*I
    float H_T[6][3];
    float HP[3][6];
    float HPH_T[3][3];

    transpose_matrix((float *)H, (float *)H_T, 3, 6);
    multiply_matrix((float *)H, (float *)P, (float *)HP, 3, 6, 6);
    multiply_matrix((float *)HP, (float *)H_T, (float *)HPH_T, 3, 6, 3);

    // Add R on diagonal
    HPH_T[0][0] += R;
    HPH_T[1][1] += R;
    HPH_T[2][2] += R;

    // S_inv = invert(S)
    float S_inv[3][3];
    if (invert_3x3_matrix(HPH_T, S_inv) != 0) {
        return; // singular, skip update
    }

    // K = P*Hᵀ*S_inv  (6x3 Kalman gain)
    float PH_T[6][3];
    float K[6][3];

    multiply_matrix((float *)P, (float *)H_T, (float *)PH_T, 6, 6, 3);
    multiply_matrix((float *)PH_T, (float *)S_inv, (float *)K, 6, 3, 3);

    // x = x + K*y
    float Ky[6];
    multiply_matrix((float *)K, y, Ky, 6, 3, 1);
    for (int i = 0; i < 6; i++) {
        X[i] += Ky[i];
    }

    // P = (I - K*H) * P
    float KH[6][6];
    float I_KH[6][6];
    multiply_matrix((float *)K, (float *)H, (float *)KH, 6, 3, 6);

    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            I_KH[i][j] = (i == j ? 1.0f : 0.0f) - KH[i][j];
        }
    }

    float P_new[6][6];
    multiply_matrix((float *)I_KH, (float *)P, (float *)P_new, 6, 6, 6);
    memcpy(P, P_new, sizeof(P));
}

void kalman_get_position(float *x, float *y, float *z)
{
    *x = X[0];
    *y = X[1];
    *z = X[2];
}
