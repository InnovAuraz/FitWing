#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

#include "core/performance.h"
#include "core/scene_config.h"

namespace cmd {

// player_stats.json: one mastery value (0..100, start 50) for every vocabulary value.
class Stats {
public:
    Stats();
    double get(int d, int valueIdx) const { return v_[featureOffset(d) + valueIdx]; }
    double get(int d, const QString& value) const;
    void set(int d, int valueIdx, double x);
    const QVector<double>& flat() const { return v_; }
    double categoryMean(const QString& category) const;
    double overallMean() const;

    QString toJsonText() const;
    static bool fromJsonText(const QString& text, Stats* out, QString* err = nullptr);

private:
    QVector<double> v_;
};

struct StatDelta {
    int dim;
    int valueIdx;
    double before, after;
};

// Difficulty aware update of the stats from one finished mission.
QVector<StatDelta> applyResult(Stats& stats, const SceneConfig& scene, const Performance& perf, double learningRate = 0.25);

// Players live in <data root>/players/<name>/player_stats.json
class PlayerStore {
public:
    explicit PlayerStore(const QString& root);
    static QString defaultRoot();            // <exe dir>/AppData, falls back to the user's app-data folder
    static QString sanitize(const QString& name);

    QString root() const { return root_; }
    QStringList list() const;
    bool exists(const QString& name) const;
    bool create(const QString& name, QString* err = nullptr);
    bool remove(const QString& name, QString* err = nullptr);
    bool reset(const QString& name, QString* err = nullptr);
    Stats load(const QString& name) const;
    bool save(const QString& name, const Stats& s, QString* err = nullptr) const;
    QString statsPath(const QString& name) const;
    QString runtimeDir() const;              // scene_definition.json / player_performance.json live here

private:
    QString root_;
};

}  // namespace cmd
