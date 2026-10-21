#pragma once
#include <QStringList>

#include "core/performance.h"
#include "core/scene_config.h"

namespace cmd {

struct Feedback {
    QStringList strengths;
    QStringList issues;
    QStringList tips;
};

Feedback buildFeedback(const SceneConfig& scene, const Performance& perf);
QString actualThreatName(const SceneConfig& scene);

}  // namespace cmd
