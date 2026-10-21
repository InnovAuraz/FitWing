#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QStringList>
#include <cstdio>

#include "core/generator.h"
#include "core/performance.h"
#include "core/player_store.h"
#include "core/sim_launcher.h"
#include "selftest.h"
#include "ui/main_window.h"
#include "ui/theme.h"

// Usage:
//   FitWingCommand                       normal start
//   FitWingCommand --selftest            run the logic tests and exit (0 = ok)
//   FitWingCommand --data <dir>          use another data folder
//   FitWingCommand --screenshot <dir>    (dev) render all pages with demo data into <dir>
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("FitWingCommand");
    app.setOrganizationName("FitWing");
    QString data, shots;
    bool selftest = false;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "--selftest") selftest = true;
        else if (args[i] == "--data" && i + 1 < args.size()) data = args[++i];
        else if (args[i] == "--emit-scene" && i + 2 < args.size()) {  // dev: <file> <level>
            cmd::SceneConfig sc = cmd::Generator().generate(args[i + 2].toInt(), "campaign");
            sc.missionId = cmd::newMissionId();
            return sc.writeFile(args[i + 1]) ? 0 : 1;
        } else if (args[i] == "--check-perf" && i + 1 < args.size()) {  // dev: <file>
            cmd::Performance pf;
            QString e;
            const bool ok = cmd::Performance::readFile(args[i + 1], &pf, &e);
            std::printf("%s id=%d overall=%.1f completed=%d %s\n", ok ? "OK" : "BAD", pf.missionId, pf.overall, pf.completed, qPrintable(e));
            return ok ? 0 : 1;
        } else if (args[i] == "--launch-test") {  // dev: runs the real launcher (FITWING_SIM_PATH) once
            cmd::SimLauncher l("/tmp/fitwing_launch_test");
            cmd::SceneConfig sc = cmd::Generator().generate(2, "campaign");
            sc.missionId = cmd::newMissionId();
            int rc = 2;
            QObject::connect(&l, &cmd::SimLauncher::finished, [&](bool ok, const cmd::Performance& p, const QString& m) {
                std::printf("launcher: ok=%d id=%d/%d overall=%.1f %s\n", ok, p.missionId, sc.missionId, p.overall, qPrintable(m));
                rc = ok ? 0 : 1;
                QCoreApplication::quit();
            });
            QString e;
            if (!l.launch(sc, cmd::Stats(), &e)) { std::printf("launch failed: %s\n", qPrintable(e)); return 1; }
            app.exec();
            return rc;
        } else if (args[i] == "--screenshot" && i + 1 < args.size()) shots = args[++i];
    }
    if (selftest) return cmd::runSelfTest() == 0 ? 0 : 1;
    cmd::ui::applyTheme(app);
    if (data.isEmpty()) data = cmd::PlayerStore::defaultRoot();
    cmd::ui::MainWindow w(data);
    if (!shots.isEmpty()) return w.saveScreenshots(shots);
    w.show();
    return app.exec();
}
