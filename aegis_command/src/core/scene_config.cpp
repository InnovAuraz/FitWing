#include "core/scene_config.h"

#include <QFile>
#include <QSaveFile>
#include "json.hpp"

namespace cmd {

using ojson = nlohmann::ordered_json;

SceneConfig::SceneConfig() {
    for (int d = 0; d < DimCount; ++d) v[static_cast<size_t>(d)] = dim(d).values.first();
    set(DTime, "day");
    set(DWeather, "clear");
    set(DTerrain, "rural");
    set(DVisibility, "excellent");
    set(DBinoculars, "available");
    set(DAudio, "high");
    set(DSignal, "available");
    set(DThermal, "available");
    set(DClue, "detailed");
    set(DDecoys, "none");
    set(DThreatType, "single");
    set(DSwarmCount, "1");
    set(DDroneCount, "1");
    set(DPosition, "same");
    set(DApproach, "converging");
    set(DSpeed, "normal");
    set(DDetWin, "long");
    set(DClsWin, "long");
    set(DRspWin, "long");
}

static ojson buildJson(const SceneConfig& s) {
    ojson j;
    j["mission"] = {{"id", s.missionId}, {"mode", s.mode.toStdString()}, {"difficulty_level", s.level}};
    for (int d = 0; d < DimCount; ++d) {
        const DimInfo& di = dim(d);
        ojson& g = j[di.group.toStdString()];
        if (di.numeric)
            g[di.key.toStdString()] = s.get(d).toInt();
        else
            g[di.key.toStdString()] = s.get(d).toStdString();
    }
    return j;
}

QString SceneConfig::toJsonText() const { return QString::fromStdString(buildJson(*this).dump(2)); }

bool SceneConfig::writeFile(const QString& path, QString* err) const {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = "cannot write " + path + ": " + f.errorString();
        return false;
    }
    f.write(toJsonText().toUtf8());
    f.write("\n");
    if (!f.commit()) {
        if (err) *err = "cannot commit " + path;
        return false;
    }
    return true;
}

bool SceneConfig::fromJsonText(const QString& text, SceneConfig* out, QString* err) {
    try {
        ojson j = ojson::parse(text.toStdString());
        SceneConfig s;
        const auto& m = j.at("mission");
        s.missionId = m.at("id").get<int>();
        s.mode = QString::fromStdString(m.at("mode").get<std::string>());
        s.level = m.at("difficulty_level").get<int>();
        for (int d = 0; d < DimCount; ++d) {
            const DimInfo& di = dim(d);
            const auto& val = j.at(di.group.toStdString()).at(di.key.toStdString());
            QString t = val.is_number() ? QString::number(val.get<int>()) : QString::fromStdString(val.get<std::string>());
            if (valueIndex(d, t) < 0) {
                if (err) *err = "unknown value '" + t + "' for " + di.group + "." + di.key;
                return false;
            }
            s.set(d, t);
        }
        if (out) *out = s;
        return true;
    } catch (const std::exception& e) {
        if (err) *err = QString("scene parse error: ") + e.what();
        return false;
    }
}

bool SceneConfig::readFile(const QString& path, SceneConfig* out, QString* err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = "cannot open " + path;
        return false;
    }
    return fromJsonText(QString::fromUtf8(f.readAll()), out, err);
}

double SceneConfig::difficultyScore() const {
    double sum = 0, wsum = 0;
    for (int d = 0; d < DimCount; ++d) {
        int i = valueIndex(d, get(d));
        double h = i >= 0 ? dim(d).hardness[i] : 0.0;
        sum += h * dim(d).weight;
        wsum += dim(d).weight;
    }
    return 100.0 * sum / wsum;
}

QString SceneConfig::validate() const {
    for (int d = 0; d < DimCount; ++d)
        if (valueIndex(d, get(d)) < 0) return "invalid value for " + dim(d).key;
    if (level < 1 || level > 5) return "difficulty_level out of range";
    if (!modes().contains(mode)) return "unknown mode";
    const QString t = get(DThreatType);
    if (t == "single") {
        if (get(DSwarmCount) != "1" || get(DDroneCount) != "1") return "single threat needs 1 swarm of 1 drone";
        if (get(DPosition) != "same") return "single threat needs position 'same'";
    } else if (t == "swarm") {
        if (get(DSwarmCount) != "1") return "swarm needs swarm_count 1";
        if (get(DDroneCount) == "1") return "swarm needs 5 or 10 drones";
        if (get(DPosition) != "same") return "one swarm needs position 'same'";
    } else {
        if (get(DSwarmCount) == "1") return "multiple_swarm needs 2 or 3 swarms";
        if (get(DDroneCount) == "1") return "multiple_swarm needs 5 or 10 drones per swarm";
    }
    if (get(DBinoculars) == "unavailable" && get(DThermal) == "unavailable" && get(DSignal) == "unavailable")
        return "no usable detection route";
    return QString();
}

QString SceneConfig::headline() const {
    return (get(DTime) + " / " + get(DWeather) + " / " + get(DTerrain)).toUpper();
}

QString SceneConfig::threatSummary() const {
    const QString t = get(DThreatType);
    QString core;
    if (t == "single") core = "Single drone";
    else if (t == "swarm") core = QString("1 swarm of %1").arg(get(DDroneCount));
    else core = QString("%1 swarms of %2").arg(get(DSwarmCount), get(DDroneCount));
    QString s = core + ", " + get(DApproach) + ", " + get(DSpeed) + " speed";
    if (t == "multiple_swarm") s += get(DPosition) == "same" ? ", shared sector" : ", separate sectors";
    return s;
}

QString SceneConfig::sensorSummary() const {
    auto st = [](const QString& s) { return s == "available" || s == "high" ? "OK" : (s == "degraded" || s == "medium" ? "DEGRADED" : (s == "unavailable" ? "OFFLINE" : "LOW")); };
    return QString("BINOC %1  ·  THERMAL %2  ·  RF %3  ·  ACOUSTIC %4")
        .arg(st(get(DBinoculars)), st(get(DThermal)), st(get(DSignal)), st(get(DAudio)));
}

QStringList SceneConfig::briefingLines() const {
    QStringList l;
    l << "Conditions: " + valueLabel(DTime, get(DTime)) + ", " + get(DWeather) + ", " + get(DTerrain) + " terrain, visibility " +
             get(DVisibility).replace('_', ' ');
    l << "Threat: " + threatSummary();
    l << "Sensors: " + sensorSummary();
    l << "Intel: " + get(DClue) + " clue  ·  decoys: " + get(DDecoys);
    l << QString("Windows: detect %1 s / classify %2 s / respond %3 s")
             .arg(detectionWindowSeconds(get(DDetWin)))
             .arg(classificationWindowSeconds(get(DClsWin)))
             .arg(responseWindowSeconds(get(DRspWin)));
    return l;
}

bool SceneConfig::operator==(const SceneConfig& o) const { return v == o.v && level == o.level && mode == o.mode; }

QPair<double, double> levelBand(int level) {
    switch (level) {
        case 1: return {0.0, 22.0};
        case 2: return {22.0, 36.0};
        case 3: return {36.0, 50.0};
        case 4: return {50.0, 64.0};
        default: return {64.0, 100.0};
    }
}

int levelFromScore(double s) {
    if (s < 22.0) return 1;
    if (s < 36.0) return 2;
    if (s < 50.0) return 3;
    if (s < 64.0) return 4;
    return 5;
}

}  // namespace cmd
