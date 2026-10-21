#pragma once
#include <QDateTime>
#include <QObject>
#include <QVector>

#include "core/generator.h"
#include "core/performance.h"
#include "core/player_store.h"
#include "core/recommender.h"
#include "core/scene_config.h"

namespace cmd {

struct MissionRecord {
    SceneConfig scene;
    Performance perf;
    QDateTime when;
    QVector<StatDelta> deltas;
};

// Everything that lives only while the trainer is open: selected operator, history, campaign progress.
class Session : public QObject {
    Q_OBJECT
public:
    explicit Session(const QString& dataRoot, QObject* parent = nullptr);

    PlayerStore& store() { return store_; }
    const QString& player() const { return player_; }
    bool selectPlayer(const QString& name);
    void clearPlayer();
    const Stats& stats() const { return stats_; }
    bool resetStats();

    // campaign
    const QVector<SceneConfig>& campaign() const { return campaign_; }
    bool unlocked(int idx) const { return idx == 0 || done_[idx - 1]; }
    bool done(int idx) const { return done_[idx]; }
    double best(int idx) const { return best_[idx]; }
    int attempts(int idx) const { return attempts_[idx]; }
    void newCampaign();
    bool campaignFinished() const;

    // a scene that is about to be flown gets its unique mission id here
    SceneConfig stamp(const SceneConfig& s) const;

    // stores a finished mission: updates stats (and the file) and the session history
    const MissionRecord& record(const SceneConfig& scene, const Performance& perf);
    const QVector<MissionRecord>& history() const { return history_; }
    QVector<HistoryItem> historyItems() const;
    const MissionRecord* lastRecord() const { return history_.isEmpty() ? nullptr : &history_.last(); }

signals:
    void playerChanged();
    void statsChanged();
    void historyChanged();
    void campaignChanged();

private:
    PlayerStore store_;
    QString player_;
    Stats stats_;
    QVector<SceneConfig> campaign_;
    QVector<bool> done_;
    QVector<double> best_;
    QVector<int> attempts_;
    QVector<MissionRecord> history_;
};

}  // namespace cmd
