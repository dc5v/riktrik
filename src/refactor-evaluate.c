
/*  이진 엔트로피 (Binary Entropy) */
double c_bin_entropy ( const double *data, size_t len );

/*  변동계수 (Coefficient of Variation, CV) */
double c_cv ( double mean, double standard_deviation );

/*  기대값 (Expected Value) */
double c_exp_value ( const double *data, size_t len );

/*  기하평균 (Geometric Mean) */
double c_geo_mean ( const double *data, size_t len );

/*  조화평균 (Harmonic Mean) */
double c_harm_mean ( const double *data, size_t len );

/*  사분위범위 (Interquartile Range, IQR) */
double c_iqr ( double *data, size_t len );

/*  첨도 (Kurtosis)# */
double c_kurtosis ( const double *data, size_t len, double mean, double standard_deviation );

/*  평균 절대 편차 (Mean Absolute Deviation, MAD) */
double c_mad ( double *data, size_t len, double median );

/*  평균 절대 편차 (Mean Absolute Error, MAE) */
double c_mae ( const double *data, size_t len );

/*  최대 차이 (Maximum Deviation) */
double c_max_deviation ( const double *data, size_t len, double mean );

/*  절대 편차 중앙값 (Median Absolute Deviation, MeAD) */
double c_mead ( double *data, size_t len );

/*  평균값 (Mean) */
double c_mean ( const double *data, size_t len );

/*  중위값 (Median) */
double c_median ( double *data, size_t len );

/*  최빈값 (Mode) */
double c_mode ( double *data, size_t len );

/*  평균 제곱 편차 (Mean Squared Error, MSE) */
double c_mse ( const double *data, size_t len, double mean );

/*  왜도(MSkewness) : 중위값 기준 왜도 */
double c_mskewness ( double *data, size_t len, double median );

/*  퍼센트 범위 (PercentRange) */
double c_percent_range ( const double *data, size_t len );

/*  백분위슈 (Percentiles) */
double c_percentile ( double *data, size_t len, double percentile );

/*  범위 (Range) */
double c_range ( const double *data, size_t len );

/*  제곱평균 (Root Mean Square, RMS) */
double c_rms ( const double *data, size_t len );

/*  제곱평균 로그 (RMSLE) */
double c_rmsle ( const double *data, size_t len );

/*  표준편차 (Standard Deviation) */
double c_std_deviation ( const double *data, size_t len, double mean );

/*  절사평균 (Trimmed Mean) */
double c_trim_mean ( double *data, size_t len, double trim_ratio );

/*  분산 (Variance) */
double c_variance ( const double *data, size_t len, double mean );

/*  왜도(ZSkewness) : 0 기준 왜도 */
double c_zskewness ( const double *data, size_t len, double mean );

/*  사분위수 (Quartiles) */
void c_quartiles ( double *data, size_t len, double *q1, double *q2, double *q3 );