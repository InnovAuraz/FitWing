#pragma once
#include <QMainWindow>
#include <QSettings>
#include <QStackedWidget>

#include "core/recommender.h"
#include "core/session.h"
#include "core/sim_launcher.h"
#include "ui/pages.h"

namespace cmd::ui {

class HeaderBar;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString& dataRoot, QWidget* parent = nullptr);
    ~MainWindow() override;

    enum Page { PgOperator, PgMenu, PgCampaign, PgTraining, PgDashboard, PgAar, PgDeploy };
    void go(Page p);

    // Offscreen helper: fills a demo profile and saves a screenshot of every page into `dir`.
    int saveScreenshots(const QString& dir);

protected:
    void closeEvent(QCloseEvent*) override;

private:
    void deploy(const SceneConfig& scene);
    void onFinished(bool ok, const Performance& perf, const QString& message);
    void openSettings();
    void updateChrome();

    QSettings settings_;
    Session session_;
    SimLauncher launcher_;
    Recommender recommender_;
    HeaderBar* header_;
    QStackedWidget* stack_;
    OperatorPage* operatorPage_;
    MenuPage* menuPage_;
    CampaignPage* campaignPage_;
    TrainingPage* trainingPage_;
    DashboardPage* dashboardPage_;
    AarPage* aarPage_;
    DeployPage* deployPage_;
    SceneConfig lastScene_;
    Page returnPage_ = PgMenu;
};

}  // namespace cmd::ui
