#pragma once
#include <QString>
#include <QStringList>
#include <array>

#include "core/vocab.h"

namespace cmd {

struct SceneConfig {
    int missionId = 0;
    QString mode = "campaign";  // campaign | ai_training
    int level = 1;              // difficulty_level 1..5
    std::array<QString, DimCount> v;

    SceneConfig();

    QString get(int d) const { return v[static_cast<size_t>(d)]; }
    void set(int d, const QString& s) { v[static_cast<size_t>(d)] = s; }

    // scene_definition.json  (written for the simulator)
    QString toJsonText() const;
    bool writeFile(const QString& path, QString* err = nullptr) const;
    static bool fromJsonText(const QString& text, SceneConfig* out, QString* err = nullptr);
    static bool readFile(const QString& path, SceneConfig* out, QString* err = nullptr);

    // 0..100 weighted hardness of the chosen options
    double difficultyScore() const;
    // returns an empty string if the scene obeys all rules of the simulator
    QString validate() const;
    bool isValid() const { return validate().isEmpty(); }

    // short human texts
    QString headline() const;          // e.g. "NIGHT · FOG · FOREST"
    QString threatSummary() const;     // e.g. "2 swarms × 5 drones, converging, fast"
    QString sensorSummary() const;     // e.g. "BINOC ok · THERM degraded · ..."
    QStringList briefingLines() const; // multi-line briefing for the cards
    bool operator==(const SceneConfig& o) const;
};

int levelFromScore(double score);               // 1..5
QPair<double, double> levelBand(int level);     // inclusive difficulty-score band

}  // namespace cmd
