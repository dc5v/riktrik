#ifndef EVALUATE_H
#define EVALUATE_H

#include <stdlib.h>
#include <string.h>

#define EVA_CV_P2                 0b00001111
#define EVA_DEVIATION_MAXIMUM_P2  0b00010001
#define EVA_DEVIATION_STANDARD_P2 0b00010101
#define EVA_ENTROPY_BINARY_P1     0b00000001
#define EVA_EXPECTED_P1           0b00000010
#define EVA_IQR_P1                0b00000101
#define EVA_KURTOSIS_P3           0b00011001
#define EVA_MAD_P2                0b00010000
#define EVA_MAE_P1                0b00000110
#define EVA_MEAD_P1               0b00000111
#define EVA_MEAN_GEOMETRIC_P1     0b00000011
#define EVA_MEAN_HARMONIC_P1      0b00000100
#define EVA_MEAN_P1               0b00001000
#define EVA_MEAN_TRIMMED_P2       0b00010110
#define EVA_MEDIAN_P1             0b00001001
#define EVA_MOD_P1E               0b00001010
#define EVA_MSE_P2                0b00010010
#define EVA_PERCENTILE_P2         0b00010100
#define EVA_QUARTILES_P4R3        0b00011010
#define EVA_RANGE_P1              0b00001100
#define EVA_RANGE_PERCENT_P1      0b00001011
#define EVA_RMS_P1                0b00001101
#define EVA_RMSLE_P1              0b00001110
#define EVA_SKEWNESS_M_P2         0b00010011
#define EVA_SKEWNESS_Z_P2         0b00011000
#define EVA_VARIANCE_P2           0b00010111

double eva_p1   ( int operation, size_t len, const double *data );
double eva_p2   ( int operation, size_t len, const double *data, const double p1 );
double eva_p3   ( int operation, size_t len, const double *data, const double p1, const double p2 );
double eva_p4r3 ( int operation, size_t len, const double *data, double *p1, double *p2, double *p3 );

#endif