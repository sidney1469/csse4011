#include <string.h>
#include <math.h>
#include "kalman.h"

#define Q_POS 0.05f
#define Q_VEL 0.001f
#define R     2.0f // measurement noise variance (units: m²)

static float X[4];
static float P[4][4];

void init_filter(float x0, float y0)
{
    memset(X, 0, sizeof(X));
    X[0] = x0;
    X[1] = y0;

    memset(P, 0, sizeof(P));

    // Set initial uncertainty (Can be toggled)
    P[0][0] = P[1][1] = 1.0f; // position
    P[2][2] = P[3][3] = 0.1f; // velocity
}

void kalman_predict(float dt)
{
    float F[4][4] = {
        {1, 0, dt, 0},
        {0, 1, 0, dt},
        {0, 0, 1, 0},
        {0, 0, 0, 1},
    };

    // State transition: F @ x
    float X_temp[4];
    multiply_matrix((float *)F, X, X_temp, 4, 4, 1);
    for (int i = 0; i < 4; i++) {
        X[i] = X_temp[i];
    }
    // P = F * P * F_T + Q
    float F_T[4][4];
    float FP[4][4];
    float FPF_T[4][4];

    transpose_matrix((float *)F, (float *)F_T, 4, 4);

    multiply_matrix((float *)F, (float *)P, (float *)FP, 4, 4, 4);
    multiply_matrix((float *)FP, (float *)F_T, (float *)FPF_T, 4, 4, 4);

    // Write back into P and add Q
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            P[i][j] = FPF_T[i][j];
        }
    }

    // Add process noise Q
    for (int i = 0; i < 2; i++) {
        P[i][i] += Q_POS;
        P[i + 2][i + 2] += Q_VEL;
    }
}

void kalman_update(float x_meas, float y_meas)
{
    // H: state-to-measurement matrix (3x6)
    float H[2][4] = {
        {1, 0, 0, 0},
        {0, 1, 0, 0},
    };

    // Innovation: y = z - H*x
    float HX[2];
    multiply_matrix((float *)H, X, HX, 2, 4, 1);
    float y[2] = {
        x_meas - HX[0],
        y_meas - HX[1],
    };

    // S = H*P*Hᵀ + R*I
    float H_T[4][2];
    float HP[2][4];
    float HPH_T[2][2];

    transpose_matrix((float *)H, (float *)H_T, 2, 4);
    multiply_matrix((float *)H, (float *)P, (float *)HP, 2, 4, 4);
    multiply_matrix((float *)HP, (float *)H_T, (float *)HPH_T, 2, 4, 4);

    // Add R on diagonal
    HPH_T[0][0] += R;
    HPH_T[1][1] += R;

    // S_inv = invert(S)
    float S_inv[2][2];
    if (invert_2x2_matrix(HPH_T, S_inv) != 0) {
        return; // singular, skip update
    }

    // K = P*Hᵀ*S_inv  (6x3 Kalman gain)
    float PH_T[4][2];
    float K[4][2];

    multiply_matrix((float *)P, (float *)H_T, (float *)PH_T, 4, 4, 2);
    multiply_matrix((float *)PH_T, (float *)S_inv, (float *)K, 4, 2, 2);

    // x = x + K*y
    float Ky[4];
    multiply_matrix((float *)K, y, Ky, 4, 2, 1);
    for (int i = 0; i < 4; i++) {
        X[i] += Ky[i];
    }

    // P = (I - K*H) * P
    float KH[4][4];
    float I_KH[4][4];
    multiply_matrix((float *)K, (float *)H, (float *)KH, 4, 2, 4);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            I_KH[i][j] = (i == j ? 1.0f : 0.0f) - KH[i][j];
        }
    }

    float P_new[4][4];
    multiply_matrix((float *)I_KH, (float *)P, (float *)P_new, 4, 4, 4);
    memcpy(P, P_new, sizeof(P));
}

void kalman_get_position(float *x, float *y)
{
    *x = X[0];
    *y = X[1];
}
