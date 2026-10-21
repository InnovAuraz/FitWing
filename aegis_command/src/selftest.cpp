#include "selftest.h"

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <cstdio>

#include "core/generator.h"
#include "core/recommender.h"
#include "core/session.h"
#include "core/sim_launcher.h"

namespace cmd {
namespace {
int g_fail = 0;
int g_pass = 0;
void check(bool ok, const QString& what) {
    if (ok) ++g_pass;
    else {
        ++g_fail;
        std::printf("  FAIL: %s\n", qPrintable(what));
    }
}
}  // namespace

int runSelfTest() {
    g_fail = g_pass = 0;
    QTemporaryDir tmp;
    std::printf("[selftest] data dir %s\n", qPrintable(tmp.path()));

    // --- vocabulary / json round trip
    {
        SceneConfig s;
        s.missionId = 42;
        s.set(DThreatType, "multiple_swarm");
        s.set(DSwarmCount, "3");
        s.set(DDroneCount, "10");
        s.set(DPosition, "different");
        s.level = 5;
        SceneConfig b;
        QString err;
        check(SceneConfig::fromJsonText(s.toJsonText(), &b, &err), "scene json parses: " + err);
        check(b == s && b.missionId == 42, "scene json round trip");
        check(s.toJsonText().contains("\"swarm_count\": 3"), "numeric values are json numbers");
        check(featureCount() == 64, QString("feature count is %1").arg(featureCount()));
    }

    // --- generator validity and difficulty bands
    {
        QElapsedTimer tm; tm.start();
        Generator g(12345);
        for (int level = 1; level <= 5; ++level) {
            int bad = 0, outside = 0;
            double lo = 1000, hi = -1;
            for (int i = 0; i < 300; ++i) {
                SceneConfig s = g.generate(level, "campaign");
                if (!s.isValid()) ++bad;
                const double sc = s.difficultyScore();
                lo = std::min(lo, sc);
                hi = std::max(hi, sc);
                if (levelFromScore(sc) != level || s.level != level) ++outside;
            }
            std::printf("  level %d: score range %.1f .. %.1f\n", level, lo, hi);
            check(bad == 0, QString("level %1 scenes are valid").arg(level));
            check(outside == 0, QString("level %1 scenes sit in the level band").arg(level));
        }
        auto camp = g.campaign();
        check(camp.size() == 5, "campaign has 5 missions");
        bool rising = true;
        for (int i = 1; i < camp.size(); ++i) rising &= camp[i].difficultyScore() > camp[i - 1].difficultyScore();
        check(rising, "campaign difficulty rises");
        int bad = 0;
        for (const auto& c : g.candidates(300)) bad += c.isValid() ? 0 : 1;
        check(bad == 0, "recommender candidates are valid");
        std::printf("  generator time %lld ms\n", (long long)tm.elapsed());
    }

    // --- stats file + update rule
    {
        PlayerStore store(tmp.path());
        QString err;
        check(store.create("Test Pilot", &err), "create player " + err);
        check(!store.create("Test Pilot"), "duplicate player rejected");
        check(store.list() == QStringList{"Test Pilot"}, "player listed");
        Stats s = store.load("Test Pilot");
        check(qFuzzyCompare(s.overallMean(), 50.0), "fresh stats are 50");
        Stats back;
        check(Stats::fromJsonText(s.toJsonText(), &back), "stats json parses");
        check(s.toJsonText().contains("\"1\":") && s.toJsonText().contains("very_short"), "stats json has schema keys");

        SceneConfig easy;  // easy scene, level 1
        easy.level = 1;
        SceneConfig hard = easy;
        hard.level = 5;
        hard.set(DTime, "night");
        hard.set(DWeather, "storm");
        Performance fail;
        fail.overall = 10;
        Stats a, b;
        applyResult(a, easy, fail);
        applyResult(b, hard, fail);
        check(a.get(DTime, "day") < b.get(DTime, "night"), "failing an easy scene hurts more than failing a hard one");
        Performance win;
        win.overall = 90;
        Stats c, d;
        applyResult(c, easy, win);
        SceneConfig hard2 = easy;
        hard2.level = 5;
        applyResult(d, hard2, win);
        check(d.get(DTime, "day") > c.get(DTime, "day"), "winning a hard scene rewards more");
        check(c.get(DTime, "day") > 50 && a.get(DTime, "day") < 50, "direction of the update");
        Stats e;
        for (int i = 0; i < 50; ++i) applyResult(e, easy, win);
        check(e.get(DTime, "day") <= 100.0 && e.get(DTime, "day") > 85, "stats saturate below 100");
        check(qFuzzyCompare(e.get(DTime, "night"), 50.0), "untouched values stay unchanged");
        check(store.save("Test Pilot", e), "stats saved");
        check(qAbs(store.load("Test Pilot").get(DTime, "day") - e.get(DTime, "day")) < 0.01, "stats reload");
        check(store.reset("Test Pilot"), "reset");
        check(qFuzzyCompare(store.load("Test Pilot").overallMean(), 50.0), "reset restores 50");
        check(store.remove("Test Pilot") && store.list().isEmpty(), "delete player");
    }

    // --- performance parsing and mock sim
    {
        PlayerStore store(tmp.path());
        Generator g(7);
        SceneConfig s = g.generate(3, "campaign");
        s.missionId = 777;
        Performance p = SimLauncher::fakeResult(s, Stats());
        Performance q;
        check(Performance::fromJsonText(p.toJsonText(), &q), "performance json parses");
        check(q.missionId == 777 && q.level == 3 && q.mode == "campaign", "performance mission block");
        check(p.toJsonText().contains("\"shots fired\"") && p.toJsonText().contains("\"missiles fired\""), "performance action keys");
        check(!Performance::fromJsonText("{ nope", &q), "bad performance rejected");
    }

    // --- session
    {
        Session ses(tmp.path());
        ses.store().create("Alpha");
        check(ses.selectPlayer("Alpha"), "select player");
        check(ses.unlocked(0) && !ses.unlocked(1), "campaign mission 1 unlocked only");
        SceneConfig s = ses.stamp(ses.campaign()[0]);
        Performance p = SimLauncher::fakeResult(s, ses.stats());
        p.completed = true;
        p.overall = 77;
        const auto& rec = ses.record(s, p);
        check(!rec.deltas.isEmpty(), "record applies stat deltas");
        check(ses.unlocked(1) && ses.done(0) && ses.best(0) == 77, "campaign progress");
        check(ses.history().size() == 1, "history");
        check(qAbs(ses.store().load("Alpha").overallMean() - 50.0) > 0.001, "stats persisted by session");
    }

    // --- recommender: built-in and (when available) PyTorch must agree
    {
        Recommender rec(tmp.path() + "/rt");
        RecommendRequest req;
        req.seed = 99;
        // a player that is weak at night + fog + forest
        for (int i = 0; i < 64; ++i) {
        }
        Stats st;
        st.set(DTime, 2, 5);
        st.set(DWeather, 3, 5);
        st.set(DTerrain, 2, 5);
        for (int d = 0; d < DimCount; ++d)
            for (int i = 0; i < dim(d).values.size(); ++i)
                if (!((d == DTime && i == 2) || (d == DWeather && i == 3) || (d == DTerrain && i == 2))) st.set(d, i, 85);
        req.stats = st;
        Recommendation a = rec.recommendBuiltin(req);
        check(a.ok && a.scene.isValid(), "built-in recommendation is a valid scene");
        check(a.scene.get(DTime) == "night" && a.scene.get(DWeather) == "fog" && a.scene.get(DTerrain) == "forest",
              "recommender targets the weak areas (got " + a.scene.headline() + " / " + a.scene.get(DWeather) + ")");
        std::printf("  built-in pick: %s  predicted %.1f  level %d\n", qPrintable(a.scene.headline()), a.predicted, a.scene.level);
        // with some history
        for (int i = 0; i < 4; ++i) {
            SceneConfig h = Generator(100 + i).generate(2 + i % 3, "ai_training");
            req.history.push_back({h, 40.0 + 10 * i});
        }
        Recommendation a2 = rec.recommendBuiltin(req);
        check(a2.ok, "built-in recommendation with history");
        QElapsedTimer t2; t2.start();
        Recommendation b = rec.recommendBlocking(req);
        std::printf("  recommendBlocking %lld ms\n", (long long)t2.elapsed());
        std::printf("  engine: %s\n", qPrintable(b.engine));
        check(b.ok, "blocking recommendation");
        if (b.engine.startsWith("PyTorch")) {
            check(b.scene == a2.scene, "PyTorch and built-in engines agree");
            check(qAbs(b.predicted - a2.predicted) < 1e-6, "predicted score agrees");
        } else {
            std::printf("  (PyTorch not available - fallback verified only)\n");
        }
    }

    std::printf("[selftest] %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}

}  // namespace cmd
