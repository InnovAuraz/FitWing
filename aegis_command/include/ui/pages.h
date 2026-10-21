#pragma once
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QTimer>
#include <QWidget>

#include "core/recommender.h"
#include "core/session.h"
#include "core/sim_launcher.h"
#include "ui/widgets.h"

namespace cmd::ui {

QString codename(const SceneConfig& s, int index);

class OperatorPage : public QWidget {
    Q_OBJECT
public:
    explicit OperatorPage(Session& s, QWidget* parent = nullptr);
    void refresh();
signals:
    void operatorChosen();

private:
    void createOperator();
    void deleteOperator();
    void choose();
    Session& s_;
    QListWidget* list_;
    QLineEdit* name_;
    QLabel* msg_;
    QPushButton* select_;
    QPushButton* del_;
};

class MenuPage : public QWidget {
    Q_OBJECT
public:
    explicit MenuPage(QWidget* parent = nullptr);
    void setStatus(const QString& text) { status_->setText(text); }
signals:
    void openCampaign();
    void openTraining();
    void openDashboard();
    void openSettings();

private:
    QLabel* status_;
};

class CampaignPage : public QWidget {
    Q_OBJECT
public:
    explicit CampaignPage(Session& s, QWidget* parent = nullptr);
    void refresh();
signals:
    void deploy(const cmd::SceneConfig& scene);

private:
    Session& s_;
    QVBoxLayout* cards_;
};

class TrainingPage : public QWidget {
    Q_OBJECT
public:
    TrainingPage(Session& s, Recommender& r, QWidget* parent = nullptr);
    void refresh();
    void analyse();
signals:
    void deploy(const cmd::SceneConfig& scene);

private:
    void showRecommendation(const Recommendation& rec);
    Session& s_;
    Recommender& rec_;
    QLabel* weak_;
    QComboBox* focus_;
    QPushButton* analyse_;
    QPushButton* deploy_;
    QLabel* engine_;
    QLabel* head_;
    QLabel* detail_;
    QLabel* reasons_;
    Recommendation current_;
};

class DashboardPage : public QWidget {
    Q_OBJECT
public:
    explicit DashboardPage(Session& s, QWidget* parent = nullptr);
    void refresh();

private:
    Session& s_;
    StatBars* bars_;
    RadarChart* radar_;
    LineChart* line_;
    ScoreRing* ring_;
    QLabel* strong_;
    QLabel* weak_;
    QLabel* campaign_;
    QLabel* session_;
};

class AarPage : public QWidget {
    Q_OBJECT
public:
    explicit AarPage(Session& s, QWidget* parent = nullptr);
    void show(const MissionRecord& rec, const QString& note = QString());
signals:
    void again();
    void menu();
    void nextMission();

private:
    void exportPdf();
    Session& s_;
    MissionRecord rec_;
    QLabel* rank_;
    QLabel* rankTitle_;
    QLabel* outcome_;
    QLabel* sub_;
    ScoreRing* ringOverall_;
    ScoreRing* ringDecision_;
    ScoreRing* ringTime_;
    ScoreRing* ringAcc_;
    StageBars* stages_;
    QLabel* actions_;
    QLabel* feedback_;
    QLabel* deltas_;
    QLabel* note_;
    QPushButton* next_;
};

class DeployPage : public QWidget {
    Q_OBJECT
public:
    explicit DeployPage(QWidget* parent = nullptr);
    void show(const SceneConfig& scene, bool mock);
signals:
    void cancel();

private:
    QLabel* title_;
    QLabel* brief_;
    QLabel* state_;
    QTimer* blink_;
    int tick_ = 0;
};

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(const QString& simPath, bool mock, bool python, const QString& dataRoot, QWidget* parent = nullptr);
    QString simPath() const { return path_->text().trimmed(); }
    bool mock() const { return mock_->isChecked(); }
    bool python() const { return py_->isChecked(); }

private:
    QLineEdit* path_;
    QCheckBox* mock_;
    QCheckBox* py_;
};

}  // namespace cmd::ui
