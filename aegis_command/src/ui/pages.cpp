#include "ui/pages.h"

#include <QApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QStyle>
#include <QUrl>
#include <algorithm>

#include "core/feedback.h"
#include "ui/report.h"
#include "ui/theme.h"

namespace cmd::ui {

namespace {
QWidget* pageTitle(const QString& title, const QString& sub) {
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(2);
    v->addWidget(makeLabel(title.toUpper(), "h1"));
    v->addWidget(makeLabel(sub, "dim"));
    w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    return w;
}

QString levelBadge(int level) {
    static const char* names[] = {"", "RECRUIT", "REGULAR", "VETERAN", "ELITE", "NIGHTMARE"};
    return QString("LEVEL %1 · %2").arg(level).arg(names[std::min(5, std::max(1, level))]);
}


QVBoxLayout* pageLayout(QWidget* w) {
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(36, 26, 36, 26);
    v->setSpacing(16);
    return v;
}
}  // namespace

QString codename(const SceneConfig& s, int index) {
    static const QStringList a = {"IRON", "PALE", "GLASS", "COLD", "RED", "HOLLOW", "SILENT", "BLACK", "AMBER", "STATIC", "WIDOW", "TIDAL"};
    static const QStringList b = {"ORCHARD", "SENTRY", "HARBOR", "LANTERN", "ANTENNA", "RIDGE", "TOLL", "MERIDIAN", "FENCE", "VESPER", "CAUSEWAY", "WATCH"};
    unsigned h = static_cast<unsigned>(index * 7919);
    for (int d = 0; d < DimCount; ++d)
        for (QChar c : s.get(d)) h = h * 31 + c.unicode();
    return a[h % a.size()] + " " + b[(h / 13) % b.size()];
}

// ============================================================ OperatorPage

OperatorPage::OperatorPage(Session& s, QWidget* parent) : QWidget(parent), s_(s) {
    auto* root = pageLayout(this);
    root->addWidget(pageTitle("Operator registry", "Select who is on shift. Every operator keeps an individual mastery profile."));
    auto* row = new QHBoxLayout;
    row->setSpacing(18);
    auto* left = new Panel("Registered operators");
    list_ = new QListWidget;
    list_->setMinimumHeight(260);
    left->body()->addWidget(list_);
    auto* lrow = new QHBoxLayout;
    select_ = makeButton("SELECT OPERATOR", "primary");
    del_ = makeButton("DELETE", "danger");
    lrow->addWidget(select_, 1);
    lrow->addWidget(del_);
    left->body()->addLayout(lrow);
    row->addWidget(left, 3);

    auto* right = new Panel("New operator");
    right->body()->addWidget(makeLabel("Call sign (letters, digits, space, - _)", "dim"));
    name_ = new QLineEdit;
    name_->setMaxLength(24);
    name_->setPlaceholderText("e.g. Overwatch-7");
    right->body()->addWidget(name_);
    auto* create = makeButton("ENLIST", "primary");
    right->body()->addWidget(create);
    msg_ = makeLabel("", "warn");
    msg_->setWordWrap(true);
    right->body()->addWidget(msg_);
    right->body()->addStretch(1);
    right->body()->addWidget(makeLabel("New operators start every skill at 50 / 100.\nProfiles are stored as player_stats.json.", "dim"));
    row->addWidget(right, 2);
    root->addLayout(row, 1);

    connect(create, &QPushButton::clicked, this, &OperatorPage::createOperator);
    connect(name_, &QLineEdit::returnPressed, this, &OperatorPage::createOperator);
    connect(select_, &QPushButton::clicked, this, &OperatorPage::choose);
    connect(list_, &QListWidget::itemDoubleClicked, this, &OperatorPage::choose);
    connect(del_, &QPushButton::clicked, this, &OperatorPage::deleteOperator);
    connect(list_, &QListWidget::currentRowChanged, this, [this](int r) { select_->setEnabled(r >= 0); del_->setEnabled(r >= 0); });
    refresh();
}

void OperatorPage::refresh() {
    list_->clear();
    for (const QString& n : s_.store().list()) {
        const Stats st = s_.store().load(n);
        auto* it = new QListWidgetItem(QString("%1      mastery %2").arg(n, -24).arg(qRound(st.overallMean())));
        it->setData(Qt::UserRole, n);
        it->setFont(monoFont(14));
        list_->addItem(it);
    }
    if (list_->count()) list_->setCurrentRow(0);
    select_->setEnabled(list_->count() > 0);
    del_->setEnabled(list_->count() > 0);
    msg_->clear();
}

void OperatorPage::createOperator() {
    QString err;
    const QString n = PlayerStore::sanitize(name_->text());
    if (!s_.store().create(n, &err)) {
        msg_->setText(err);
        return;
    }
    name_->clear();
    refresh();
    for (int i = 0; i < list_->count(); ++i)
        if (list_->item(i)->data(Qt::UserRole).toString() == n) list_->setCurrentRow(i);
    msg_->setText("Operator " + n + " enlisted.");
}

void OperatorPage::deleteOperator() {
    auto* it = list_->currentItem();
    if (!it) return;
    const QString n = it->data(Qt::UserRole).toString();
    if (QMessageBox::question(this, "Delete operator", "Delete operator '" + n + "' and the whole mastery profile?") != QMessageBox::Yes) return;
    QString err;
    if (!s_.store().remove(n, &err)) msg_->setText(err);
    if (s_.player() == n) s_.clearPlayer();
    refresh();
}

void OperatorPage::choose() {
    auto* it = list_->currentItem();
    if (!it) return;
    if (s_.selectPlayer(it->data(Qt::UserRole).toString())) emit operatorChosen();
}

// ============================================================ MenuPage

MenuPage::MenuPage(QWidget* parent) : QWidget(parent) {
    auto* root = pageLayout(this);
    root->addWidget(pageTitle("Command post", "Choose a training programme."));
    auto* grid = new QGridLayout;
    grid->setSpacing(18);
    auto* a = new MenuTile("01", "CAMPAIGN", "Five fixed missions with rising difficulty. Clear one to unlock the next.");
    auto* b = new MenuTile("02", "AI TRAINING", "The recommender reads your mastery profile and builds the mission that hurts most.");
    auto* c = new MenuTile("03", "DASHBOARD", "Skill bars, radar, strengths and weaknesses, session history.");
    auto* d = new MenuTile("04", "SETTINGS", "Simulator location, training dummy, AI engine.");
    grid->addWidget(a, 0, 0);
    grid->addWidget(b, 0, 1);
    grid->addWidget(c, 1, 0);
    grid->addWidget(d, 1, 1);
    root->addLayout(grid, 1);
    status_ = makeLabel("", "dim");
    root->addWidget(status_);
    connect(a, &QPushButton::clicked, this, &MenuPage::openCampaign);
    connect(b, &QPushButton::clicked, this, &MenuPage::openTraining);
    connect(c, &QPushButton::clicked, this, &MenuPage::openDashboard);
    connect(d, &QPushButton::clicked, this, &MenuPage::openSettings);
}

// ============================================================ CampaignPage

CampaignPage::CampaignPage(Session& s, QWidget* parent) : QWidget(parent), s_(s) {
    auto* root = pageLayout(this);
    auto* top = new QHBoxLayout;
    top->addWidget(pageTitle("Campaign", "Five shifts. Each is harder than the last."), 1);
    auto* fresh = makeButton("NEW CAMPAIGN", "ghost");
    top->addWidget(fresh, 0, Qt::AlignTop);
    root->addLayout(top);
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* inner = new QWidget;
    cards_ = new QVBoxLayout(inner);
    cards_->setContentsMargins(0, 0, 8, 0);
    cards_->setSpacing(12);
    scroll->setWidget(inner);
    root->addWidget(scroll, 1);
    connect(fresh, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, "New campaign", "Generate five new missions and reset campaign progress?") == QMessageBox::Yes) s_.newCampaign();
    });
    connect(&s_, &Session::campaignChanged, this, &CampaignPage::refresh);
    refresh();
}

void CampaignPage::refresh() {
    while (QLayoutItem* it = cards_->takeAt(0)) {
        delete it->widget();
        delete it;
    }
    const auto& camp = s_.campaign();
    for (int i = 0; i < camp.size(); ++i) {
        const SceneConfig& sc = camp[i];
        const bool open = s_.unlocked(i);
        auto* card = new Panel;
        card->setAccent(open ? (s_.done(i) ? kAccent : kAmber) : kLine);
        auto* row = new QHBoxLayout;
        row->setSpacing(18);
        auto* l = new QVBoxLayout;
        l->addWidget(makeLabel(QString("MISSION %1").arg(i + 1, 2, 10, QChar('0')), "dim"));
        auto* nm = makeLabel(open ? codename(sc, i) : "CLASSIFIED", "h2");
        l->addWidget(nm);
        l->addWidget(makeLabel(levelBadge(sc.level), open ? "warn" : "dim"));
        row->addLayout(l, 2);
        auto* m = new QVBoxLayout;
        if (open) {
            for (const QString& t : sc.briefingLines()) {
                auto* lb = makeLabel(t, "dim");
                m->addWidget(lb);
            }
        } else {
            m->addWidget(makeLabel("Complete the previous mission to receive the briefing.", "dim"));
        }
        row->addLayout(m, 6);
        auto* r = new QVBoxLayout;
        QString st;
        if (!open) st = "LOCKED";
        else if (s_.done(i)) st = QString("CLEARED · best %1").arg(qRound(s_.best(i)));
        else if (s_.attempts(i) > 0) st = QString("FAILED · best %1").arg(qRound(s_.best(i)));
        else st = "READY";
        r->addWidget(makeLabel(st, !open ? "dim" : (s_.done(i) ? "good" : "warn")), 0, Qt::AlignRight);
        r->addStretch(1);
        auto* go = makeButton(s_.attempts(i) ? "REDEPLOY" : "DEPLOY", "primary");
        go->setEnabled(open);
        r->addWidget(go);
        row->addLayout(r, 2);
        card->body()->addLayout(row);
        cards_->addWidget(card);
        connect(go, &QPushButton::clicked, this, [this, sc] { emit deploy(sc); });
    }
    cards_->addStretch(1);
}

// ============================================================ TrainingPage

TrainingPage::TrainingPage(Session& s, Recommender& r, QWidget* parent) : QWidget(parent), s_(s), rec_(r) {
    auto* root = pageLayout(this);
    root->addWidget(pageTitle("AI training", "The recommender builds the next mission from your weak spots and this session's results."));
    auto* row = new QHBoxLayout;
    row->setSpacing(18);

    auto* left = new Panel("Skill assessment");
    weak_ = makeLabel("", nullptr);
    weak_->setTextFormat(Qt::RichText);
    weak_->setWordWrap(true);
    weak_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    left->body()->addWidget(weak_, 1);
    left->body()->addWidget(makeLabel("Focus category", "dim"));
    focus_ = new QComboBox;
    focus_->addItem("Automatic (weakest overall)", "");
    for (const QString& c : categories()) focus_->addItem("Focus: " + c, c);
    left->body()->addWidget(focus_);
    analyse_ = makeButton("ANALYSE AND BUILD MISSION", "primary");
    left->body()->addWidget(analyse_);
    row->addWidget(left, 2);

    auto* right = new Panel("Recommended mission");
    engine_ = makeLabel("", "dim");
    right->body()->addWidget(engine_);
    head_ = makeLabel("No mission generated yet.", "h2");
    right->body()->addWidget(head_);
    detail_ = makeLabel("", nullptr);
    detail_->setWordWrap(true);
    detail_->setTextFormat(Qt::RichText);
    right->body()->addWidget(detail_);
    right->body()->addWidget(makeSeparator());
    right->body()->addWidget(makeLabel("WHY THIS MISSION", "h2"));
    reasons_ = makeLabel("", nullptr);
    reasons_->setWordWrap(true);
    reasons_->setTextFormat(Qt::RichText);
    reasons_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    right->body()->addWidget(reasons_, 1);
    deploy_ = makeButton("DEPLOY", "primary");
    deploy_->setEnabled(false);
    right->body()->addWidget(deploy_);
    row->addWidget(right, 3);
    root->addLayout(row, 1);

    connect(analyse_, &QPushButton::clicked, this, &TrainingPage::analyse);
    connect(deploy_, &QPushButton::clicked, this, [this] { if (current_.ok) emit deploy(current_.scene); });
    connect(&rec_, &Recommender::finished, this, [this](const Recommendation& r2) { showRecommendation(r2); });
    refresh();
}

void TrainingPage::refresh() {
    struct E { double v; int d, i; };
    QVector<E> all;
    for (int d = 0; d < DimCount; ++d)
        for (int i = 0; i < dim(d).values.size(); ++i) all.push_back({s_.stats().get(d, i), d, i});
    std::sort(all.begin(), all.end(), [](const E& a, const E& b) { return a.v < b.v; });
    QString html = "<p style='color:#6d8f77'>Weakest conditions right now</p>";
    for (int k = 0; k < 8 && k < all.size(); ++k) {
        const E& e = all[k];
        html += QString("<p style='margin:3px 0'><span style='color:%1;font-weight:bold'>%2</span>  %3 · %4</p>")
                    .arg(masteryColor(e.v).name())
                    .arg(qRound(e.v), 3)
                    .arg(dim(e.d).label.toHtmlEscaped(), valueLabel(e.d, dim(e.d).values[e.i]).toHtmlEscaped());
    }
    if (s_.history().isEmpty()) html += "<p style='color:#6d8f77'>No missions flown this session yet.</p>";
    weak_->setText(html);
    engine_->setText("Engine: " + rec_.engineStatus());
}

void TrainingPage::analyse() {
    analyse_->setEnabled(false);
    deploy_->setEnabled(false);
    head_->setText("Analysing profile ...");
    QApplication::setOverrideCursor(Qt::BusyCursor);
    RecommendRequest req;
    req.stats = s_.stats();
    req.history = s_.historyItems();
    req.focusCategory = focus_->currentData().toString();
    rec_.recommendAsync(req);
}

void TrainingPage::showRecommendation(const Recommendation& r) {
    QApplication::restoreOverrideCursor();
    analyse_->setEnabled(true);
    current_ = r;
    refresh();
    if (!r.ok) {
        head_->setText("Could not build a mission.");
        return;
    }
    engine_->setText("Engine: " + r.engine);
    const SceneConfig& sc = r.scene;
    head_->setText(QString("%1  ·  %2").arg(codename(sc, 9), levelBadge(sc.level)));
    QString html;
    for (const QString& t : sc.briefingLines()) html += "<p style='margin:2px 0'>" + t.toHtmlEscaped() + "</p>";
    html += QString("<p style='color:#e0a63a'>Difficulty index %1 / 100 · predicted score %2</p>").arg(qRound(sc.difficultyScore())).arg(qRound(r.predicted));
    detail_->setText(html);
    QString rs;
    for (const QString& t : r.reasons) rs += "<p style='margin:3px 0'>▸ " + t.toHtmlEscaped() + "</p>";
    reasons_->setText(rs);
    deploy_->setEnabled(true);
}

// ============================================================ DashboardPage

DashboardPage::DashboardPage(Session& s, QWidget* parent) : QWidget(parent), s_(s) {
    auto* root = pageLayout(this);
    auto* top = new QHBoxLayout;
    top->addWidget(pageTitle("Dashboard", "Mastery per condition (0-100). Red = needs work, green = strong."), 1);
    auto* reset = makeButton("RESET PROFILE", "danger");
    top->addWidget(reset, 0, Qt::AlignTop);
    root->addLayout(top);
    auto* row = new QHBoxLayout;
    row->setSpacing(18);

    auto* left = new Panel("Skill matrix");
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    bars_ = new StatBars;
    scroll->setWidget(bars_);
    left->body()->addWidget(scroll);
    row->addWidget(left, 3);

    auto* mid = new QVBoxLayout;
    mid->setSpacing(18);
    auto* rp = new Panel("Category radar");
    auto* rr = new QHBoxLayout;
    ring_ = new ScoreRing("mastery");
    rr->addWidget(ring_);
    radar_ = new RadarChart;
    rr->addWidget(radar_, 1);
    rp->body()->addLayout(rr);
    mid->addWidget(rp, 3);
    auto* lp = new Panel("Session results");
    line_ = new LineChart;
    lp->body()->addWidget(line_);
    session_ = makeLabel("", "dim");
    lp->body()->addWidget(session_);
    mid->addWidget(lp, 2);
    row->addLayout(mid, 3);

    auto* right = new QVBoxLayout;
    right->setSpacing(18);
    auto* sp = new Panel("Strengths");
    strong_ = makeLabel("");
    strong_->setTextFormat(Qt::RichText);
    strong_->setWordWrap(true);
    sp->body()->addWidget(strong_);
    right->addWidget(sp);
    auto* wp = new Panel("Weaknesses");
    weak_ = makeLabel("");
    weak_->setTextFormat(Qt::RichText);
    weak_->setWordWrap(true);
    wp->body()->addWidget(weak_);
    right->addWidget(wp);
    auto* cp = new Panel("Campaign");
    campaign_ = makeLabel("");
    campaign_->setTextFormat(Qt::RichText);
    campaign_->setWordWrap(true);
    cp->body()->addWidget(campaign_);
    right->addWidget(cp);
    right->addStretch(1);
    row->addLayout(right, 2);
    root->addLayout(row, 1);

    connect(reset, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, "Reset profile", "Reset every mastery value of " + s_.player() + " to 50?") == QMessageBox::Yes) s_.resetStats();
    });
    connect(&s_, &Session::statsChanged, this, &DashboardPage::refresh);
    connect(&s_, &Session::historyChanged, this, &DashboardPage::refresh);
    connect(&s_, &Session::campaignChanged, this, &DashboardPage::refresh);
    refresh();
}

void DashboardPage::refresh() {
    const Stats& st = s_.stats();
    QVector<int> dims;
    for (int d = 0; d < DimCount; ++d) dims << d;
    bars_->setData(st, dims);
    ring_->setValue(st.overallMean());
    QStringList labels;
    QVector<double> vals;
    for (const QString& c : categories()) {
        labels << c;
        vals << st.categoryMean(c);
    }
    radar_->setData(labels, vals);

    struct E { double v; int d, i; };
    QVector<E> all;
    for (int d = 0; d < DimCount; ++d)
        for (int i = 0; i < dim(d).values.size(); ++i) all.push_back({st.get(d, i), d, i});
    std::sort(all.begin(), all.end(), [](const E& a, const E& b) { return a.v > b.v; });
    auto fmt = [&](const E& e) {
        return QString("<p style='margin:2px 0'><span style='color:%1;font-weight:bold'>%2</span>  %3: %4</p>")
            .arg(masteryColor(e.v).name())
            .arg(qRound(e.v), 3)
            .arg(dim(e.d).label.toHtmlEscaped(), valueLabel(e.d, dim(e.d).values[e.i]).toHtmlEscaped());
    };
    QString a, b;
    int n = 0;
    for (const E& e : all)
        if (e.v > 52 && n++ < 5) a += fmt(e);
    n = 0;
    for (int k = all.size() - 1; k >= 0 && n < 5; --k)
        if (all[k].v < 48) { b += fmt(all[k]); ++n; }
    strong_->setText(a.isEmpty() ? "<span style='color:#6d8f77'>Nothing stands out yet - fly missions to build a profile.</span>" : a);
    weak_->setText(b.isEmpty() ? "<span style='color:#6d8f77'>No weak spots recorded.</span>" : b);

    QVector<double> hist;
    double sum = 0;
    int wins = 0;
    for (const auto& r : s_.history()) {
        hist << r.perf.overall;
        sum += r.perf.overall;
        wins += r.perf.completed ? 1 : 0;
    }
    line_->setData(hist, "No missions flown this session");
    session_->setText(hist.isEmpty() ? "" : QString("%1 mission(s) · average %2 · cleared %3").arg(hist.size()).arg(qRound(sum / hist.size())).arg(wins));

    int cleared = 0;
    for (int i = 0; i < s_.campaign().size(); ++i) cleared += s_.done(i) ? 1 : 0;
    QString c = QString("<p>Cleared %1 / %2 missions</p>").arg(cleared).arg(s_.campaign().size());
    for (int i = 0; i < s_.campaign().size(); ++i)
        c += QString("<p style='margin:2px 0;color:%1'>M%2  %3</p>")
                 .arg(s_.done(i) ? "#6fe08a" : (s_.unlocked(i) ? "#e0a63a" : "#43594a"))
                 .arg(i + 1)
                 .arg(s_.done(i) ? QString("cleared (best %1)").arg(qRound(s_.best(i))) : (s_.unlocked(i) ? "available" : "locked"));
    campaign_->setText(c);
}

// ============================================================ AarPage

AarPage::AarPage(Session& s, QWidget* parent) : QWidget(parent), s_(s) {
    auto* root = pageLayout(this);
    root->addWidget(pageTitle("After-action report", "What happened, how fast, and what to fix."));
    auto* row = new QHBoxLayout;
    row->setSpacing(18);

    auto* left = new QVBoxLayout;
    left->setSpacing(18);
    auto* rp = new Panel("Result");
    auto* rh = new QHBoxLayout;
    rank_ = makeLabel("-", "big");
    rank_->setMinimumWidth(80);
    rh->addWidget(rank_);
    auto* rv = new QVBoxLayout;
    outcome_ = makeLabel("", "h2");
    rankTitle_ = makeLabel("", "dim");
    sub_ = makeLabel("", "dim");
    rv->addWidget(outcome_);
    rv->addWidget(rankTitle_);
    rv->addWidget(sub_);
    rh->addLayout(rv, 1);
    rp->body()->addLayout(rh);
    auto* rings = new QHBoxLayout;
    ringOverall_ = new ScoreRing("overall");
    ringDecision_ = new ScoreRing("decision");
    ringTime_ = new ScoreRing("timing");
    ringAcc_ = new ScoreRing("accuracy");
    for (auto* r : {ringOverall_, ringDecision_, ringTime_, ringAcc_}) rings->addWidget(r);
    rp->body()->addLayout(rings);
    left->addWidget(rp);
    auto* tp = new Panel("Stage timing vs window");
    stages_ = new StageBars;
    tp->body()->addWidget(stages_);
    actions_ = makeLabel("", "dim");
    tp->body()->addWidget(actions_);
    left->addWidget(tp);
    row->addLayout(left, 5);

    auto* right = new QVBoxLayout;
    right->setSpacing(18);
    auto* fp = new Panel("Debrief");
    feedback_ = makeLabel("");
    feedback_->setTextFormat(Qt::RichText);
    feedback_->setWordWrap(true);
    feedback_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    auto* sc = new QScrollArea;
    sc->setWidgetResizable(true);
    sc->setWidget(feedback_);
    fp->body()->addWidget(sc, 1);
    right->addWidget(fp, 3);
    auto* dp = new Panel("Mastery changes");
    deltas_ = makeLabel("");
    deltas_->setTextFormat(Qt::RichText);
    deltas_->setWordWrap(true);
    dp->body()->addWidget(deltas_);
    right->addWidget(dp, 2);
    row->addLayout(right, 6);
    root->addLayout(row, 1);

    note_ = makeLabel("", "warn");
    root->addWidget(note_);
    auto* btns = new QHBoxLayout;
    auto* pdf = makeButton("EXPORT PDF", "ghost");
    auto* again = makeButton("RETRY MISSION", "ghost");
    next_ = makeButton("NEXT MISSION", "primary");
    auto* menu = makeButton("MAIN MENU", "ghost");
    btns->addWidget(pdf);
    btns->addStretch(1);
    btns->addWidget(again);
    btns->addWidget(next_);
    btns->addWidget(menu);
    root->addLayout(btns);
    connect(pdf, &QPushButton::clicked, this, &AarPage::exportPdf);
    connect(again, &QPushButton::clicked, this, &AarPage::again);
    connect(next_, &QPushButton::clicked, this, &AarPage::nextMission);
    connect(menu, &QPushButton::clicked, this, &AarPage::menu);
}

void AarPage::show(const MissionRecord& rec, const QString& note) {
    rec_ = rec;
    const Performance& p = rec.perf;
    const SceneConfig& sc = rec.scene;
    const QString rank = rankFor(p.overall, p.completed);
    rank_->setText(rank);
    outcome_->setText(p.completed ? "THREAT NEUTRALISED" : "POST LOST");
    outcome_->setProperty("role", p.completed ? "good" : "bad");
    outcome_->style()->unpolish(outcome_);
    outcome_->style()->polish(outcome_);
    rankTitle_->setText(rankTitle(rank));
    sub_->setText(QString("%1 · %2 · mission %3").arg(sc.mode == "campaign" ? "Campaign" : "AI training", levelBadge(sc.level)).arg(sc.missionId));
    ringOverall_->setValue(p.overall);
    ringDecision_->setValue(p.decision);
    ringTime_->setValue(p.time);
    ringAcc_->setValue(p.accuracy);
    const double wd = detectionWindowSeconds(sc.get(DDetWin)), wc = classificationWindowSeconds(sc.get(DClsWin)),
                 wr = responseWindowSeconds(sc.get(DRspWin));
    stages_->setRows({{"Detection", p.detection, wd, p.detection >= wd * 0.98},
                      {"Classification", p.classification, wc, p.classification >= wc * 0.98},
                      {"Response", p.response, wr, false}});
    actions_->setText(QString("Shots fired %1   ·   Missiles fired %2").arg(p.shots).arg(p.missiles));

    const Feedback fb = buildFeedback(sc, p);
    QString html;
    auto block = [&](const QString& title, const QStringList& items, const QColor& c, const QString& mark) {
        html += QString("<p style='color:%1;font-weight:bold;margin-top:8px'>%2</p>").arg(c.name(), title);
        for (const QString& t : items) html += QString("<p style='margin:3px 0'><span style='color:%1'>%2</span> %3</p>").arg(c.name(), mark, t.toHtmlEscaped());
    };
    block("STRENGTHS", fb.strengths, kAccent, "+");
    block("ISSUES", fb.issues, kRed, "−");
    block("RECOMMENDATIONS", fb.tips, kAmber, "▸");
    feedback_->setText(html);

    QVector<StatDelta> ds = rec.deltas;
    std::sort(ds.begin(), ds.end(), [](const StatDelta& a, const StatDelta& b) { return std::fabs(a.after - a.before) > std::fabs(b.after - b.before); });
    QString dh;
    for (int i = 0; i < std::min<int>(6, ds.size()); ++i) {
        const double dv = ds[i].after - ds[i].before;
        dh += QString("<p style='margin:2px 0'><span style='color:%1;font-weight:bold'>%2%3</span>  %4: %5</p>")
                  .arg(dv >= 0 ? "#6fe08a" : "#e05a4a", dv >= 0 ? "+" : "")
                  .arg(dv, 0, 'f', 1)
                  .arg(dim(ds[i].dim).label.toHtmlEscaped(), valueLabel(ds[i].dim, dim(ds[i].dim).values[ds[i].valueIdx]).toHtmlEscaped());
    }
    deltas_->setText(dh.isEmpty() ? "<span style='color:#6d8f77'>No profile selected.</span>" : dh);
    note_->setText(p.mock ? "Training-dummy result: generated without the simulator." : note);
    bool hasNext = false;
    if (sc.mode == "campaign") hasNext = p.completed && sc.level < 5;
    else hasNext = true;
    next_->setText(sc.mode == "campaign" ? "NEXT MISSION" : "NEXT AI MISSION");
    next_->setEnabled(hasNext);
}

void AarPage::exportPdf() {
    const QString def = QString("AAR_%1_%2.pdf").arg(s_.player().isEmpty() ? "operator" : s_.player()).arg(rec_.scene.missionId);
    const QString path = QFileDialog::getSaveFileName(this, "Export after-action report", def, "PDF (*.pdf)");
    if (path.isEmpty()) return;
    QString err;
    if (!exportReport(path, s_.player(), rec_, &err)) {
        QMessageBox::warning(this, "Export failed", err);
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

// ============================================================ DeployPage

DeployPage::DeployPage(QWidget* parent) : QWidget(parent) {
    auto* root = pageLayout(this);
    root->addWidget(pageTitle("Mission in progress", "The simulator window is open. Finish or abort the shift there."));
    auto* p = new Panel("Briefing");
    title_ = makeLabel("", "h2");
    p->body()->addWidget(title_);
    brief_ = makeLabel("", "dim");
    brief_->setWordWrap(true);
    p->body()->addWidget(brief_);
    p->body()->addWidget(makeSeparator());
    state_ = makeLabel("", "warn");
    p->body()->addWidget(state_);
    p->body()->addStretch(1);
    auto* c = makeButton("FORCE TERMINATE SIMULATOR", "danger");
    p->body()->addWidget(c, 0, Qt::AlignLeft);
    root->addWidget(p, 1);
    connect(c, &QPushButton::clicked, this, &DeployPage::cancel);
    blink_ = new QTimer(this);
    blink_->setInterval(500);
    connect(blink_, &QTimer::timeout, this, [this] {
        ++tick_;
        state_->setText(QString(tick_ % 2 ? "● " : "○ ") + "SIMULATOR RUNNING - waiting for the result file");
    });
}

void DeployPage::show(const SceneConfig& scene, bool mock) {
    title_->setText(codename(scene, 3) + "  ·  " + levelBadge(scene.level));
    brief_->setText(scene.briefingLines().join("\n") + "\n\nControls: LMB raise/lower item · RMB next item · Q previous · scroll zoom · SPACE act/tag/fire · "
                                                       "R reload · SHIFT steady · 1/2/3 classify · ENTER start · ESC abort");
    state_->setText(mock ? "TRAINING DUMMY - no simulator window" : "SIMULATOR RUNNING");
    blink_->start();
}

// ============================================================ SettingsDialog

SettingsDialog::SettingsDialog(const QString& simPath, bool mock, bool python, const QString& dataRoot, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Settings");
    setMinimumWidth(560);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(22, 22, 22, 22);
    v->setSpacing(12);
    v->addWidget(makeLabel("SETTINGS", "h2"));
    v->addWidget(makeLabel("Simulator executable (leave empty to use FitWingSim next to this program)", "dim"));
    auto* row = new QHBoxLayout;
    path_ = new QLineEdit(simPath);
    auto* browse = makeButton("BROWSE", "ghost");
    row->addWidget(path_, 1);
    row->addWidget(browse);
    v->addLayout(row);
    mock_ = new QCheckBox("Training dummy: fake the simulator result (for testing the trainer only)");
    mock_->setChecked(mock);
    v->addWidget(mock_);
    py_ = new QCheckBox("Use the Python / PyTorch recommender when available");
    py_->setChecked(python);
    v->addWidget(py_);
    v->addWidget(makeLabel("Data folder: " + dataRoot, "dim"));
    auto* btns = new QHBoxLayout;
    btns->addStretch(1);
    auto* ok = makeButton("SAVE", "primary");
    auto* cancel = makeButton("CANCEL", "ghost");
    btns->addWidget(cancel);
    btns->addWidget(ok);
    v->addLayout(btns);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(this, "Select simulator executable");
        if (!f.isEmpty()) path_->setText(f);
    });
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
}

}  // namespace cmd::ui
