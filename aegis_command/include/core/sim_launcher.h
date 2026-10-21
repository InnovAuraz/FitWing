#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>

#include "core/performance.h"
#include "core/player_store.h"
#include "core/scene_config.h"

namespace cmd {

int newMissionId();  // seconds since epoch, fits an int

class SimLauncher : public QObject {
    Q_OBJECT
public:
    explicit SimLauncher(const QString& runtimeDir, QObject* parent = nullptr);

    QString simPath() const;          // "" when the simulator executable cannot be found
    bool simAvailable() const { return !simPath().isEmpty(); }
    void setCustomPath(const QString& p) { custom_ = p; }
    void setMock(bool on) { mock_ = on; }
    bool mock() const { return mock_; }
    bool running() const { return proc_ || mockTimer_->isActive(); }
    QString scenePath() const { return runtime_ + "/scene_definition.json"; }
    QString performancePath() const { return runtime_ + "/player_performance.json"; }

    // `stats` is only used by the training dummy to fake a plausible result.
    bool launch(const SceneConfig& scene, const Stats& stats, QString* err);
    void terminate();

    static Performance fakeResult(const SceneConfig& scene, const Stats& stats);

signals:
    void started();
    void finished(bool ok, const cmd::Performance& perf, const QString& message);

private:
    void handleFinished(int code, QProcess::ExitStatus st);
    QString runtime_;
    QString custom_;
    bool mock_ = false;
    QProcess* proc_ = nullptr;
    QTimer* mockTimer_ = nullptr;
    SceneConfig current_;
    Stats stats_;
};

}  // namespace cmd
