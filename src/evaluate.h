#ifndef EVALUATE_H
#define EVALUATE_H

#include <stdlib.h>
#include <string.h>

#define EVA_CV_P2                 0b00001111 // 변동계수 (Coefficient of Variation, CV)
#define EVA_DEVIATION_MAXIMUM_P2  0b00010001 // 최대차이 (Maximum Deviation)
#define EVA_DEVIATION_STANDARD_P2 0b00010101 // 표준편차 (Standard Deviation)
#define EVA_ENTROPY_BINARY_P1     0b00000001 // 이진 엔트로피 (Binary Entropy)
#define EVA_EXPECTED_P1           0b00000010 // 기대값 (Expected Value)
#define EVA_IQR_P1                0b00000101 // 사분위범위 (Interquartile Range, IQR)
#define EVA_KURTOSIS_P3           0b00011001 // 첨도 (Kurtosis)
#define EVA_MAD_P2                0b00010000 // 평균 절대 편차 (Mean Absolute Deviation, MAD)
#define EVA_MAE_P1                0b00000110 // 평균 절대 편차 (Mean Absolute Error, MAE)
#define EVA_MEAD_P1               0b00000111 // 절대 편차 중앙값 (Median Absolute Deviation, MeAD)
#define EVA_MEAN_GEOMETRIC_P1     0b00000011 // 기하평균 (Geometric Mean)
#define EVA_MEAN_HARMONIC_P1      0b00000100 // 조화평균 (Harmonic Mean)
#define EVA_MEAN_P1               0b00001000 // 평균값 (Mean)
#define EVA_MEAN_TRIMMED_P2       0b00010110 // 절사평균 (Trimmed Mean)
#define EVA_MEDIAN_P1             0b00001001 // 중위값 (Median)
#define EVA_MODE_P1               0b00001010 // 최빈값 (Mode)
#define EVA_MSE_P2                0b00010010 // 평균 제곱 편차 (Mean Squared Error, MSE)
#define EVA_PERCENTILE_P2         0b00010100 // 백분위수 (Percentiles)
#define EVA_QUARTILES_P4R3        0b00011010 // 사분위수 (Quartiles)
#define EVA_RANGE_P1              0b00001100 // 범위 (Range)
#define EVA_RANGE_PERCENT_P1      0b00001011 // 퍼센트 범위 (PercentRange)
#define EVA_RMS_P1                0b00001101 // 제곱평균 (Root Mean Square, RMS)
#define EVA_RMSLE_P1              0b00001110 // 제곱평균 로그 (RMSLE)
#define EVA_SKEWNESS_M_P2         0b00010011 // 중위값 기준 왜도 (Median Skewness)
#define EVA_SKEWNESS_Z_P2         0b00011000 // 왜도(Skewness)
#define EVA_VARIANCE_P2           0b00010111 // 분산 (Variance)

double eva_p1 ( int operation, size_t len, const double *data );
double eva_p2 ( int operation, size_t len, const double *data, const double p1 );
double eva_p3 ( int operation, size_t len, const double *data, const double p1, const double p2 );
double eva_p4r3 ( int operation, size_t len, const double *data, double *p1, double *p2, double *p3 );

#endif