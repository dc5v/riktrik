#ifndef EVALUATE_H
#define EVALUATE_H

#include <stdlib.h>
#include <string.h>

#define C1_BINARY_ENTROPY     0b00000001
#define C1_EXPECTED_VALUE     0b00000010
#define C1_GEOMETRIC_MEAN     0b00000011
#define C1_HARMONIC_MEAN      0b00000100
#define C1_IQR                0b00000101
#define C1_MAE                0b00000110
#define C1_MEAD               0b00000111
#define C1_MEAN               0b00001000
#define C1_MEDIAN             0b00001001
#define C1_MODE               0b00001010
#define C1_PERCENT_RANGE      0b00001011
#define C1_RANGE              0b00001100
#define C1_RMS                0b00001101
#define C1_RMSLE              0b00001110
#define C2_CV                 0b00001111
#define C2_MAD                0b00010000
#define C2_MAXIMUM_DEVIATION  0b00010001
#define C2_MSE                0b00010010
#define C2_MSKEWNESS          0b00010011
#define C2_PERCENTILE         0b00010100
#define C2_STANDARD_DEVIATION 0b00010101
#define C2_TRIMMED_MEAN       0b00010110
#define C2_VARIANCE           0b00010111
#define C2_ZSKEWNESS          0b00011000
#define C3_KURTOSIS           0b00011001
#define C4R_QUARTILES         0b00011010

double eva_p1   ( int operation, size_t len, const double *data );
double eva_p2   ( int operation, size_t len, const double *data, double p1 );
double eva_p3   ( int operation, size_t len, const double *data, double p1, double p2 );
double eva_p4r3 ( int operation, size_t len, const double *data, double *p1, double *p2, double *p3 );
void   eva_sort ( double *data, int lh, int rh );

#endif