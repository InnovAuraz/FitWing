#include "core/recommender.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <algorithm>
#include <cmath>

#include "core/generator.h"

namespace cmd {

namespace {
constexpr double kTarget = 62.0;
constexpr double kLambda = 2.0;
constexpr double kFocusWeight = 3.0;
constexpr int kMaxHistory = 40;

QVector<int> activeFeatures(const SceneConfig& s) {
    QVector<int> a;
    for (int d = 0; d < DimCount; ++d) {
        int i = valueIndex(d, s.get(d));
        if (i >= 0) a.push_back(featureOffset(d) + i);
    }
    return a;
}

// Solves A x = b (A is n x n, symmetric positive definite). Gaussian elimination with partial pivoting.
QVector<double> solve(QVector<QVector<double>> A, QVector<double> b) {
    const int n = b.size();
    for (int c = 0; c < n; ++c) {
        int piv = c;
        for (int r = c + 1; r < n; ++r)
            if (std::fabs(A[r][c]) > std::fabs(A[piv][c])) piv = r;
        std::swap(A[c], A[piv]);
        std::swap(b[c], b[piv]);
        const double d = A[c][c];
        for (int r = c + 1; r < n; ++r) {
            const double f = A[r][c] / d;
            if (f == 0.0) continue;
            for (int k = c; k < n; ++k) A[r][k] -= f * A[c][k];
            b[r] -= f * b[c];
        }
    }
    QVector<double> x(n, 0.0);
    for (int r = n - 1; r >= 0; --r) {
        double s = b[r];
        for (int k = r + 1; k < n; ++k) s -= A[r][k] * x[k];
        x[r] = s / A[r][r];
    }
    return x;
}
}  // namespace

struct Recommender::Prepared {
    RecommendRequest req;
    QVector<SceneConfig> cands;
    QVector<QVector<int>> active;
    QVector<double> featureWeights;
    QVector<QVector<int>> histActive;
    QVector<double> histScore;
    QVector<double> histDiff;
    QVector<double> candDiff;
    QVector<int> lastActive;
    bool hasLast = false;
};

Recommender::Recommender(const QString& workDir, QObject* parent) : QObject(parent), workDir_(workDir) { QDir().mkpath(workDir_); }

std::shared_ptr<Recommender::Prepared> Recommender::prepare(const RecommendRequest& req) const {
    auto p = std::make_shared<Prepared>();
    p->req = req;
    Generator gen(req.seed);
    p->cands = gen.candidates(std::max(20, req.candidateCount));
    for (const auto& c : p->cands) {
        p->active.push_back(activeFeatures(c));
        p->candDiff.push_back(c.difficultyScore());
    }
    p->featureWeights = QVector<double>(featureCount(), 1.0);
    if (!req.focusCategory.isEmpty())
        for (int d : dimsOfCategory(req.focusCategory))
            for (int i = 0; i < dim(d).values.size(); ++i) p->featureWeights[featureOffset(d) + i] = kFocusWeight;
    const int from = std::max(0, static_cast<int>(req.history.size()) - kMaxHistory);
    for (int i = from; i < req.history.size(); ++i) {
        p->histActive.push_back(activeFeatures(req.history[i].scene));
        p->histScore.push_back(req.history[i].overall);
        p->histDiff.push_back(req.history[i].scene.difficultyScore());
    }
    if (!req.history.isEmpty()) {
        p->lastActive = activeFeatures(req.history.last().scene);
        p->hasLast = true;
    }
    return p;
}

QVector<RankedCandidate> Recommender::rankBuiltin(const Prepared& p) const {
    const int F = featureCount();
    const int N = p.cands.size();
    const QVector<double>& s = p.req.stats.flat();
    const QVector<double>& fw = p.featureWeights;

    // history: ridge on residual vs prior
    QVector<double> w(F, 0.0);
    QVector<double> counts(F, 0.0);
    const int H = p.histActive.size();
    if (H > 0) {
        QVector<QVector<double>> A(F, QVector<double>(F, 0.0));
        QVector<double> b(F, 0.0);
        for (int h = 0; h < H; ++h) {
            double prior = 0;
            for (int f : p.histActive[h]) prior += s[f];
            prior = prior / std::max<int>(1, p.histActive[h].size()) - 0.5 * (p.histDiff[h] - 50.0);
            const double r = p.histScore[h] - prior;
            for (int f : p.histActive[h]) {
                counts[f] += 1.0;
                b[f] += r;
                for (int g : p.histActive[h]) A[f][g] += 1.0;
            }
        }
        for (int f = 0; f < F; ++f) A[f][f] += kLambda;
        w = solve(A, b);
    }

    QVector<RankedCandidate> all(N);
    double wmin = 1e18, wmax = -1e18;
    for (int i = 0; i < N; ++i) {
        const auto& act = p.active[i];
        double num = 0, den = 0, prior = 0, wsum = 0, nov = 0;
        for (int f : act) {
            num += fw[f] * (100.0 - s[f]) / 100.0;
            den += fw[f];
            prior += s[f];
            wsum += w[f];
            nov += 1.0 / (1.0 + counts[f]);
        }
        const double cnt = std::max<int>(1, act.size());
        RankedCandidate r;
        r.index = i;
        r.weakness = num / std::max(den, 1e-9);
        prior = prior / cnt - 0.5 * (p.candDiff[i] - 50.0);
        r.predicted = std::min(100.0, std::max(0.0, prior + (H > 0 ? wsum : 0.0)));
        r.fit = std::max(0.0, 1.0 - std::fabs(r.predicted - kTarget) / 45.0);
        r.novelty = nov / cnt;
        wmin = std::min(wmin, r.weakness);
        wmax = std::max(wmax, r.weakness);
        all[i] = r;
    }
    for (auto& r : all) {
        const double wn = (r.weakness - wmin) / (wmax - wmin + 1e-9);
        r.total = 0.40 * wn + 0.45 * r.fit + 0.15 * r.novelty;
        if (p.hasLast && p.active[r.index] == p.lastActive) r.total -= 1.0;
    }
    std::stable_sort(all.begin(), all.end(), [](const RankedCandidate& a, const RankedCandidate& b) { return a.total > b.total; });
    all.resize(std::min(5, N));
    return all;
}

Recommendation Recommender::build(const Prepared& p, const QVector<RankedCandidate>& top, const QString& engine) const {
    Recommendation r;
    if (top.isEmpty()) return r;
    const RankedCandidate& best = top.first();
    r.scene = p.cands[best.index];
    r.scene.mode = "ai_training";
    r.scene.level = levelFromScore(r.scene.difficultyScore());
    r.engine = engine;
    r.predicted = best.predicted;
    r.weakness = best.weakness;
    r.fit = best.fit;
    r.novelty = best.novelty;
    r.total = best.total;
    r.ok = true;

    // explanation: weakest activated options
    struct W { int d; double stat; };
    QVector<W> ws;
    for (int d = 0; d < DimCount; ++d) {
        int i = valueIndex(d, r.scene.get(d));
        ws.push_back({d, p.req.stats.get(d, i)});
    }
    std::sort(ws.begin(), ws.end(), [](const W& a, const W& b) { return a.stat < b.stat; });
    QStringList weak;
    for (int k = 0; k < ws.size() && weak.size() < 4; ++k)
        if (ws[k].stat < 55.0) weak << QString("%1 = %2 (mastery %3)").arg(dim(ws[k].d).label, valueLabel(ws[k].d, r.scene.get(ws[k].d)).toLower()).arg(qRound(ws[k].stat));
    if (!weak.isEmpty()) r.reasons << "Exercises your weakest areas: " + weak.join("; ") + ".";
    else r.reasons << "No clear weak spot left in this combination - this mission keeps the whole skill set sharp.";
    if (!p.req.focusCategory.isEmpty()) r.reasons << "Focus category '" + p.req.focusCategory + "' was weighted 3x.";
    r.reasons << QString("Predicted score %1 - target zone is %2 (hard enough to teach, not so hard it is hopeless).")
                     .arg(qRound(best.predicted)).arg(qRound(kTarget));
    if (!p.req.history.isEmpty())
        r.reasons << QString("Adapted to your last %1 mission(s) in this session.").arg(std::min<int>(kMaxHistory, p.req.history.size()));
    if (best.novelty > 0.7) r.reasons << "Contains conditions you have rarely trained.";
    return r;
}

Recommendation Recommender::recommendBuiltin(const RecommendRequest& req) const {
    auto p = prepare(req);
    return build(*p, rankBuiltin(*p), "Built-in engine");
}

// ------------------------------------------------------------------ python

QString Recommender::ensureScript() {
    const QString dir = workDir_ + "/ai";
    QDir().mkpath(dir);
    const QString path = dir + "/recommender.py";
    QFile res(":/ai/recommender.py");
    if (!res.open(QIODevice::ReadOnly)) return QString();
    const QByteArray data = res.readAll();
    QFile cur(path);
    if (cur.open(QIODevice::ReadOnly)) {
        if (cur.readAll() == data) return path;
        cur.close();
    }
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) return QString();
    out.write(data);
    return path;
}

QString Recommender::engineStatus() const {
    if (!pythonEnabled_) return "Built-in engine (Python disabled)";
    if (probing_ || !probed_) return "Checking Python / PyTorch ...";
    if (!torchInfo_.isEmpty()) return "PyTorch engine (" + torchInfo_ + ")";
    return "Built-in engine (PyTorch not found)";
}

void Recommender::probePython() {
    if (probed_ || probing_ || !pythonEnabled_) return;
    probing_ = true;
    probeQueue_.clear();
    probeQueue_ << "python3" << "python" << "py";
    startNextProbe();
}

void Recommender::startNextProbe() {
    const QString script = ensureScript();
    if (script.isEmpty() || probeQueue_.isEmpty()) {
        probing_ = false;
        probed_ = true;
        emit probeFinished();
        if (waitingForProbe_ && pending_) {
            waitingForProbe_ = false;
            auto p = pending_;
            pending_.reset();
            runPython(p);
        }
        return;
    }
    const QString prog = probeQueue_.takeFirst();
    QStringList prefix;
    if (prog == "py") prefix << "-3";
    delete probeProc_;
    probeProc_ = new QProcess(this);
    auto* proc = probeProc_;
    auto* timer = new QTimer(proc);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, proc, [proc] { proc->kill(); });
    connect(proc, &QProcess::errorOccurred, this, [this, proc, prog](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart && proc == probeProc_) QTimer::singleShot(0, this, [this] { startNextProbe(); });
    });
    connect(proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, proc, prog, prefix](int code, QProcess::ExitStatus st) {
        if (proc != probeProc_) return;
        if (st == QProcess::NormalExit && code == 0) {
            torchInfo_ = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
            pyProgram_ = prog;
            pyPrefix_ = prefix;
            probeQueue_.clear();
        }
        QTimer::singleShot(0, this, [this] { startNextProbe(); });
    });
    timer->start(40000);
    proc->start(prog, prefix + QStringList{script, "--probe"});
}

bool Recommender::parsePythonAnswer(const Prepared& p, const QByteArray& data, Recommendation* out) const {
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) return false;
    const QJsonObject o = doc.object();
    QVector<RankedCandidate> top;
    for (const QJsonValue& v : o.value("top").toArray()) {
        const QJsonObject t = v.toObject();
        RankedCandidate r;
        r.index = t.value("index").toInt(-1);
        if (r.index < 0 || r.index >= p.cands.size()) return false;
        r.total = t.value("total").toDouble();
        r.weakness = t.value("weakness").toDouble();
        r.fit = t.value("fit").toDouble();
        r.novelty = t.value("novelty").toDouble();
        r.predicted = t.value("predicted").toDouble();
        top.push_back(r);
    }
    if (top.isEmpty()) return false;
    *out = build(p, top, o.value("engine").toString("PyTorch"));
    return out->ok;
}

static QByteArray payloadFor(const Recommender* /*unused*/, const QVector<SceneConfig>& cands, const QVector<QVector<int>>& active,
                             const QVector<double>& fw, const QVector<QVector<int>>& hact, const QVector<double>& hscore,
                             const QVector<int>& last, bool hasLast, const QVector<double>& stats,
                             const QVector<double>& cdiff, const QVector<double>& hdiff) {
    QJsonObject o;
    o["feature_count"] = featureCount();
    QJsonArray st;
    for (double x : stats) st.append(x);
    o["stats"] = st;
    QJsonArray fwa;
    for (double x : fw) fwa.append(x);
    o["feature_weights"] = fwa;
    QJsonArray ca;
    for (const auto& a : active) {
        QJsonArray r;
        for (int f : a) r.append(f);
        ca.append(r);
    }
    o["candidates"] = ca;
    QJsonArray da;
    for (double x : cdiff) da.append(x);
    o["difficulty"] = da;
    QJsonArray ha;
    for (int h = 0; h < hact.size(); ++h) {
        QJsonObject e;
        QJsonArray r;
        for (int f : hact[h]) r.append(f);
        e["active"] = r;
        e["score"] = hscore[h];
        e["difficulty"] = hdiff[h];
        ha.append(e);
    }
    o["history"] = ha;
    if (hasLast) {
        QJsonArray la;
        for (int f : last) la.append(f);
        o["last_active"] = la;
    }
    o["target"] = kTarget;
    o["lambda"] = kLambda;
    o["top"] = 5;
    Q_UNUSED(cands);
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

void Recommender::runPython(std::shared_ptr<Prepared> p) {
    if (pyProgram_.isEmpty() || torchInfo_.isEmpty()) {
        emit finished(build(*p, rankBuiltin(*p), "Built-in engine"));
        return;
    }
    const QString script = ensureScript();
    delete runProc_;
    runProc_ = new QProcess(this);
    auto* proc = runProc_;
    const QByteArray payload = payloadFor(this, p->cands, p->active, p->featureWeights, p->histActive, p->histScore, p->lastActive,
                                          p->hasLast, p->req.stats.flat(), p->candDiff, p->histDiff);
    auto* timer = new QTimer(proc);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, proc, [proc] { proc->kill(); });
    auto fallback = [this, p] { emit finished(build(*p, rankBuiltin(*p), "Built-in engine (PyTorch failed)")); };
    connect(proc, &QProcess::errorOccurred, this, [this, proc, fallback](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart && proc == runProc_) { runProc_ = nullptr; proc->deleteLater(); fallback(); }
    });
    connect(proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, proc, p, fallback](int code, QProcess::ExitStatus st) {
        if (proc != runProc_) return;
        runProc_ = nullptr;
        Recommendation rec;
        if (st == QProcess::NormalExit && code == 0 && parsePythonAnswer(*p, proc->readAllStandardOutput(), &rec)) emit finished(rec);
        else fallback();
        proc->deleteLater();
    });
    proc->start(pyProgram_, pyPrefix_ + QStringList{script});
    proc->write(payload);
    proc->closeWriteChannel();
    timer->start(60000);
}

void Recommender::recommendAsync(const RecommendRequest& req) {
    auto p = prepare(req);
    if (!pythonEnabled_) {
        QTimer::singleShot(0, this, [this, p] { emit finished(build(*p, rankBuiltin(*p), "Built-in engine")); });
        return;
    }
    if (!probed_) {
        pending_ = p;
        waitingForProbe_ = true;
        probePython();
        return;
    }
    runPython(p);
}

Recommendation Recommender::recommendBlocking(const RecommendRequest& req) {
    auto p = prepare(req);
    if (pythonEnabled_) {
        if (!probed_) {
            probePython();
            QElapsedTimer t;
            t.start();
            while (!probed_ && t.elapsed() < 60000) QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
        if (!pyProgram_.isEmpty() && !torchInfo_.isEmpty()) {
            const QString script = ensureScript();
            QProcess proc;
            proc.start(pyProgram_, pyPrefix_ + QStringList{script});
            if (proc.waitForStarted(10000)) {
                proc.write(payloadFor(this, p->cands, p->active, p->featureWeights, p->histActive, p->histScore, p->lastActive,
                                      p->hasLast, p->req.stats.flat(), p->candDiff, p->histDiff));
                proc.closeWriteChannel();
                if (proc.waitForFinished(60000) && proc.exitCode() == 0) {
                    Recommendation rec;
                    if (parsePythonAnswer(*p, proc.readAllStandardOutput(), &rec)) return rec;
                }
            }
        }
    }
    return build(*p, rankBuiltin(*p), "Built-in engine");
}

}  // namespace cmd
