#include "core/vocab.h"

#include <array>

namespace cmd {

namespace {

std::array<DimInfo, DimCount> build() {
    std::array<DimInfo, DimCount> a;
    auto set = [&](Dim d, const char* group, const char* key, const char* label, const char* cat,
                   QStringList vals, QVector<double> hard, double w, bool numeric = false) {
        a[d] = DimInfo{group, key, label, cat, std::move(vals), std::move(hard), w, numeric};
    };
    set(DTime, "environment", "time", "Time of day", "Environment", {"day", "twilight", "night"}, {0.0, 0.5, 1.0}, 1.0);
    set(DWeather, "environment", "weather", "Weather", "Environment", {"clear", "cloudy", "rain", "fog", "storm"},
        {0.0, 0.2, 0.55, 0.75, 1.0}, 1.0);
    set(DTerrain, "environment", "terrain", "Terrain", "Environment", {"rural", "urban", "forest", "coastal"},
        {0.0, 0.7, 1.0, 0.35}, 0.7);
    set(DVisibility, "environment", "visibility", "Visibility", "Environment",
        {"excellent", "good", "moderate", "poor", "very_poor"}, {0.0, 0.25, 0.5, 0.75, 1.0}, 1.2);

    set(DBinoculars, "sensors", "binoculars", "Binoculars", "Sensors", {"available", "degraded", "unavailable"},
        {0.0, 0.6, 1.0}, 0.8);
    set(DAudio, "sensors", "audio", "Acoustic sensor", "Sensors", {"high", "medium", "low"}, {0.0, 0.5, 1.0}, 0.7);
    set(DSignal, "sensors", "signal", "RF signal sensor", "Sensors", {"available", "degraded", "unavailable"},
        {0.0, 0.6, 1.0}, 0.7);
    set(DThermal, "sensors", "thermal", "Thermal imager", "Sensors", {"available", "degraded", "unavailable"},
        {0.0, 0.6, 1.0}, 0.8);

    set(DClue, "information", "clue", "Intel clue", "Intel", {"none", "partial", "detailed"}, {1.0, 0.5, 0.0}, 1.0);
    set(DDecoys, "distractions", "decoys", "Decoys", "Intel", {"none", "noise", "blast"}, {0.0, 0.5, 1.0}, 0.9);

    set(DThreatType, "threat", "type", "Threat type", "Threat", {"single", "swarm", "multiple_swarm"},
        {0.0, 0.6, 1.0}, 1.4);
    set(DSwarmCount, "threat", "swarm_count", "Swarm count", "Threat", {"1", "2", "3"}, {0.0, 0.5, 1.0}, 1.0, true);
    set(DDroneCount, "threat", "drone_count_per_swarm", "Drones per swarm", "Threat", {"1", "5", "10"},
        {0.0, 0.5, 1.0}, 1.0, true);
    set(DPosition, "threat", "position", "Swarm positions", "Threat", {"same", "different"}, {0.0, 1.0}, 0.6);
    set(DApproach, "threat", "approach", "Approach", "Threat", {"stationary", "crossing", "converging"},
        {0.2, 0.6, 1.0}, 0.8);
    set(DSpeed, "threat", "speed", "Speed", "Threat", {"slow", "normal", "fast"}, {0.0, 0.5, 1.0}, 0.9);

    set(DDetWin, "time_pressure", "detection_window", "Detection window", "Time pressure",
        {"long", "moderate", "short", "very_short"}, {0.0, 1.0 / 3, 2.0 / 3, 1.0}, 1.0);
    set(DClsWin, "time_pressure", "classification_window", "Classification window", "Time pressure",
        {"long", "moderate", "short", "very_short"}, {0.0, 1.0 / 3, 2.0 / 3, 1.0}, 1.0);
    set(DRspWin, "time_pressure", "response_window", "Response window", "Time pressure",
        {"long", "moderate", "short", "very_short"}, {0.0, 1.0 / 3, 2.0 / 3, 1.0}, 1.2);
    return a;
}

const std::array<DimInfo, DimCount>& table() {
    static const std::array<DimInfo, DimCount> t = build();
    return t;
}

double lookup(const QString& v, double a, double b, double c, double d) {
    if (v == "long") return a;
    if (v == "moderate") return b;
    if (v == "short") return c;
    return d;
}

}  // namespace

const DimInfo& dim(int d) { return table()[static_cast<size_t>(d)]; }

int valueIndex(int d, const QString& v) { return dim(d).values.indexOf(v); }

QString valueLabel(int d, const QString& v) {
    QString s = v;
    s.replace('_', ' ');
    if (d == DSwarmCount) return v == "1" ? "1 swarm" : v + " swarms";
    if (d == DDroneCount) return v == "1" ? "1 drone" : v + " drones";
    if (!s.isEmpty()) s[0] = s[0].toUpper();
    return s;
}

QStringList categories() { return {"Environment", "Sensors", "Intel", "Threat", "Time pressure"}; }

QVector<int> dimsOfCategory(const QString& cat) {
    QVector<int> r;
    for (int d = 0; d < DimCount; ++d)
        if (dim(d).category == cat) r.push_back(d);
    return r;
}

int featureOffset(int d) {
    int o = 0;
    for (int i = 0; i < d; ++i) o += dim(i).values.size();
    return o;
}

int featureCount() { return featureOffset(DimCount); }

double detectionWindowSeconds(const QString& v) { return lookup(v, 70, 45, 28, 16); }
double classificationWindowSeconds(const QString& v) { return lookup(v, 40, 26, 16, 9); }
double responseWindowSeconds(const QString& v) { return lookup(v, 30, 20, 12, 7); }

QStringList modes() { return {"campaign", "ai_training"}; }

}  // namespace cmd
