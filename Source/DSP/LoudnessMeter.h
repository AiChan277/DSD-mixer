#pragma once

#include "DSP/TruePeakDetector.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace dsd
{
    // ITU-R BS.1770-4 / EBU R128 loudness meter for a stereo bus.
    //   * K-weighting (pre-filter shelf + RLB high-pass), any sample rate
    //   * Momentary (400 ms), Short-term (3 s), Integrated (gated: -70 LUFS
    //     absolute, -10 LU relative) and maximum true peak.
    // process() runs on the audio thread (no allocation, no locks). Integrated
    // loudness is computed on demand from a histogram of 400 ms block
    // loudness values, so memory stays fixed however long the programme runs.
    class LoudnessMeter
    {
    public:
        void prepare(double sampleRate) noexcept
        {
            sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
            designFilters();
            segmentLength = static_cast<int>(std::lround(sr * 0.1)); // 100 ms
            resetAll();
        }

        void resetAll() noexcept
        {
            for (auto& s : state) s = {};
            segmentAccum = 0.0;
            segmentCount = 0;
            segments.fill(0.0);
            segmentsWritten = 0;
            for (auto& h : histogram) h.store(0, std::memory_order_relaxed);
            momentary.store(-200.0f, std::memory_order_relaxed);
            shortTerm.store(-200.0f, std::memory_order_relaxed);
            truePeakMax.store(0.0f, std::memory_order_relaxed);
            tpL.reset();
            tpR.reset();
            resetRequested.store(false, std::memory_order_relaxed);
        }

        // Any thread: clears integrated / max true peak at the next block.
        void requestReset() noexcept { resetRequested.store(true, std::memory_order_relaxed); }

        void process(const float* l, const float* r, int n) noexcept
        {
            if (resetRequested.load(std::memory_order_relaxed))
                resetAll();
            if (l == nullptr || r == nullptr || n <= 0)
                return;

            float tpMax = truePeakMax.load(std::memory_order_relaxed);
            for (int i = 0; i < n; ++i)
            {
                tpMax = std::max(tpMax, std::max(tpL.push(l[i]), tpR.push(r[i])));

                const double kl = kWeight(0, l[i]);
                const double kr = kWeight(1, r[i]);
                segmentAccum += kl * kl + kr * kr;

                if (++segmentCount >= segmentLength)
                    finishSegment();
            }
            truePeakMax.store(tpMax, std::memory_order_relaxed);
        }

        float getMomentaryLufs() const noexcept { return momentary.load(std::memory_order_relaxed); }
        float getShortTermLufs() const noexcept { return shortTerm.load(std::memory_order_relaxed); }
        float getTruePeakMaxDb() const noexcept
        {
            const float tp = truePeakMax.load(std::memory_order_relaxed);
            return tp > 1.0e-9f ? 20.0f * std::log10(tp) : -200.0f;
        }

        // Gated integrated loudness (LUFS); -200 when nothing above the gate yet.
        float getIntegratedLufs() const noexcept
        {
            // Pass 1: absolute gate.
            double energy = 0.0;
            uint64_t count = 0;
            for (int b = 0; b < kBins; ++b)
            {
                const uint64_t c = histogram[static_cast<size_t>(b)].load(std::memory_order_relaxed);
                if (c == 0) continue;
                energy += static_cast<double>(c) * binEnergy(b);
                count += c;
            }
            if (count == 0)
                return -200.0f;

            const double relGate = energyToLufs(energy / static_cast<double>(count)) - 10.0;

            // Pass 2: relative gate.
            energy = 0.0;
            count = 0;
            for (int b = 0; b < kBins; ++b)
            {
                if (binLufs(b) < relGate) continue;
                const uint64_t c = histogram[static_cast<size_t>(b)].load(std::memory_order_relaxed);
                energy += static_cast<double>(c) * binEnergy(b);
                count += c;
            }
            return count == 0 ? -200.0f : static_cast<float>(energyToLufs(energy / static_cast<double>(count)));
        }

        static double energyToLufs(double meanSquare) noexcept
        {
            return meanSquare > 1.0e-20 ? -0.691 + 10.0 * std::log10(meanSquare) : -200.0;
        }

    private:
        struct Biquad { double b0, b1, b2, a1, a2; };
        struct State { double z1[2]{}, z2[2]{}; };

        // 0.1 LU bins from -70 LUFS (absolute gate) to +5 LUFS.
        static constexpr int kBins = 750;
        static constexpr double kBinMin = -70.0, kBinWidth = 0.1;
        static double binLufs(int b) noexcept { return kBinMin + (b + 0.5) * kBinWidth; }
        static double binEnergy(int b) noexcept { return std::pow(10.0, (binLufs(b) + 0.691) / 10.0); }

        void designFilters() noexcept
        {
            const double pi = 3.14159265358979323846;
            {   // Stage 1: high-shelf pre-filter
                const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
                const double K = std::tan(pi * f0 / sr);
                const double Vh = std::pow(10.0, G / 20.0);
                const double Vb = std::pow(Vh, 0.4996667741545416);
                const double a0 = 1.0 + K / Q + K * K;
                stage[0] = { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0,
                             (Vh - Vb * K / Q + K * K) / a0, 2.0 * (K * K - 1.0) / a0,
                             (1.0 - K / Q + K * K) / a0 };
            }
            {   // Stage 2: RLB high-pass
                const double f0 = 38.13547087602444, Q = 0.5003270373238773;
                const double K = std::tan(pi * f0 / sr);
                const double a0 = 1.0 + K / Q + K * K;
                stage[1] = { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
            }
        }

        inline double kWeight(int ch, float x) noexcept
        {
            double v = x;
            for (int s = 0; s < 2; ++s)
            {
                const auto& c = stage[s];
                auto& st = state[static_cast<size_t>(ch)];
                const double out = c.b0 * v + st.z1[s];
                st.z1[s] = c.b1 * v - c.a1 * out + st.z2[s];
                st.z2[s] = c.b2 * v - c.a2 * out;
                v = out;
            }
            return v;
        }

        void finishSegment() noexcept
        {
            const double ms = segmentAccum / static_cast<double>(segmentCount);
            segments[static_cast<size_t>(segmentsWritten % kSegments)] = ms;
            ++segmentsWritten;
            segmentAccum = 0.0;
            segmentCount = 0;

            auto meanOfLast = [this](int count) {
                const int avail = static_cast<int>(std::min<uint64_t>(segmentsWritten, static_cast<uint64_t>(count)));
                double sum = 0.0;
                for (int k = 0; k < avail; ++k)
                    sum += segments[static_cast<size_t>((segmentsWritten - 1 - static_cast<uint64_t>(k)) % kSegments)];
                return avail > 0 ? sum / avail : 0.0;
            };

            const double m = energyToLufs(meanOfLast(4));
            momentary.store(static_cast<float>(m), std::memory_order_relaxed);
            shortTerm.store(static_cast<float>(energyToLufs(meanOfLast(30))), std::memory_order_relaxed);

            // Every 100 ms a new 400 ms gating block (75% overlap).
            if (segmentsWritten >= 4 && m >= kBinMin)
            {
                const int bin = std::clamp(static_cast<int>((m - kBinMin) / kBinWidth), 0, kBins - 1);
                histogram[static_cast<size_t>(bin)].fetch_add(1, std::memory_order_relaxed);
            }
        }

        static constexpr int kSegments = 32;

        double sr{48000.0};
        Biquad stage[2]{};
        std::array<State, 2> state{};
        int segmentLength{4800};
        double segmentAccum{0.0};
        int segmentCount{0};
        std::array<double, kSegments> segments{};
        uint64_t segmentsWritten{0};
        TruePeakDetector tpL, tpR;

        std::array<std::atomic<uint64_t>, kBins> histogram{};
        std::atomic<float> momentary{-200.0f};
        std::atomic<float> shortTerm{-200.0f};
        std::atomic<float> truePeakMax{0.0f};
        std::atomic<bool> resetRequested{false};
    };
} // namespace dsd
