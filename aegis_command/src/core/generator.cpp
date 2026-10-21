#include "core/generator.h"

#include <cmath>

namespace cmd {

Generator::Generator(quint32 seed) : rng_(seed == 0 ? QRandomGenerator::global()->generate() : seed) {}

// Chooses a value index of dimension d. Options whose hardness is near t (0..1) are preferred.
int Generator::pick(int d, double t, const QVector<int>& allowed) {
    const DimInfo& di = dim(d);
    QVector<int> idx = allowed;
    if (idx.isEmpty())
        for (int i = 0; i < di.values.size(); ++i) idx.push_back(i);
    QVector<double> w;
    double sum = 0;
    for (int i : idx) {
        double x = di.hardness[i] - t;
        double p = std::exp(-(x * x) / (2.0 * 0.28 * 0.28)) + 0.04;
        w.push_back(p);
        sum += p;
    }
    double r = rng_.generateDouble() * sum;
    for (int k = 0; k < idx.size(); ++k) {
        r -= w[k];
        if (r <= 0) return idx[k];
    }
    return idx.back();
}

SceneConfig Generator::sample(double t) {
    SceneConfig s;
    auto setIdx = [&](int d, int i) { s.set(d, dim(d).values[i]); };
    t = std::min(1.0, std::max(0.0, t));
    for (int d : {DTime, DWeather, DTerrain, DVisibility, DBinoculars, DAudio, DSignal, DThermal, DClue, DDecoys, DApproach,
                  DSpeed, DDetWin, DClsWin, DRspWin})
        setIdx(d, pick(d, std::min(1.0, std::max(0.0, t + (rng_.generateDouble() - 0.5) * 0.3))));
    int type = pick(DThreatType, t);
    setIdx(DThreatType, type);
    const QString ty = dim(DThreatType).values[type];
    if (ty == "single") {
        s.set(DSwarmCount, "1");
        s.set(DDroneCount, "1");
        s.set(DPosition, "same");
    } else if (ty == "swarm") {
        s.set(DSwarmCount, "1");
        setIdx(DDroneCount, pick(DDroneCount, t, {1, 2}));
        s.set(DPosition, "same");
    } else {
        setIdx(DSwarmCount, pick(DSwarmCount, t, {1, 2}));
        setIdx(DDroneCount, pick(DDroneCount, t, {1, 2}));
        setIdx(DPosition, pick(DPosition, t));
    }
    return s;
}

SceneConfig Generator::generateTarget(double target, double tolerance, const QString& mode) {
    double tol = tolerance;
    for (int round = 0; round < 6; ++round) {
        for (int i = 0; i < 1500; ++i) {
            SceneConfig s = sample(target / 100.0);
            if (!s.isValid()) continue;
            if (std::fabs(s.difficultyScore() - target) > tol) continue;
            s.mode = mode;
            s.level = levelFromScore(s.difficultyScore());
            return s;
        }
        tol *= 1.6;
    }
    // Practically unreachable; return a valid scene regardless.
    SceneConfig s;
    s.mode = mode;
    s.level = levelFromScore(s.difficultyScore());
    return s;
}

SceneConfig Generator::generate(int level, const QString& mode) {
    level = std::min(5, std::max(1, level));
    const auto band = levelBand(level);
    double lo = band.first, hi = band.second;
    if (level == 1) lo = 2.0;
    if (level == 5) hi = 92.0;
    for (int tries = 0; tries < 400; ++tries) {
        double target = lo + (hi - lo) * rng_.generateDouble();
        SceneConfig s = generateTarget(target, 4.0, mode);
        double sc = s.difficultyScore();
        if (sc >= band.first && sc < band.second + (level == 5 ? 1e9 : 0.0)) {
            s.level = level;
            return s;
        }
    }
    SceneConfig s = generateTarget((lo + hi) / 2, 6.0, mode);
    s.level = level;
    return s;
}

QVector<SceneConfig> Generator::campaign() {
    QVector<SceneConfig> out;
    for (int level = 1; level <= 5; ++level) {
        SceneConfig s;
        for (int tries = 0; tries < 50; ++tries) {
            s = generate(level, "campaign");
            bool same = false;
            for (const auto& o : out)
                if (o.get(DTime) == s.get(DTime) && o.get(DWeather) == s.get(DWeather) && o.get(DTerrain) == s.get(DTerrain))
                    same = true;
            if (!same) break;
        }
        out.push_back(s);
    }
    return out;
}

QVector<SceneConfig> Generator::candidates(int count) {
    QVector<SceneConfig> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        double target = 6.0 + 88.0 * (static_cast<double>(i) + rng_.generateDouble()) / count;
        out.push_back(generateTarget(target, 5.0, "ai_training"));
    }
    return out;
}

}  // namespace cmd
