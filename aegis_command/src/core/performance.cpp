#include "core/performance.h"

#include <QFile>
#include "json.hpp"

namespace cmd {

using ojson = nlohmann::ordered_json;

QString Performance::toJsonText() const {
    ojson j;
    j["mission"] = {{"id", missionId}, {"mode", mode.toStdString()}, {"difficulty_level", level}};
    ojson p;
    p["response"] = {{"detection_time_seconds", detection},
                     {"classification_time_seconds", classification},
                     {"response_time_seconds", response}};
    p["actions"] = {{"shots fired", shots}, {"missiles fired", missiles}};
    p["decision_score"] = decision;
    p["time_score"] = time;
    p["accuracy"] = accuracy;
    p["overall_score"] = overall;
    p["mission_completed"] = completed;
    j["performance"] = p;
    return QString::fromStdString(j.dump(2));
}

bool Performance::fromJsonText(const QString& text, Performance* out, QString* err) {
    try {
        ojson j = ojson::parse(text.toStdString());
        Performance r;
        const auto& m = j.at("mission");
        r.missionId = m.at("id").get<int>();
        r.mode = QString::fromStdString(m.at("mode").get<std::string>());
        r.level = m.at("difficulty_level").get<int>();
        const auto& p = j.at("performance");
        const auto& resp = p.at("response");
        r.detection = resp.at("detection_time_seconds").get<double>();
        r.classification = resp.at("classification_time_seconds").get<double>();
        r.response = resp.at("response_time_seconds").get<double>();
        const auto& a = p.at("actions");
        r.shots = a.at("shots fired").get<int>();
        r.missiles = a.at("missiles fired").get<int>();
        r.decision = p.at("decision_score").get<double>();
        r.time = p.at("time_score").get<double>();
        r.accuracy = p.at("accuracy").get<double>();
        r.overall = p.at("overall_score").get<double>();
        r.completed = p.at("mission_completed").get<bool>();
        if (out) *out = r;
        return true;
    } catch (const std::exception& e) {
        if (err) *err = QString("performance parse error: ") + e.what();
        return false;
    }
}

bool Performance::readFile(const QString& path, Performance* out, QString* err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = "no result file at " + path;
        return false;
    }
    return fromJsonText(QString::fromUtf8(f.readAll()), out, err);
}

QString rankFor(double s, bool completed) {
    if (!completed && s < 50) return "F";
    if (s >= 92) return "S";
    if (s >= 80) return "A";
    if (s >= 65) return "B";
    if (s >= 50) return "C";
    if (s >= 35) return "D";
    return "F";
}

QString rankTitle(const QString& r) {
    if (r == "S") return "APEX WARDEN";
    if (r == "A") return "SENTINEL";
    if (r == "B") return "OPERATOR";
    if (r == "C") return "TRAINEE";
    if (r == "D") return "PROBATION";
    return "DEBRIEF REQUIRED";
}

}  // namespace cmd
