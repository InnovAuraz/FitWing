#include "core/session.h"

#include "core/sim_launcher.h"

namespace cmd {

Session::Session(const QString& dataRoot, QObject* parent) : QObject(parent), store_(dataRoot) { newCampaign(); }

bool Session::selectPlayer(const QString& name) {
    if (!store_.exists(name)) return false;
    player_ = name;
    stats_ = store_.load(name);
    history_.clear();
    newCampaign();
    emit playerChanged();
    emit statsChanged();
    emit historyChanged();
    return true;
}

void Session::clearPlayer() {
    player_.clear();
    stats_ = Stats();
    history_.clear();
    newCampaign();
    emit playerChanged();
    emit historyChanged();
}

bool Session::resetStats() {
    if (player_.isEmpty()) return false;
    if (!store_.reset(player_)) return false;
    stats_ = Stats();
    emit statsChanged();
    return true;
}

void Session::newCampaign() {
    Generator g;
    campaign_ = g.campaign();
    done_ = QVector<bool>(campaign_.size(), false);
    best_ = QVector<double>(campaign_.size(), -1.0);
    attempts_ = QVector<int>(campaign_.size(), 0);
    emit campaignChanged();
}

bool Session::campaignFinished() const {
    for (bool d : done_)
        if (!d) return false;
    return !done_.isEmpty();
}

SceneConfig Session::stamp(const SceneConfig& s) const {
    SceneConfig c = s;
    c.missionId = newMissionId();
    return c;
}

const MissionRecord& Session::record(const SceneConfig& scene, const Performance& perf) {
    MissionRecord r;
    r.scene = scene;
    r.perf = perf;
    r.when = QDateTime::currentDateTime();
    if (!player_.isEmpty()) {
        r.deltas = applyResult(stats_, scene, perf);
        store_.save(player_, stats_);
    }
    history_.push_back(r);
    if (scene.mode == "campaign" && scene.level >= 1 && scene.level <= campaign_.size()) {
        const int i = scene.level - 1;
        attempts_[i]++;
        best_[i] = std::max(best_[i], perf.overall);
        if (perf.completed) done_[i] = true;
        emit campaignChanged();
    }
    emit statsChanged();
    emit historyChanged();
    return history_.last();
}

QVector<HistoryItem> Session::historyItems() const {
    QVector<HistoryItem> v;
    for (const auto& r : history_) v.push_back({r.scene, r.perf.overall});
    return v;
}

}  // namespace cmd
