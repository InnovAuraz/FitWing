#include "core/sim_launcher.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <algorithm>

namespace cmd {

int newMissionId() {
    qint64 s = QDateTime::currentSecsSinceEpoch();
    if (s > 2147483000LL) s = s % 2147483000LL;
    return static_cast<int>(std::max<qint64>(1, s));
}

SimLauncher::SimLauncher(const QString& runtimeDir, QObject* parent) : QObject(parent), runtime_(runtimeDir) {
    QDir().mkpath(runtime_);
    mockTimer_ = new QTimer(this);
    mockTimer_->setSingleShot(true);
    connect(mockTimer_, &QTimer::timeout, this, [this] {
        Performance p = fakeResult(current_, stats_);
        p.mock = true;
        QFile f(performancePath());
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(p.toJsonText().toUtf8());
            f.close();
        }
        emit finished(true, p, "Training dummy result (no simulator was started).");
    });
}

QString SimLauncher::simPath() const {
#ifdef Q_OS_WIN
    const QString exe = "FitWingSim.exe";
#else
    const QString exe = "FitWingSim";
#endif
    QStringList cands;
    if (!custom_.isEmpty()) cands << custom_;
    const QByteArray env = qgetenv("FITWING_SIM_PATH");
    if (!env.isEmpty()) cands << QString::fromLocal8Bit(env);
    cands << QCoreApplication::applicationDirPath() + "/" + exe;
    for (const QString& c : cands)
        if (QFileInfo(c).isFile()) return QFileInfo(c).absoluteFilePath();
    return QString();
}

bool SimLauncher::launch(const SceneConfig& scene, const Stats& stats, QString* err) {
    if (running()) {
        if (err) *err = "A mission is already running.";
        return false;
    }
    current_ = scene;
    stats_ = stats;
    QString e;
    if (!scene.isValid()) {
        if (err) *err = "Scene is not valid: " + scene.validate();
        return false;
    }
    if (!scene.writeFile(scenePath(), &e)) {
        if (err) *err = e;
        return false;
    }
    QFile::remove(performancePath());  // never read a stale result

    if (mock_) {
        mockTimer_->start(2200);
        emit started();
        return true;
    }
    const QString exe = simPath();
    if (exe.isEmpty()) {
        if (err) *err = "FitWingSim executable not found next to the trainer. Enable the training dummy or copy FitWingSim into the same folder.";
        return false;
    }
    proc_ = new QProcess(this);
    proc_->setWorkingDirectory(QFileInfo(exe).absolutePath());
    proc_->setProcessChannelMode(QProcess::ForwardedChannels);
    connect(proc_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &SimLauncher::handleFinished);
    connect(proc_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError pe) {
        if (pe == QProcess::FailedToStart && proc_) {
            proc_->deleteLater();
            proc_ = nullptr;
            emit finished(false, Performance(), "The simulator could not be started.");
        }
    });
    proc_->start(exe, {"--scene", scenePath(), "--out", performancePath()});
    emit started();
    return true;
}

void SimLauncher::terminate() {
    if (mockTimer_->isActive()) {
        mockTimer_->stop();
        emit finished(false, Performance(), "Mission cancelled.");
        return;
    }
    if (proc_) {
        proc_->kill();
    }
}

void SimLauncher::handleFinished(int code, QProcess::ExitStatus st) {
    auto* p = proc_;
    proc_ = nullptr;
    if (p) p->deleteLater();
    Performance perf;
    QString err;
    if (!Performance::readFile(performancePath(), &perf, &err)) {
        emit finished(false, perf,
                      st == QProcess::CrashExit ? "The simulator terminated unexpectedly - no result was recorded."
                                                : QString("The simulator closed without writing a result (exit code %1).").arg(code));
        return;
    }
    if (perf.missionId != current_.missionId) {
        emit finished(false, perf, "Result belongs to a different mission (id mismatch) and was ignored.");
        return;
    }
    emit finished(true, perf, QString());
}

Performance SimLauncher::fakeResult(const SceneConfig& s, const Stats& stats) {
    auto* rng = QRandomGenerator::global();
    auto noise = [&](double a) { return (rng->generateDouble() * 2.0 - 1.0) * a; };
    double skill = 0;
    for (int d = 0; d < DimCount; ++d) skill += stats.get(d, s.get(d));
    skill = skill / static_cast<double>(DimCount);
    const double eff = std::min(100.0, std::max(0.0, skill - 0.35 * (s.difficultyScore() - 50.0) + noise(12.0)));
    Performance p;
    p.missionId = s.missionId;
    p.mode = s.mode;
    p.level = s.level;
    auto ratio = [&](double bias) { return std::min(1.0, std::max(0.12, 1.0 - eff / 130.0 + bias + noise(0.1))); };
    p.detection = ratio(0.0) * detectionWindowSeconds(s.get(DDetWin));
    p.classification = ratio(0.02) * classificationWindowSeconds(s.get(DClsWin));
    p.response = ratio(0.05) * responseWindowSeconds(s.get(DRspWin));
    const bool single = s.get(DThreatType) == "single";
    p.shots = single ? 2 + rng->bounded(7) : rng->bounded(5);
    p.missiles = single ? rng->bounded(2) : 1 + rng->bounded(3);
    auto clamp = [](double x) { return std::min(100.0, std::max(0.0, x)); };
    p.decision = clamp(eff + noise(10));
    p.time = clamp(eff + noise(10));
    p.accuracy = clamp(eff + noise(14));
    p.overall = clamp(0.4 * p.decision + 0.35 * p.time + 0.25 * p.accuracy);
    p.completed = p.overall >= 45.0;
    return p;
}

}  // namespace cmd
