#pragma once
#include <QRandomGenerator>
#include <QVector>

#include "core/scene_config.h"

namespace cmd {

// Produces only scenes that satisfy SceneConfig::validate().
class Generator {
public:
    explicit Generator(quint32 seed = 0);  // 0 -> non deterministic

    // Random valid scene whose difficulty score lies inside the band of `level`.
    SceneConfig generate(int level, const QString& mode);
    // Random valid scene whose difficulty score is close to `target` (0..100).
    SceneConfig generateTarget(double target, double tolerance, const QString& mode);
    // Five missions of rising difficulty (levels 1..5).
    QVector<SceneConfig> campaign();
    // Many varied valid scenes spread over the whole difficulty range (for the recommender).
    QVector<SceneConfig> candidates(int count);

private:
    SceneConfig sample(double t);
    int pick(int d, double t, const QVector<int>& allowed = {});
    QRandomGenerator rng_;
};

}  // namespace cmd
