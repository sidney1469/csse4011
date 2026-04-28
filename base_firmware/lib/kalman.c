#include <string.h>
#include <math.h>
#include "kalman.h"

#define Q 0.01f
#define R 4.0f

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
    float dt2 = dt * dt;
    float dt3 = dt2 * dt;
    float dt4 = dt2 * dt2;

    // Position-position
    P[0][0] += Q * dt4 / 4.0f;
    P[1][1] += Q * dt4 / 4.0f;
    // Velocity-velocity
    P[2][2] += Q * dt2;
    P[3][3] += Q * dt2;
    // Position-velocity cross terms
    P[0][2] += Q * dt3 / 2.0f;
    P[2][0] += Q * dt3 / 2.0f;
    P[1][3] += Q * dt3 / 2.0f;
    P[3][1] += Q * dt3 / 2.0f;
}

void kalman_update(float x_meas, float y_meas)
{
    // H: state-to-measurement matrix (3x6)
    float H[2][4] = {
        {1, 0, 0, 0},
        {0, 1, 0, 0},
    };

    // y = z - H*x
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

    // P = IKH * P * IKH_T + K * R * K_T
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
}

void kalman_get_position(float *x, float *y)
{
    *x = X[0];
    *y = X[1];
}

void kalman_get_velocity(float *x, float *y)
{
    *x = X[2];
    *y = X[3];
}
