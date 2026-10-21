#include "ui/report.h"

#include <QFont>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <algorithm>

#include "core/feedback.h"

namespace cmd::ui {

namespace {
struct Ctx {
    QPainter* p;
    QPdfWriter* w;
    double y = 0;     // in px of the pdf device
    double left, right, bottom, top;
    double px(double mm) const { return mm * w->resolution() / 25.4; }
};

void newPage(Ctx& c, const QString& title, const QString& sub) {
    c.p->fillRect(QRectF(0, 0, c.w->width(), c.px(26)), QColor(0x0a, 0x1a, 0x10));
    c.p->setPen(QColor(0x6f, 0xe0, 0x8a));
    QFont f("DejaVu Sans", 22, QFont::Bold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 3);
    c.p->setFont(f);
    c.p->drawText(QRectF(c.left, c.px(5), c.right - c.left, c.px(10)), Qt::AlignLeft | Qt::AlignVCenter, title);
    c.p->setPen(QColor(0xb8, 0xd8, 0xc0));
    c.p->setFont(QFont("DejaVu Sans Mono", 10));
    c.p->drawText(QRectF(c.left, c.px(15), c.right - c.left, c.px(8)), Qt::AlignLeft | Qt::AlignVCenter, sub);
    c.y = c.px(34);
}

void heading(Ctx& c, const QString& t) {
    c.p->setPen(QColor(0x1e, 0x7a, 0x44));
    QFont f("DejaVu Sans", 11, QFont::Bold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    c.p->setFont(f);
    c.p->drawText(QRectF(c.left, c.y, c.right - c.left, c.px(7)), Qt::AlignLeft | Qt::AlignVCenter, t.toUpper());
    c.y += c.px(7.5);
    c.p->setPen(QPen(QColor(0x9c, 0xc8, 0xa8), 1));
    c.p->drawLine(QPointF(c.left, c.y), QPointF(c.right, c.y));
    c.y += c.px(2.5);
}

void para(Ctx& c, const QString& t, const QColor& col = QColor(0x20, 0x28, 0x22), int pt = 10, bool mono = false) {
    c.p->setPen(col);
    c.p->setFont(QFont(mono ? "DejaVu Sans Mono" : "DejaVu Sans", pt));
    QRectF r(c.left, c.y, c.right - c.left, c.px(200));
    QRectF used = c.p->boundingRect(r, Qt::AlignLeft | Qt::TextWordWrap, t);
    if (c.y + used.height() > c.bottom) {
        c.w->newPage();
        newPage(c, "FITWING COMMAND", "AFTER-ACTION REPORT (continued)");
        r = QRectF(c.left, c.y, c.right - c.left, c.px(200));
        used = c.p->boundingRect(r, Qt::AlignLeft | Qt::TextWordWrap, t);
    }
    c.p->drawText(r, Qt::AlignLeft | Qt::TextWordWrap, t);
    c.y += used.height() + c.px(1.6);
}

void bar(Ctx& c, const QString& label, double used, double window, const QString& text, bool missed, bool higherBetter = false) {
    double ratio = missed ? 1.0 : std::min(1.0, used / window);
    const double fillRatio = ratio;
    if (higherBetter) ratio = 1.0 - ratio;
    c.p->setPen(QColor(0x20, 0x28, 0x22));
    c.p->setFont(QFont("DejaVu Sans", 9, QFont::Bold));
    c.p->drawText(QRectF(c.left, c.y, c.px(38), c.px(6)), Qt::AlignLeft | Qt::AlignVCenter, label);
    QRectF track(c.left + c.px(40), c.y + c.px(1), c.px(80), c.px(4));
    c.p->fillRect(track, QColor(0xdc, 0xe8, 0xdf));
    QColor col = missed || ratio > 0.75 ? QColor(0xc0, 0x45, 0x3a) : (ratio > 0.4 ? QColor(0xd0, 0x92, 0x20) : QColor(0x2a, 0x9d, 0x55));
    c.p->fillRect(QRectF(track.left(), track.top(), track.width() * fillRatio, track.height()), col);
    c.p->setPen(QColor(0x20, 0x28, 0x22));
    c.p->setFont(QFont("DejaVu Sans Mono", 9));
    c.p->drawText(QRectF(c.left + c.px(124), c.y, c.px(50), c.px(6)), Qt::AlignLeft | Qt::AlignVCenter, text);
    c.y += c.px(7.5);
}
}  // namespace

bool exportReport(const QString& path, const QString& operatorName, const MissionRecord& rec, QString* err) {
    QPdfWriter w(path);
    w.setResolution(150);
    w.setPageSize(QPageSize(QPageSize::A4));
    w.setPageMargins(QMarginsF(0, 0, 0, 0));
    w.setTitle("FitWing Command - After-Action Report");
    w.setCreator("FitWing Command");
    QPainter p;
    if (!p.begin(&w)) {
        if (err) *err = "Cannot write " + path;
        return false;
    }
    p.setRenderHint(QPainter::Antialiasing, true);
    Ctx c;
    c.p = &p;
    c.w = &w;
    c.left = c.px(16);
    c.right = w.width() - c.px(16);
    c.bottom = w.height() - c.px(18);
    c.top = c.px(34);

    const SceneConfig& s = rec.scene;
    const Performance& r = rec.perf;
    newPage(c, "AFTER-ACTION REPORT", QString("Operator %1   |   Mission %2   |   %3   |   %4")
                                          .arg(operatorName.toUpper())
                                          .arg(s.missionId)
                                          .arg(s.mode == "campaign" ? "CAMPAIGN" : "AI TRAINING")
                                          .arg(rec.when.toString("yyyy-MM-dd HH:mm")));

    // result block
    const QString rank = rankFor(r.overall, r.completed);
    p.fillRect(QRectF(c.left, c.y, c.right - c.left, c.px(30)), QColor(0xee, 0xf5, 0xf0));
    p.setPen(QColor(0x1e, 0x7a, 0x44));
    p.setFont(QFont("DejaVu Sans", 44, QFont::Bold));
    p.drawText(QRectF(c.left + c.px(4), c.y, c.px(26), c.px(30)), Qt::AlignCenter, rank);
    p.setPen(QColor(0x20, 0x28, 0x22));
    p.setFont(QFont("DejaVu Sans", 15, QFont::Bold));
    p.drawText(QRectF(c.left + c.px(36), c.y + c.px(3), c.px(110), c.px(9)), Qt::AlignLeft | Qt::AlignVCenter,
               r.completed ? "THREAT NEUTRALISED" : "POST LOST");
    p.setFont(QFont("DejaVu Sans", 10));
    p.drawText(QRectF(c.left + c.px(36), c.y + c.px(12), c.px(140), c.px(7)), Qt::AlignLeft | Qt::AlignVCenter,
               QString("%1  -  overall score %2 / 100").arg(rankTitle(rank)).arg(r.overall, 0, 'f', 1));
    p.drawText(QRectF(c.left + c.px(36), c.y + c.px(19), c.px(140), c.px(7)), Qt::AlignLeft | Qt::AlignVCenter,
               QString("Difficulty level %1  (index %2 / 100)").arg(s.level).arg(s.difficultyScore(), 0, 'f', 0));
    c.y += c.px(36);

    heading(c, "Scores");
    auto score = [&](const QString& n, double v) { bar(c, n, v, 100, QString("%1 / 100").arg(v, 0, 'f', 1), false, true); };
    score("Decision", r.decision);
    score("Timing", r.time);
    score("Accuracy", r.accuracy);
    score("Overall", r.overall);
    c.y += c.px(3);

    heading(c, "Stage timing");
    const double wd = detectionWindowSeconds(s.get(DDetWin)), wc = classificationWindowSeconds(s.get(DClsWin)),
                 wr = responseWindowSeconds(s.get(DRspWin));
    bar(c, "Detection", r.detection, wd, QString("%1 s / %2 s").arg(r.detection, 0, 'f', 1).arg(wd), r.detection >= wd * 0.98);
    bar(c, "Classification", r.classification, wc, QString("%1 s / %2 s").arg(r.classification, 0, 'f', 1).arg(wc), r.classification >= wc * 0.98);
    bar(c, "Response", r.response, wr, QString("%1 s / %2 s").arg(r.response, 0, 'f', 1).arg(wr), false);
    para(c, QString("Shots fired: %1     Missiles fired: %2").arg(r.shots).arg(r.missiles), QColor(0x20, 0x28, 0x22), 10, true);
    c.y += c.px(2);

    heading(c, "Mission conditions");
    for (const QString& l : s.briefingLines()) para(c, l, QColor(0x20, 0x28, 0x22), 9, true);
    c.y += c.px(2);

    const Feedback fb = buildFeedback(s, r);
    heading(c, "Strengths");
    for (const QString& t : fb.strengths) para(c, "+  " + t, QColor(0x1c, 0x6b, 0x3a));
    heading(c, "Issues");
    for (const QString& t : fb.issues) para(c, "-  " + t, QColor(0xa8, 0x3a, 0x30));
    heading(c, "Recommendations");
    for (const QString& t : fb.tips) para(c, ">  " + t);

    if (!rec.deltas.isEmpty()) {
        c.y += c.px(2);
        heading(c, "Mastery changes");
        QVector<StatDelta> ds = rec.deltas;
        std::sort(ds.begin(), ds.end(), [](const StatDelta& a, const StatDelta& b) { return std::fabs(a.after - a.before) > std::fabs(b.after - b.before); });
        for (int i = 0; i < std::min<int>(8, ds.size()); ++i) {
            const double dv = ds[i].after - ds[i].before;
            para(c, QString("%1: %2   %3 -> %4  (%5%6)")
                        .arg(dim(ds[i].dim).label, -24)
                        .arg(valueLabel(ds[i].dim, dim(ds[i].dim).values[ds[i].valueIdx]), -14)
                        .arg(ds[i].before, 0, 'f', 1)
                        .arg(ds[i].after, 0, 'f', 1)
                        .arg(dv >= 0 ? "+" : "")
                        .arg(dv, 0, 'f', 1),
                 QColor(0x20, 0x28, 0x22), 9, true);
        }
    }
    if (r.mock) para(c, "Note: this result was produced by the built-in training dummy, not by the simulator.", QColor(0xa8, 0x3a, 0x30), 9);
    p.end();
    return true;
}

}  // namespace cmd::ui
