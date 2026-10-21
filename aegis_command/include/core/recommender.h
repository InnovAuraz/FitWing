#pragma once
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QVector>
#include <memory>
#include <optional>

#include "core/player_store.h"
#include "core/scene_config.h"

namespace cmd {

struct HistoryItem {
    SceneConfig scene;
    double overall = 0;
};

struct RecommendRequest {
    Stats stats;
    QVector<HistoryItem> history;   // this session, oldest first
    QString focusCategory;          // "" = automatic, otherwise one of categories()
    int candidateCount = 400;
    quint32 seed = 0;
};

struct Recommendation {
    SceneConfig scene;
    QString engine;            // "PyTorch 2.x.x" or "Built-in engine"
    double predicted = 0;      // expected score
    double weakness = 0;       // 0..1
    double fit = 0;
    double novelty = 0;
    double total = 0;
    QStringList reasons;       // human readable explanation
    bool ok = false;
};

struct RankedCandidate {
    int index;
    double total, weakness, fit, novelty, predicted;
};

class Recommender : public QObject {
    Q_OBJECT
public:
    explicit Recommender(const QString& workDir, QObject* parent = nullptr);

    void setPythonEnabled(bool on) { pythonEnabled_ = on; }
    QString engineStatus() const;                 // text for the UI
    void probePython();                           // async; safe to call more than once

    // Pure C++ implementation (also used when Python / torch is missing)
    Recommendation recommendBuiltin(const RecommendRequest& req) const;
    // Python first, falls back to the built-in engine. Blocking (used by tests).
    Recommendation recommendBlocking(const RecommendRequest& req);
    // Non-blocking, result is delivered through finished().
    void recommendAsync(const RecommendRequest& req);

signals:
    void finished(const Recommendation& rec);
    void probeFinished();

private:
    struct Prepared;
    std::shared_ptr<Prepared> prepare(const RecommendRequest& req) const;
    QVector<RankedCandidate> rankBuiltin(const Prepared& p) const;
    Recommendation build(const Prepared& p, const QVector<RankedCandidate>& top, const QString& engine) const;
    bool parsePythonAnswer(const Prepared& p, const QByteArray& data, Recommendation* out) const;
    QString ensureScript();
    void startNextProbe();
    void runPython(std::shared_ptr<Prepared> p);

    QString workDir_;
    bool pythonEnabled_ = true;
    bool probed_ = false;
    bool probing_ = false;
    QString pyProgram_;
    QStringList pyPrefix_;
    QString torchInfo_;                 // "torch 2.x" when usable
    QStringList probeQueue_;
    QProcess* probeProc_ = nullptr;
    QProcess* runProc_ = nullptr;
    std::shared_ptr<Prepared> pending_;
    bool waitingForProbe_ = false;
};

}  // namespace cmd
