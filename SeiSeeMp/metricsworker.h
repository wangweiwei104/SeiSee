#ifndef METRICSWORKER_H
#define METRICSWORKER_H
#include "workthread.h"
#include <complex>
#include <vector>
class MetricsWorker : public Worker
{
    Q_OBJECT

public:
    MetricsWorker(SeisFile* sf1,
                  SeisFile* sf2,
                  bool    isSynthetic,
                  int     trmin,
                  int     trmax,
                  int     trstp,
                  int     tmmin,
                  int     tmmax,
                  bool    calcPSNR,
                  bool    calcSSIM,
                  bool    calcMSE,
                  bool    calcSNR,
                  bool    calcTPRS,
                  QObject *parent = nullptr);

public slots:
    void process();

signals:
    void emetrics(QString results);
    void eprogr(int pers, QString mess);
    void efin(QString mess);

private:
    SeisFile* m_sf1;         // 主文件
    SeisFile* m_sf2;         // 副文件（合成模式）
    bool      m_isSynthetic; // 模式：true=合成，false=实际
    int       m_trmin;       // 道范围
    int       m_trmax;
    int       m_trstp;
    int       m_tmmin;       // 时间范围（采样点索引）
    int       m_tmmax;
    bool      m_calcPSNR;
    bool      m_calcSSIM;
    bool      m_calcMSE;
    bool      m_calcSNR;
    bool      m_calcTPRS;


    double computeTPRS_realMode(int pStart = 75, int pEnd = 99); // 仅真实模式用

    // FFT & shift helpers
    static int nextPow2(int x);
    static void fftRadix2(std::vector<std::complex<double>>& a);
    static int  estimateShift(const float* a, const float* b, int n, int maxLag);
    static void applyShift(const float* src, float* dst, int n, int shift);


    // 辅助函数
    double safe_log10(double x);
    double ssim1d(const float* x, const float* y, int n, int win = 11);
};




#endif
