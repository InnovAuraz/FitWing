#include "ui/main_window.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QEventLoop>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QShortcut>
#include <QStyle>
#include <QTimer>

#include "ui/report.h"
#include "ui/theme.h"

namespace cmd::ui {

class HeaderBar : public QWidget {
public:
    explicit HeaderBar(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedHeight(96);
        auto* lay = new QHBoxLayout(this);
        lay->setContentsMargins(36, 0, 36, 0);
        lay->addStretch(1);
        navLayout_ = new QHBoxLayout;
        navLayout_->setSpacing(2);
        lay->addLayout(navLayout_);
    }
    QPushButton* addNav(const QString& text) {
        auto* b = makeButton(text, "nav");
        b->setProperty("active", false);
        navLayout_->addWidget(b);
        navs_ << b;
        return b;
    }
    void setActive(int idx) {
        for (int i = 0; i < navs_.size(); ++i) {
            navs_[i]->setProperty("active", i == idx);
            navs_[i]->style()->unpolish(navs_[i]);
            navs_[i]->style()->polish(navs_[i]);
        }
    }
    void setNavVisible(bool v) {
        for (auto* b : navs_) b->setVisible(v);
    }
    void setOperator(const QString& n) {
        op_ = n;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        QLinearGradient g(0, 0, 0, height());
        g.setColorAt(0, QColor(0x0b, 0x1a, 0x11));
        g.setColorAt(1, kBg0);
        p.fillRect(rect(), g);
        // faint scanlines
        p.setPen(QColor(111, 224, 138, 10));
        for (int y = 0; y < height(); y += 4) p.drawLine(0, y, width(), y);
        p.setPen(QPen(kLineBright, 1));
        p.drawLine(0, height() - 1, width(), height() - 1);
        p.setPen(QPen(kAccent, 3));
        p.drawLine(36, height() - 1, 36 + 120, height() - 1);
        // mark: shield-ish chevron
        QPainterPath path;
        const QPointF o(36, 22);
        path.moveTo(o + QPointF(0, 0));
        path.lineTo(o + QPointF(22, 8));
        path.lineTo(o + QPointF(44, 0));
        path.lineTo(o + QPointF(40, 30));
        path.lineTo(o + QPointF(22, 46));
        path.lineTo(o + QPointF(4, 30));
        path.closeSubpath();
        p.setPen(QPen(kAccent, 2));
        p.setBrush(QColor(111, 224, 138, 40));
        p.drawPath(path);
        p.setBrush(kAccent);
        p.setPen(Qt::NoPen);
        p.drawEllipse(o + QPointF(22, 20), 4, 4);
        p.setPen(QPen(kAccent, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(o + QPointF(22, 20), 10, 10);
        p.setFont(headingFont(28));
        p.setPen(QColor(0xe8, 0xff, 0xee));
        p.drawText(QRectF(94, 16, 420, 38), Qt::AlignLeft | Qt::AlignVCenter, "FITWING COMMAND");
        p.setFont(monoFont(12));
        p.setPen(kDim);
        QString sub = "COUNTER-UAS TRAINING TERMINAL";
        if (!op_.isEmpty()) sub += "   ·   OPERATOR " + op_.toUpper();
        p.drawText(QRectF(95, 54, 700, 20), Qt::AlignLeft | Qt::AlignVCenter, sub);
    }

private:
    QHBoxLayout* navLayout_;
    QVector<QPushButton*> navs_;
    QString op_;
};

MainWindow::MainWindow(const QString& dataRoot, QWidget* parent)
    : QMainWindow(parent),
      settings_("FitWing", "Command"),
      session_(dataRoot),
      launcher_(session_.store().runtimeDir()),
      recommender_(dataRoot + "/runtime") {
    setWindowTitle("FitWing Command");
    resize(1260, 820);
    setMinimumSize(1100, 720);

    launcher_.setCustomPath(settings_.value("sim/path").toString());
    launcher_.setMock(settings_.value("sim/mock", false).toBool());
    recommender_.setPythonEnabled(settings_.value("ai/python", true).toBool());

    auto* central = new QWidget;
    auto* v = new QVBoxLayout(central);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    header_ = new HeaderBar;
    v->addWidget(header_);
    stack_ = new QStackedWidget;
    v->addWidget(stack_, 1);
    setCentralWidget(central);

    operatorPage_ = new OperatorPage(session_);
    menuPage_ = new MenuPage;
    campaignPage_ = new CampaignPage(session_);
    trainingPage_ = new TrainingPage(session_, recommender_);
    dashboardPage_ = new DashboardPage(session_);
    aarPage_ = new AarPage(session_);
    deployPage_ = new DeployPage;
    for (QWidget* w : std::initializer_list<QWidget*>{operatorPage_, menuPage_, campaignPage_, trainingPage_, dashboardPage_, aarPage_, deployPage_})
        stack_->addWidget(w);

    auto* nMenu = header_->addNav("MENU");
    auto* nCamp = header_->addNav("CAMPAIGN");
    auto* nAi = header_->addNav("AI TRAINING");
    auto* nDash = header_->addNav("DASHBOARD");
    auto* nOp = header_->addNav("SWITCH OPERATOR");
    connect(nMenu, &QPushButton::clicked, this, [this] { go(PgMenu); });
    connect(nCamp, &QPushButton::clicked, this, [this] { go(PgCampaign); });
    connect(nAi, &QPushButton::clicked, this, [this] { go(PgTraining); });
    connect(nDash, &QPushButton::clicked, this, [this] { go(PgDashboard); });
    connect(nOp, &QPushButton::clicked, this, [this] {
        if (launcher_.running()) return;
        session_.clearPlayer();
        operatorPage_->refresh();
        go(PgOperator);
    });

    connect(operatorPage_, &OperatorPage::operatorChosen, this, [this] { go(PgMenu); });
    connect(menuPage_, &MenuPage::openCampaign, this, [this] { go(PgCampaign); });
    connect(menuPage_, &MenuPage::openTraining, this, [this] { go(PgTraining); });
    connect(menuPage_, &MenuPage::openDashboard, this, [this] { go(PgDashboard); });
    connect(menuPage_, &MenuPage::openSettings, this, [this] { openSettings(); });
    connect(campaignPage_, &CampaignPage::deploy, this, [this](const SceneConfig& s) { returnPage_ = PgCampaign; deploy(s); });
    connect(trainingPage_, &TrainingPage::deploy, this, [this](const SceneConfig& s) { returnPage_ = PgTraining; deploy(s); });
    connect(deployPage_, &DeployPage::cancel, this, [this] { launcher_.terminate(); });
    connect(&launcher_, &SimLauncher::finished, this, &MainWindow::onFinished);
    connect(&session_, &Session::statsChanged, trainingPage_, &TrainingPage::refresh);
    connect(&session_, &Session::historyChanged, trainingPage_, &TrainingPage::refresh);

    connect(aarPage_, &AarPage::menu, this, [this] { go(PgMenu); });
    connect(aarPage_, &AarPage::again, this, [this] { deploy(lastScene_); });
    connect(aarPage_, &AarPage::nextMission, this, [this] {
        if (lastScene_.mode == "campaign") {
            const int next = lastScene_.level;  // index of the following mission
            if (next < session_.campaign().size()) {
                returnPage_ = PgCampaign;
                deploy(session_.campaign()[next]);
            } else go(PgCampaign);
        } else {
            go(PgTraining);
            trainingPage_->analyse();
        }
    });

    new QShortcut(QKeySequence(Qt::Key_Escape), this, [this] {
        const int cur = stack_->currentIndex();
        if (cur == PgDeploy || cur == PgOperator || cur == PgMenu) return;
        go(PgMenu);
    });

    recommender_.probePython();
    go(PgOperator);
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* e) {
    if (launcher_.running()) {
        if (QMessageBox::question(this, "Mission running", "The simulator is still running. Close it and exit?") != QMessageBox::Yes) {
            e->ignore();
            return;
        }
        launcher_.terminate();
    }
    e->accept();
}

void MainWindow::go(Page p) {
    if (session_.player().isEmpty() && p != PgOperator) p = PgOperator;
    if (p == PgMenu) {
        menuPage_->setStatus(QString("Operator %1  ·  mastery %2  ·  simulator: %3")
                                 .arg(session_.player())
                                 .arg(qRound(session_.stats().overallMean()))
                                 .arg(launcher_.mock() ? "TRAINING DUMMY" : (launcher_.simAvailable() ? "ready" : "NOT FOUND (see Settings)")));
    }
    if (p == PgTraining) trainingPage_->refresh();
    if (p == PgDashboard) dashboardPage_->refresh();
    stack_->setCurrentIndex(p);
    updateChrome();
}

void MainWindow::updateChrome() {
    const int cur = stack_->currentIndex();
    header_->setOperator(session_.player());
    header_->setNavVisible(cur != PgOperator);
    int active = -1;
    if (cur == PgMenu) active = 0;
    else if (cur == PgCampaign) active = 1;
    else if (cur == PgTraining) active = 2;
    else if (cur == PgDashboard) active = 3;
    header_->setActive(active);
}

void MainWindow::openSettings() {
    SettingsDialog dlg(settings_.value("sim/path").toString(), launcher_.mock(), settings_.value("ai/python", true).toBool(),
                       session_.store().root(), this);
    if (dlg.exec() != QDialog::Accepted) return;
    settings_.setValue("sim/path", dlg.simPath());
    settings_.setValue("sim/mock", dlg.mock());
    settings_.setValue("ai/python", dlg.python());
    launcher_.setCustomPath(dlg.simPath());
    launcher_.setMock(dlg.mock());
    recommender_.setPythonEnabled(dlg.python());
    if (dlg.python()) recommender_.probePython();
    go(PgMenu);
}

void MainWindow::deploy(const SceneConfig& scene) {
    if (launcher_.running()) return;
    if (!launcher_.mock() && !launcher_.simAvailable()) {
        QMessageBox box(this);
        box.setWindowTitle("Simulator not found");
        box.setText("FitWingSim was not found next to the trainer.\n\nPut FitWingSim(.exe) in the same folder, set its path in Settings, "
                    "or use the training dummy to test the trainer without the simulator.");
        auto* dummy = box.addButton("Use training dummy", QMessageBox::AcceptRole);
        box.addButton("Cancel", QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() != dummy) return;
        launcher_.setMock(true);
    }
    lastScene_ = session_.stamp(scene);
    QString err;
    if (!launcher_.launch(lastScene_, session_.stats(), &err)) {
        QMessageBox::warning(this, "Cannot start mission", err);
        return;
    }
    deployPage_->show(lastScene_, launcher_.mock());
    go(PgDeploy);
    showMinimized();  // get out of the way of the simulator window
    if (launcher_.mock()) showNormal();
}

void MainWindow::onFinished(bool ok, const Performance& perf, const QString& message) {
    showNormal();
    raise();
    activateWindow();
    if (!ok) {
        QMessageBox::information(this, "No result", message.isEmpty() ? "No result was recorded." : message);
        go(returnPage_);
        return;
    }
    Performance p = perf;
    p.mock = launcher_.mock();
    const MissionRecord& rec = session_.record(lastScene_, p);
    aarPage_->show(rec);
    go(PgAar);
}

int MainWindow::saveScreenshots(const QString& dir) {
    QDir().mkpath(dir);
    session_.store().remove("Demo");
    session_.store().create("Demo");
    operatorPage_->refresh();
    show();
    QApplication::processEvents();
    auto snap = [&](const QString& name) {
        QApplication::processEvents();
        grab().save(dir + "/" + name + ".png");
    };
    snap("1_operator");
    session_.selectPlayer("Demo");
    go(PgMenu);
    snap("2_menu");
    launcher_.setMock(true);
    // fly a handful of dummy missions so every screen has data
    for (int i = 0; i < 4; ++i) {
        SceneConfig s = session_.stamp(session_.campaign()[std::min(i, 4)]);
        lastScene_ = s;
        Performance p = SimLauncher::fakeResult(s, session_.stats());
        p.completed = i != 2;
        p.mock = true;
        session_.record(s, p);
    }
    go(PgCampaign);
    snap("3_campaign");
    recommender_.setPythonEnabled(false);
    trainingPage_->refresh();
    go(PgTraining);
    trainingPage_->analyse();
    QEventLoop loop;
    QTimer::singleShot(1500, &loop, &QEventLoop::quit);
    loop.exec();
    snap("4_training");
    go(PgDashboard);
    snap("5_dashboard");
    aarPage_->show(*session_.lastRecord());
    go(PgAar);
    snap("6_aar");
    exportReport(dir + "/aar.pdf", session_.player(), *session_.lastRecord());
    deployPage_->show(lastScene_, false);
    go(PgDeploy);
    snap("7_deploy");
    return 0;
}

}  // namespace cmd::ui
