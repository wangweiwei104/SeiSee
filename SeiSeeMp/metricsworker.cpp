#include "metricsworker.h"
#include <algorithm>
#include <cmath>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
MetricsWorker::MetricsWorker(SeisFile* sf1,
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
                             QObject *parent)
    : Worker(parent)
{
    m_sf1 = sf1;
    m_sf2 = sf2;
    m_isSynthetic = isSynthetic;

    // DiffWorker 里是 ms -> sample index
    // tmmin/tmmax 输入单位是 ms，这里转换为采样点下标
    // 注意：tmmax 我也做成“右开区间”，方便循环
    m_tmmin = int((double)tmmin / (m_sf1->Si() * 1000.0));
    m_tmmax = int((double)tmmax / (m_sf1->Si() * 1000.0)) + 1;

    m_trmin = trmin;
    m_trmax = trmax;
    m_trstp = trstp;
    m_calcPSNR = calcPSNR;
    m_calcSSIM = calcSSIM;
    m_calcMSE  = calcMSE;
    m_calcSNR  = calcSNR;
    m_calcTPRS = calcTPRS;

}

double MetricsWorker::safe_log10(double x)
{
    // 避免 log10(0) / log10(负数)
    if (x <= 0) return -1e30;
    return std::log10(x);
}

// 1D SSIM：用滑窗(默认 win=11)在 1D 信号上做 SSIM，并取平均
// 公式：SSIM = ((2uxuy + C1)(2cov + C2))/((ux^2+uy^2 + C1)(vx+vy + C2))
// 这里 vx/vy 用局部方差，cov 为局部协方差
double MetricsWorker::ssim1d(const float* x, const float* y, int n, int win)
{
    if (!x || !y || n <= 1) return 0.0;
    if (win < 3) win = 3;
    if (win % 2 == 0) win += 1;
    if (win > n) win = (n % 2 == 1) ? n : n - 1;
    if (win < 3) return 0.0;

    int half = win / 2;

    // 估计动态范围 L（用 max-min）
    float xmin = x[0], xmax = x[0];
    float ymin = y[0], ymax = y[0];
    for (int i = 1; i < n; ++i)
    {
        xmin = std::min(xmin, x[i]); xmax = std::max(xmax, x[i]);
        ymin = std::min(ymin, y[i]); ymax = std::max(ymax, y[i]);
    }
    double L = std::max((double)(xmax - xmin), (double)(ymax - ymin));
    if (L <= 0) L = 1.0;

    // SSIM 常数（标准做法）
    double C1 = (0.01 * L) * (0.01 * L);
    double C2 = (0.03 * L) * (0.03 * L);

    // 逐点滑窗计算并平均
    double ssim_sum = 0.0;
    int    ssim_cnt = 0;

    for (int c = 0; c < n; ++c)
    {
        int l = std::max(0, c - half);
        int r = std::min(n - 1, c + half);
        int m = r - l + 1;

        double mux = 0.0, muy = 0.0;
        for (int i = l; i <= r; ++i)
        {
            mux += x[i];
            muy += y[i];
        }
        mux /= m;
        muy /= m;

        double vx = 0.0, vy = 0.0, cov = 0.0;
        for (int i = l; i <= r; ++i)
        {
            double dx = x[i] - mux;
            double dy = y[i] - muy;
            vx  += dx * dx;
            vy  += dy * dy;
            cov += dx * dy;
        }
        vx  /= m;
        vy  /= m;
        cov /= m;

        double num = (2.0 * mux * muy + C1) * (2.0 * cov + C2);
        double den = (mux * mux + muy * muy + C1) * (vx + vy + C2);

        double s = (den != 0.0) ? (num / den) : 0.0;
        ssim_sum += s;
        ssim_cnt++;
    }

    if (ssim_cnt < 1) return 0.0;
    return ssim_sum / ssim_cnt;
}

void MetricsWorker::process()
{
    QString mess;

    if (!m_sf1 || !m_sf1->Active())
    {
        mess = "Primary file is not active.";
        emit efin(mess);
        emit finished();
        return;
    }

    if (m_isSynthetic)
    {
        if (!m_sf2 || !m_sf2->Active())
        {
            mess = "Secondary file is not active (synthetic mode requires two files).";
            emit efin(mess);
            emit finished();
            return;
        }

        if (m_sf1->Nt() != m_sf2->Nt())
        {
            mess = "Traces must be same for synthetic metrics.";
            emit efin(mess);
            emit finished();
            return;
        }
        if (m_sf1->Ns() != m_sf2->Ns())
        {
            mess = "Samples must be same for synthetic metrics.";
            emit efin(mess);
            emit finished();
            return;
        }
    }

    int Ns = m_sf1->Ns();
    m_tmmin = std::max(0, m_tmmin);
    m_tmmax = std::min(Ns, m_tmmax);
    if (m_tmmax <= m_tmmin)
    {
        mess = "Time window is invalid.";
        emit efin(mess);
        emit finished();
        return;
    }

    long long Nt = m_sf1->Nt();
    m_trmin = std::max(0, m_trmin);
    m_trmax = std::min((int)Nt, m_trmax);
    if (m_trmax <= m_trmin)
    {
        mess = "Trace range is invalid.";
        emit efin(mess);
        emit finished();
        return;
    }

    int inc = m_trstp;
    if (inc < 1) inc = 1;

    int win = 11;

    // ---- synthetic: sf1 vs sf2 ----
    if (m_isSynthetic)
    {
        double mse_sum = 0.0;
        double sig_sum = 0.0;
        double ssim_sum = 0.0;
        double peak = 0.0;
        long long total_samples = 0;
        long long total_traces  = 0;

        long long trccount = (m_trmax - m_trmin);
        int pp = -1;

        for (long long tidx = m_trmin; tidx < m_trmax; tidx += inc)
        {
            float* clean  = m_sf1->Rs(tidx);
            float* noisy  = m_sf2->Rs(tidx);
            if (!clean || !noisy) continue;
            int n = (m_tmmax - m_tmmin);

            // 【改】peak 使用干净信号
            for (int i = m_tmmin; i < m_tmmax; ++i)
                   peak = std::max(peak, (double)std::fabs(clean[i]));

            // 【改】MSE 和信号功率
            for (int i = m_tmmin; i < m_tmmax; ++i)
               {
                   double noise = (double)noisy[i] - (double)clean[i];
                   mse_sum += noise * noise;
                   sig_sum += (double)clean[i] * (double)clean[i];
               }


            ssim_sum += ssim1d(clean + m_tmmin, noisy + m_tmmin, n, win);

            total_samples += n;
            total_traces++;

            int pers = (int)(100.0 * (double)(tidx - m_trmin) / (double)trccount);

            if (tidx % 10 == 0) // 每 10 个 trace 输出一次
                {
                    qDebug() << "tidx=" << tidx
                             << "m_trmin=" << m_trmin
                             << "trccount=" << trccount
                             << "pers=" << pers;
                }

                if (pp != pers)
                {
                    qDebug() << "Calling xpers:" << pers;  // ← 添加这行
                    emit eprogr(pers, "Computing metrics...");
                    pp = pers;
                }



            if (pp != pers)
            {
                emit eprogr(pers, "Computing metrics...");
            }

            if (m_stop) break;
        }

        if (total_samples <= 0 || total_traces <= 0)
        {
            mess = "No valid traces to compute.";
            emit efin(mess);
            emit finished();
            return;
        }

        double mse  = mse_sum / (double)total_samples;
        double rmse = std::sqrt(std::max(0.0, mse));
        double psnr = 0.0;
        if (mse <= 0.0) psnr = 1e30;
        else            psnr = 10.0 * safe_log10((peak * peak) / mse);

        double snr = 0.0;
        if (mse_sum <= 0.0) snr = 1e30;
        else                snr = 10.0 * safe_log10(sig_sum / mse_sum);

        double mean_ssim = ssim_sum / (double)total_traces;

        QString out;
        out += "Mode: Synthetic (two files)\n";
        out += QString("Trace range: [%1, %2), step=%3\n").arg(m_trmin).arg(m_trmax).arg(inc);
        out += QString("Time samples: [%1, %2)\n").arg(m_tmmin).arg(m_tmmax);
        out += QString("Total traces used: %1\n").arg(total_traces);
        out += QString("Total samples used: %1\n\n").arg((qlonglong)total_samples);

        if(m_calcMSE)
        {
            out += QString("MSE  = %1\n").arg(mse, 0, 'g', 10);
            out += QString("RMSE = %1\n").arg(rmse, 0, 'g', 10);
        }

        if(m_calcPSNR)
        {
            out += QString("PSNR = %1 dB\n").arg(psnr, 0, 'g', 10);
        }

        if(m_calcSNR)
        {
            out += QString("SNR  = %1 dB\n").arg(snr,  0, 'g', 10);
        }

        if(m_calcSSIM)
        {
            out += QString("SSIM = %1\n").arg(mean_ssim, 0, 'g', 10);
        }
        if (m_calcTPRS) {
            out += "TPRS: N/A in Synthetic mode (only for Real mode)\n";
        }

        emit emetrics(out);
        emit efin(out);
        emit finished();
        return;
    }

    // ---- real: only sf1 ----
    {
        int n = (m_tmmax - m_tmmin);
        std::vector<double> mean(n, 0.0);

        long long Nt = m_sf1->Nt();
        int inc = m_trstp; if (inc < 1) inc = 1;
        long long trccount = (m_trmax - m_trmin);

        // 经典指标？
        bool anyClassic = (m_calcPSNR || m_calcSSIM || m_calcMSE || m_calcSNR);

        // ★ 仅 TPRS 的快捷路径：不计算均值、不跑经典指标
        if (m_calcTPRS && !anyClassic)
        {
            emit eprogr(0, "TPRS: preparing...");
            double tprs = computeTPRS_realMode(/*pStart=*/0, /*pEnd=*/100);

            QString out;
            out += "Mode: Real (one file)\n";
            out += QString("Trace range: [%1, %2), step=%3\n").arg(m_trmin).arg(m_trmax).arg(inc);
            out += QString("Time samples: [%1, %2)\n").arg(m_tmmin).arg(m_tmmax);
            out += "\n";
            out += QString("TPRS = %1\n").arg(tprs, 0, 'g', 10);

            emit emetrics(out);
            emit efin(out);
            emit finished();
            return;
        }

        // ★ 其余情况：保留原流程（先均值，再经典指标），TPRS 放到最后 75%→99%
        long long used_tr = 0;
        int pp = -1;

        for (long long tidx = m_trmin; tidx < m_trmax; tidx += inc)
        {
            float* a = m_sf1->Rs(tidx);
            if (!a) continue;

            for (int i = 0; i < n; ++i)
                mean[i] += a[m_tmmin + i];

            used_tr++;

            int pers = (int)(50.0 * (double)(tidx - m_trmin) / (double)trccount);
            if (pp != pers)
            {
                emit eprogr(pers, "Building mean trace...");
                pp = pers;
            }
            if (m_stop) break;
        }

        if (used_tr <= 0)
        {
            QString mess = "No valid traces to compute (real mode).";
            emit efin(mess);
            emit finished();
            return;
        }

        for (int i = 0; i < n; ++i) mean[i] /= (double)used_tr;

        // 2) 经典指标（MSE/PSNR/SSIM/SNR）
        double mse_sum = 0.0;
        double sig_sum = 0.0;
        double ssim_sum = 0.0;
        double peak = 0.0;
        long long total_samples = 0;
        long long total_traces  = 0;

        pp = -1;
        int win = 11;
        for (int i = 0; i < n; ++i) peak = std::max(peak, std::fabs(mean[i]));

        for (long long tidx = m_trmin; tidx < m_trmax; tidx += inc)
        {
            float* a = m_sf1->Rs(tidx);
            if (!a) continue;

            for (int i = 0; i < n; ++i)
            {
                double d = (double)a[m_tmmin + i] - mean[i];
                mse_sum += d * d;
                sig_sum += mean[i] * mean[i];
            }

            std::vector<float> meanf(n);
            for (int i = 0; i < n; ++i) meanf[i] = (float)mean[i];

            if (m_calcSSIM) ssim_sum += ssim1d(a + m_tmmin, meanf.data(), n, win);

            total_samples += n;
            total_traces++;

            int pers = 50 + (int)(25.0 * (double)(tidx - m_trmin) / (double)trccount); // 50→75
            if (pp != pers)
            {
                emit eprogr(pers, "Computing classic metrics...");
                pp = pers;
            }
            if (m_stop) break;
        }

        double mse  = (total_samples > 0) ? (mse_sum / (double)total_samples) : 0.0;
        double rmse = std::sqrt(std::max(0.0, mse));
        double psnr = (mse <= 0.0) ? 1e30 : (10.0 * safe_log10((peak * peak) / mse));
        double snr  = (mse_sum <= 0.0) ? 1e30 : (10.0 * safe_log10(sig_sum / mse_sum));
        double mean_ssim = (total_traces > 0) ? (ssim_sum / (double)total_traces) : 0.0;

        QString out;
        out += "Mode: Real (one file)\n";
        out += "Reference: Mean trace of selected traces\n";
        out += QString("Trace range: [%1, %2), step=%3\n").arg(m_trmin).arg(m_trmax).arg(inc);
        out += QString("Time samples: [%1, %2)\n").arg(m_tmmin).arg(m_tmmax);
        out += QString("Total traces used: %1\n").arg(total_traces);
        out += QString("Total samples used: %1\n\n").arg((qlonglong)total_samples);

        if(m_calcMSE)  { out += QString("MSE  = %1\n").arg(mse, 0, 'g', 10);
                         out += QString("RMSE = %1\n").arg(rmse, 0, 'g', 10); }
        if(m_calcPSNR) { out += QString("PSNR = %1 dB\n").arg(psnr, 0, 'g', 10); }
        if(m_calcSNR)  { out += QString("SNR  = %1 dB\n").arg(snr,  0, 'g', 10); }
        if(m_calcSSIM) { out += QString("SSIM = %1\n").arg(mean_ssim, 0, 'g', 10); }

        // 3) TPRS（如果勾选），放到 75%→99%
        if (m_calcTPRS)
        {
            double tprs = computeTPRS_realMode(/*pStart=*/75, /*pEnd=*/99);
            out += QString("TPRS = %1\n").arg(tprs, 0, 'g', 10);
        }

        emit emetrics(out);
        emit efin(out);
        emit finished();
        return;
    }

}

#include <vector>
#include <complex>
#include <algorithm>
#include <cmath>
#include <QDebug>
#include <QString>


int MetricsWorker::nextPow2(int x)
{
    int p = 1;
    while (p < x) p <<= 1;
    return p;
}

// 迭代 Cooley–Tukey radix-2 FFT（in-place）
void MetricsWorker::fftRadix2(std::vector<std::complex<double>>& a)
{
    int n = (int)a.size();
    // bit-reversal
    for (int i = 1, j = 0; i < n; i++)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }

    for (int len = 2; len <= n; len <<= 1)
    {
        double ang = -2.0 * M_PI / (double)len;
        std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len)
        {
            std::complex<double> w(1.0, 0.0);
            for (int j = 0; j < len / 2; j++)
            {
                auto u = a[i + j];
                auto v = a[i + j + len/2] * w;
                a[i + j] = u + v;
                a[i + j + len/2] = u - v;
                w *= wlen;
            }
        }
    }
}

// 估计 b 相对 a 的整数时移：max_{lag in [-maxLag,maxLag]} sum a[i]*b[i+lag]
int MetricsWorker::estimateShift(const float* a, const float* b, int n, int maxLag)
{
    if (!a || !b || n <= 1) return 0;
    maxLag = std::max(0, std::min(maxLag, n - 1));

    double best = -1e300;
    int bestLag = 0;

    for (int lag = -maxLag; lag <= maxLag; ++lag)
    {
        double s = 0.0;
        int i0 = std::max(0, -lag);
        int i1 = std::min(n - 1, n - 1 - lag);
        for (int i = i0; i <= i1; ++i)
        {
            s += (double)a[i] * (double)b[i + lag];
        }
        if (s > best)
        {
            best = s;
            bestLag = lag;
        }
    }
    return bestLag;
}


void MetricsWorker::applyShift(const float* src, float* dst, int n, int shift)
{
    for (int i = 0; i < n; ++i)
    {
        int j = i + shift;
        dst[i] = (j >= 0 && j < n) ? src[j] : 0.0f;
    }
}

double MetricsWorker::computeTPRS_realMode(int pStart, int pEnd)
{
    auto prog = [&](int local0to100, const char* msg){
        int p = pStart + (int)((pEnd - pStart) * (local0to100 / 100.0));
        if (p < pStart) p = pStart;
        if (p > pEnd)   p = pEnd;
        emit eprogr(p, msg);
    };

    const int m = (m_tmmax - m_tmmin);
    int incTr = m_trstp;
    if (incTr < 1) incTr = 1;


    std::vector<std::vector<float>> traceData;
    traceData.reserve((m_trmax > m_trmin) ? ((m_trmax - m_trmin + incTr - 1) / incTr) : 0);

    for (long long tidx = m_trmin; tidx < m_trmax; tidx += incTr) {
        const float* a = m_sf1->Rs(tidx);
        if (a) {
            // 复制数据到独立的vector
            std::vector<float> trace(m);
            for (int i = 0; i < m; ++i) {
                trace[i] = a[m_tmmin + i];
            }
            traceData.push_back(std::move(trace));
        }
    }

    const int ntr = (int)traceData.size();
    if (ntr < 2 || m < 8) {
        prog(100, "TPRS: not enough data");
        return 0.0;
    }

    // 建立指针数组（指向各自的数据副本）
    std::vector<const float*> traces(ntr);
    for (int t = 0; t < ntr; ++t) {
        traces[t] = traceData[t].data();
    }

    int nfft = nextPow2(m);
    const int nhalf = nfft / 2;

    // ===== 验证数据确实不同 =====
    qDebug() << "===== Verifying trace data =====";
    for (int t = 0; t < std::min(3, ntr-1); ++t) {
        double diff = 0.0;
        for (int i = 0; i < m; ++i) {
            diff += std::abs(traces[t][i] - traces[t+1][i]);
        }
        qDebug() << "Sum|trace[" << t << "] - trace[" << t+1 << "]| =" << diff;
    }

    // ===== 第一步：计算 Px_avr =====
    std::vector<std::complex<double>> X(nfft);
    std::vector<double> Px_sum(nhalf, 0.0);

    for (int t = 0; t < ntr; ++t) {
        for (int i = 0; i < m; ++i)
            X[i] = std::complex<double>((double)traces[t][i], 0.0);
        for (int i = m; i < nfft; ++i)
            X[i] = std::complex<double>(0.0, 0.0);

        fftRadix2(X);

        for (int k = 0; k < nhalf; ++k) {
            Px_sum[k] += std::norm(X[k]);
        }

        if ((t & 0x3f) == 0) {
            prog((int)(40.0 * t / ntr), "TPRS: computing Px...");
            if (m_stop) return 0.0;
        }
    }

    std::vector<double> Px_avr(nhalf);
    for (int k = 0; k < nhalf; ++k)
        Px_avr[k] = Px_sum[k] / (double)ntr;

    // ===== 第二步：计算 Ps_avr =====
    std::vector<std::complex<double>> A(nfft), B(nfft);
    std::vector<double> Ps_sum(nhalf, 0.0);
    std::vector<float> bShift(m);

    int maxLag = std::min(200, m / 2);
    if (maxLag < 1) maxLag = 1;

    const int nPairs = ntr - 1;

    for (int j = 0; j < nPairs; ++j) {
        const float* a = traces[j];
        const float* b = traces[j + 1];

        int lag = estimateShift(a, b, m, maxLag);
        applyShift(b, bShift.data(), m, lag);

        for (int i = 0; i < m; ++i) {
            A[i] = std::complex<double>((double)a[i], 0.0);
            B[i] = std::complex<double>((double)bShift[i], 0.0);
        }
        for (int i = m; i < nfft; ++i) {
            A[i] = std::complex<double>(0.0, 0.0);
            B[i] = std::complex<double>(0.0, 0.0);
        }

        fftRadix2(A);
        fftRadix2(B);

        for (int k = 0; k < nhalf; ++k) {
            std::complex<double> c = A[k] * std::conj(B[k]);
            double ps_val = 2.0 * std::real(c);
            Ps_sum[k] += std::abs(ps_val);
        }

        if ((j & 0x1f) == 0) {
            prog(40 + (int)(60.0 * j / nPairs), "TPRS: computing Ps...");
            if (m_stop) return 0.0;
        }
    }

    std::vector<double> Ps_avr(nhalf);
    for (int k = 0; k < nhalf; ++k)
        Ps_avr[k] = Ps_sum[k] / (2.0 * (double)nPairs);

    // ===== 第三步：计算 TPRS =====
    double sumPs = 0.0, sumPx = 0.0;
    for (int k = 0; k < nhalf; ++k) {
        sumPs += Ps_avr[k];
        sumPx += Px_avr[k];
    }

    qDebug() << "sumPs:" << sumPs << "sumPx:" << sumPx << "ratio:" << (sumPs/sumPx);

    if (!(sumPx > 0.0) || !std::isfinite(sumPx) || !std::isfinite(sumPs)) {
        prog(100, "TPRS: invalid spectrum");
        return 0.0;
    }

    double tprs = sumPs / sumPx;
    if (!std::isfinite(tprs) || tprs < 0.0) tprs = 0.0;
    if (tprs > 1.0) tprs = 1.0;

    prog(100, "TPRS: done");
    return tprs;
}
