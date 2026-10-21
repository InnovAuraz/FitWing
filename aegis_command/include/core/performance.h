#pragma once
#include <QString>

namespace cmd {

// player_performance.json as written by the simulator
struct Performance {
    int missionId = 0;
    QString mode;
    int level = 1;
    double detection = 0, classification = 0, response = 0;  // seconds
    int shots = 0, missiles = 0;
    double decision = 0, time = 0, accuracy = 0, overall = 0;  // 0..100
    bool completed = false;
    bool mock = false;  // produced by the built-in training dummy (not written to the JSON)

    QString toJsonText() const;
    static bool fromJsonText(const QString& text, Performance* out, QString* err = nullptr);
    static bool readFile(const QString& path, Performance* out, QString* err = nullptr);
};

QString rankFor(double overall, bool completed);
QString rankTitle(const QString& rank);

}  // namespace cmd
