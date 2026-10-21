#include "core/player_store.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>
#include "json.hpp"

namespace cmd {

using ojson = nlohmann::ordered_json;

Stats::Stats() : v_(featureCount(), 50.0) {}

double Stats::get(int d, const QString& value) const {
    int i = valueIndex(d, value);
    return i < 0 ? 50.0 : get(d, i);
}

void Stats::set(int d, int valueIdx, double x) { v_[featureOffset(d) + valueIdx] = std::min(100.0, std::max(0.0, x)); }

double Stats::categoryMean(const QString& cat) const {
    double s = 0;
    int n = 0;
    for (int d : dimsOfCategory(cat))
        for (int i = 0; i < dim(d).values.size(); ++i) {
            s += get(d, i);
            ++n;
        }
    return n ? s / n : 50.0;
}

double Stats::overallMean() const {
    double s = 0;
    for (double x : v_) s += x;
    return v_.isEmpty() ? 50.0 : s / v_.size();
}

QString Stats::toJsonText() const {
    ojson j;
    for (int d = 0; d < DimCount; ++d) {
        const DimInfo& di = dim(d);
        ojson o = ojson::object();
        for (int i = 0; i < di.values.size(); ++i) o[di.values[i].toStdString()] = std::round(get(d, i) * 1000.0) / 1000.0;
        j[di.group.toStdString()][di.key.toStdString()] = o;
    }
    return QString::fromStdString(j.dump(2));
}

bool Stats::fromJsonText(const QString& text, Stats* out, QString* err) {
    try {
        ojson j = ojson::parse(text.toStdString());
        Stats s;
        for (int d = 0; d < DimCount; ++d) {
            const DimInfo& di = dim(d);
            if (!j.contains(di.group.toStdString())) continue;
            const auto& g = j[di.group.toStdString()];
            if (!g.contains(di.key.toStdString())) continue;
            const auto& o = g[di.key.toStdString()];
            for (int i = 0; i < di.values.size(); ++i) {
                const std::string k = di.values[i].toStdString();
                if (o.contains(k) && o[k].is_number()) s.set(d, i, o[k].get<double>());
            }
        }
        if (out) *out = s;
        return true;
    } catch (const std::exception& e) {
        if (err) *err = QString("stats parse error: ") + e.what();
        return false;
    }
}

static double stageScore(double ratio) {
    if (ratio <= 0.25) return 1.0;
    return std::max(0.0, 1.0 - (ratio - 0.25) / 0.75);
}

QVector<StatDelta> applyResult(Stats& stats, const SceneConfig& scene, const Performance& perf, double lr) {
    QVector<StatDelta> out;
    const double diff = 0.5 * (scene.level - 1) / 4.0 + 0.5 * std::min(1.0, std::max(0.0, scene.difficultyScore() / 100.0));
    for (int d = 0; d < DimCount; ++d) {
        int i = valueIndex(d, scene.get(d));
        if (i < 0) continue;
        double score = perf.overall;
        if (d == DDetWin)
            score = 0.5 * perf.overall + 50.0 * stageScore(perf.detection / detectionWindowSeconds(scene.get(d)));
        else if (d == DClsWin)
            score = 0.5 * perf.overall + 50.0 * stageScore(perf.classification / classificationWindowSeconds(scene.get(d)));
        else if (d == DRspWin)
            score = 0.5 * perf.overall + 50.0 * stageScore(perf.response / responseWindowSeconds(scene.get(d)));
        score = std::min(100.0, std::max(0.0, score));
        const double before = stats.get(d, i);
        const double e = score - before;
        const double k = e < 0 ? lr * (1.5 - diff) : lr * (0.7 + 0.6 * diff);
        stats.set(d, i, before + k * e);
        out.push_back({d, i, before, stats.get(d, i)});
    }
    return out;
}

// ---------------------------------------------------------------- PlayerStore

PlayerStore::PlayerStore(const QString& root) : root_(root) { QDir().mkpath(root_ + "/players"); QDir().mkpath(runtimeDir()); }

QString PlayerStore::defaultRoot() {
    QString p = QCoreApplication::applicationDirPath() + "/AppData";
    if (QDir().mkpath(p + "/players")) {
        QFile probe(p + "/.write_test");
        if (probe.open(QIODevice::WriteOnly)) {
            probe.close();
            probe.remove();
            return p;
        }
    }
    QString alt = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(alt);
    return alt;
}

QString PlayerStore::sanitize(const QString& name) {
    QString s;
    for (QChar c : name.trimmed())
        if (c.isLetterOrNumber() || c == ' ' || c == '_' || c == '-') s.push_back(c);
    return s.simplified().left(24);
}

QString PlayerStore::statsPath(const QString& name) const { return root_ + "/players/" + name + "/player_stats.json"; }
QString PlayerStore::runtimeDir() const { return root_ + "/runtime"; }

QStringList PlayerStore::list() const {
    QDir d(root_ + "/players");
    QStringList r;
    for (const QString& n : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
        if (QFileInfo::exists(statsPath(n))) r << n;
    return r;
}

bool PlayerStore::exists(const QString& name) const { return QFileInfo::exists(statsPath(name)); }

bool PlayerStore::save(const QString& name, const Stats& s, QString* err) const {
    QDir().mkpath(root_ + "/players/" + name);
    QSaveFile f(statsPath(name));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = "cannot write " + statsPath(name);
        return false;
    }
    f.write(s.toJsonText().toUtf8());
    f.write("\n");
    return f.commit();
}

bool PlayerStore::create(const QString& name, QString* err) {
    const QString n = sanitize(name);
    if (n.isEmpty()) {
        if (err) *err = "Name must contain letters or digits.";
        return false;
    }
    if (exists(n)) {
        if (err) *err = "An operator with that name already exists.";
        return false;
    }
    return save(n, Stats(), err);
}

bool PlayerStore::remove(const QString& name, QString* err) {
    QDir d(root_ + "/players/" + name);
    if (!d.exists()) return true;
    if (!d.removeRecursively()) {
        if (err) *err = "Could not delete " + d.path();
        return false;
    }
    return true;
}

bool PlayerStore::reset(const QString& name, QString* err) { return save(name, Stats(), err); }

Stats PlayerStore::load(const QString& name) const {
    QFile f(statsPath(name));
    Stats s;
    if (f.open(QIODevice::ReadOnly)) Stats::fromJsonText(QString::fromUtf8(f.readAll()), &s);
    return s;
}

}  // namespace cmd
