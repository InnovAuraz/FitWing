#include "ui/widgets.h"

#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include "ui/theme.h"

namespace cmd::ui {

QLabel* makeLabel(const QString& text, const char* role, QWidget* parent) {
    auto* l = new QLabel(text, parent);
    if (role) l->setProperty("role", role);
    l->setWordWrap(false);
    return l;
}

QFrame* makeSeparator(QWidget* parent) {
    auto* f = new QFrame(parent);
    f->setProperty("role", "sep");
    f->setFixedHeight(1);
    return f;
}

QPushButton* makeButton(const QString& text, const char* role, QWidget* parent) {
    auto* b = new QPushButton(text, parent);
    if (role) b->setProperty("role", role);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

static void brackets(QPainter& p, const QRectF& r, const QColor& c, double len = 14) {
    p.setPen(QPen(c, 2));
    p.drawLine(r.topLeft(), r.topLeft() + QPointF(len, 0));
    p.drawLine(r.topLeft(), r.topLeft() + QPointF(0, len));
    p.drawLine(r.topRight(), r.topRight() + QPointF(-len, 0));
    p.drawLine(r.topRight(), r.topRight() + QPointF(0, len));
    p.drawLine(r.bottomLeft(), r.bottomLeft() + QPointF(len, 0));
    p.drawLine(r.bottomLeft(), r.bottomLeft() + QPointF(0, -len));
    p.drawLine(r.bottomRight(), r.bottomRight() + QPointF(-len, 0));
    p.drawLine(r.bottomRight(), r.bottomRight() + QPointF(0, -len));
}

// ------------------------------------------------------------------ Panel

Panel::Panel(const QString& title, QWidget* parent) : QFrame(parent), title_(title), accent_(kLineBright) {
    body_ = new QVBoxLayout(this);
    body_->setContentsMargins(18, title.isEmpty() ? 18 : 40, 18, 18);
    body_->setSpacing(10);
}

void Panel::setTitle(const QString& t) {
    title_ = t;
    body_->setContentsMargins(18, t.isEmpty() ? 18 : 40, 18, 18);
    update();
}

void Panel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0, kBg2);
    g.setColorAt(1, kBg1);
    p.fillRect(r, g);
    p.setPen(QPen(kLine, 1));
    p.drawRect(r);
    brackets(p, r, accent_);
    if (!title_.isEmpty()) {
        p.setFont(headingFont(12));
        p.setPen(kAccent);
        p.drawText(QRectF(18, 10, width() - 36, 22), Qt::AlignLeft | Qt::AlignVCenter, title_.toUpper());
        p.setPen(QPen(kLine, 1));
        p.drawLine(QPointF(18, 34), QPointF(width() - 18, 34));
    }
}

// ------------------------------------------------------------------ MenuTile

MenuTile::MenuTile(const QString& index, const QString& title, const QString& desc, QWidget* parent)
    : QPushButton(parent), index_(index), title_(title), desc_(desc) {
    setCursor(Qt::PointingHandCursor);
    setMinimumSize(280, 140);
    setFlat(true);
    setFocusPolicy(Qt::StrongFocus);
}

void MenuTile::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const bool hot = underMouse() || hasFocus();
    QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    QLinearGradient g(r.topLeft(), r.bottomRight());
    g.setColorAt(0, hot ? QColor(0x17, 0x33, 0x22) : kBg2);
    g.setColorAt(1, hot ? QColor(0x0d, 0x22, 0x15) : kBg1);
    p.fillRect(r, g);
    p.setPen(QPen(hot ? kAccent : kLine, 1));
    p.drawRect(r);
    brackets(p, r, hot ? kAccent : kLineBright);
    p.setFont(monoFont(13, true));
    p.setPen(hot ? kAccent : kDim);
    p.drawText(QRectF(22, 16, 60, 20), Qt::AlignLeft, index_);
    p.setFont(headingFont(22));
    p.setPen(hot ? QColor(0xf0, 0xff, 0xf4) : QColor(0xdc, 0xf0, 0xe0));
    p.drawText(QRectF(22, 44, width() - 44, 34), Qt::AlignLeft | Qt::AlignVCenter, title_);
    p.setFont(monoFont(12));
    p.setPen(kDim);
    p.drawText(QRectF(22, 84, width() - 44, height() - 96), Qt::AlignLeft | Qt::TextWordWrap, desc_);
    if (hot) {
        p.setPen(Qt::NoPen);
        p.setBrush(kAccent);
        QPolygonF tri;
        tri << QPointF(width() - 34, 22) << QPointF(width() - 20, 29) << QPointF(width() - 34, 36);
        p.drawPolygon(tri);
    }
}

// ------------------------------------------------------------------ StatBars

StatBars::StatBars(QWidget* parent) : QWidget(parent) {}

void StatBars::setData(const Stats& s, const QVector<int>& dims) {
    stats_ = s;
    dims_ = dims;
    updateGeometry();
    update();
}

QSize StatBars::sizeHint() const {
    int h = 6;
    for (int d : dims_) h += 26 + dim(d).values.size() * 22 + 8;
    return QSize(420, h);
}

void StatBars::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    int y = 4;
    const int labelW = 130, numW = 38;
    for (int d : dims_) {
        p.setFont(headingFont(11));
        p.setPen(kAccent);
        p.drawText(QRect(0, y, width(), 22), Qt::AlignLeft | Qt::AlignVCenter, dim(d).label.toUpper());
        y += 26;
        for (int i = 0; i < dim(d).values.size(); ++i) {
            const double v = stats_.get(d, i);
            p.setFont(monoFont(12));
            p.setPen(kText);
            p.drawText(QRect(8, y, labelW, 20), Qt::AlignLeft | Qt::AlignVCenter,
                       valueLabel(d, dim(d).values[i]));
            QRectF track(8 + labelW, y + 5, width() - labelW - numW - 20, 10);
            p.fillRect(track, kBg0);
            p.setPen(QPen(kLine, 1));
            p.drawRect(track);
            QRectF fill = track.adjusted(1, 1, -1, -1);
            fill.setWidth(fill.width() * v / 100.0);
            p.fillRect(fill, masteryColor(v));
            p.setPen(masteryColor(v));
            p.setFont(monoFont(12, true));
            p.drawText(QRect(width() - numW - 4, y, numW, 20), Qt::AlignRight | Qt::AlignVCenter, QString::number(qRound(v)));
            y += 22;
        }
        y += 8;
    }
}

// ------------------------------------------------------------------ RadarChart

RadarChart::RadarChart(QWidget* parent) : QWidget(parent) { setMinimumSize(240, 240); }

void RadarChart::setData(const QStringList& labels, const QVector<double>& values) {
    labels_ = labels;
    values_ = values;
    update();
}

void RadarChart::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const int n = values_.size();
    if (n < 3) return;
    const QPointF c(width() / 2.0, height() / 2.0);
    const double R = std::min(width() - 130, height() - 60) / 2.0;
    auto pt = [&](int i, double frac) {
        const double a = -M_PI / 2 + 2 * M_PI * i / n;
        return QPointF(c.x() + std::cos(a) * R * frac, c.y() + std::sin(a) * R * frac);
    };
    p.setPen(QPen(kLine, 1));
    for (int ring = 1; ring <= 4; ++ring) {
        QPolygonF poly;
        for (int i = 0; i < n; ++i) poly << pt(i, ring / 4.0);
        p.drawPolygon(poly);
    }
    for (int i = 0; i < n; ++i) p.drawLine(c, pt(i, 1.0));
    QPolygonF area;
    for (int i = 0; i < n; ++i) area << pt(i, std::min(1.0, std::max(0.0, values_[i] / 100.0)));
    QColor fill = kAccent;
    fill.setAlpha(60);
    p.setBrush(fill);
    p.setPen(QPen(kAccent, 2));
    p.drawPolygon(area);
    p.setBrush(kAccent);
    for (const QPointF& q : area) p.drawEllipse(q, 3.5, 3.5);
    p.setFont(monoFont(11));
    p.setPen(kText);
    for (int i = 0; i < n; ++i) {
        const QPointF q = pt(i, 1.0);
        const QString t = labels_.value(i) + "\n" + QString::number(qRound(values_[i]));
        QRectF rc(q.x() - 60, q.y() - 20, 120, 40);
        const double a = -M_PI / 2 + 2 * M_PI * i / n;
        rc.translate(std::cos(a) * 34, std::sin(a) * 22);
        if (rc.left() < 2) rc.moveLeft(2);
        if (rc.right() > width() - 2) rc.moveRight(width() - 2);
        p.drawText(rc, Qt::AlignCenter, t);
    }
}

// ------------------------------------------------------------------ LineChart

LineChart::LineChart(QWidget* parent) : QWidget(parent) { setMinimumSize(260, 150); }

void LineChart::setData(const QVector<double>& values, const QString& emptyText) {
    values_ = values;
    empty_ = emptyText;
    update();
}

void LineChart::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF plot(34, 10, width() - 48, height() - 34);
    p.setFont(monoFont(10));
    for (int g = 0; g <= 4; ++g) {
        const double y = plot.bottom() - plot.height() * g / 4.0;
        p.setPen(QPen(g == 0 ? kLineBright : kLine, 1, g == 0 ? Qt::SolidLine : Qt::DotLine));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(kDim);
        p.drawText(QRectF(0, y - 8, 28, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(g * 25));
    }
    if (values_.isEmpty()) {
        p.setPen(kDim);
        p.setFont(monoFont(12));
        p.drawText(plot, Qt::AlignCenter, empty_);
        return;
    }
    const int n = values_.size();
    auto pt = [&](int i) {
        const double x = n == 1 ? plot.center().x() : plot.left() + plot.width() * i / (n - 1.0);
        return QPointF(x, plot.bottom() - plot.height() * std::min(100.0, std::max(0.0, values_[i])) / 100.0);
    };
    if (n > 1) {
        QPainterPath path;
        path.moveTo(pt(0));
        for (int i = 1; i < n; ++i) path.lineTo(pt(i));
        QPainterPath area = path;
        area.lineTo(plot.right(), plot.bottom());
        area.lineTo(plot.left(), plot.bottom());
        area.closeSubpath();
        QLinearGradient g(plot.topLeft(), plot.bottomLeft());
        QColor a = kAccent;
        a.setAlpha(70);
        QColor b = kAccent;
        b.setAlpha(0);
        g.setColorAt(0, a);
        g.setColorAt(1, b);
        p.fillPath(area, g);
        p.setPen(QPen(kAccent, 2));
        p.drawPath(path);
    }
    for (int i = 0; i < n; ++i) {
        p.setBrush(masteryColor(values_[i]));
        p.setPen(QPen(kBg0, 2));
        p.drawEllipse(pt(i), 5, 5);
        p.setPen(kDim);
        p.drawText(QRectF(pt(i).x() - 14, plot.bottom() + 4, 28, 16), Qt::AlignCenter, QString::number(i + 1));
    }
}

// ------------------------------------------------------------------ ScoreRing

ScoreRing::ScoreRing(const QString& caption, QWidget* parent) : QWidget(parent), caption_(caption) { setMinimumSize(110, 130); }

void ScoreRing::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const int d = std::min(width(), height() - 26) - 14;
    QRectF r((width() - d) / 2.0, 6, d, d);
    p.setPen(QPen(kLine, 8, Qt::SolidLine, Qt::FlatCap));
    p.drawArc(r, 0, 360 * 16);
    const QColor c = masteryColor(value_);
    p.setPen(QPen(c, 8, Qt::SolidLine, Qt::FlatCap));
    p.drawArc(r, 90 * 16, -static_cast<int>(360.0 * 16 * std::min(100.0, std::max(0.0, value_)) / 100.0));
    p.setPen(kText);
    p.setFont(monoFont(std::max(16, d / 4), true));
    p.drawText(r, Qt::AlignCenter, QString::number(qRound(value_)));
    p.setFont(headingFont(11));
    p.setPen(kDim);
    p.drawText(QRectF(0, r.bottom() + 6, width(), 18), Qt::AlignCenter, caption_.toUpper());
}

// ------------------------------------------------------------------ StageBars

StageBars::StageBars(QWidget* parent) : QWidget(parent) {}

void StageBars::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const int rowH = std::max(34, height() / std::max<int>(1, rows_.size()));
    const int labelW = 150, textW = 120;
    int y = 0;
    for (const StageRow& r : rows_) {
        p.setFont(headingFont(11));
        p.setPen(kText);
        p.drawText(QRect(0, y, labelW, rowH), Qt::AlignLeft | Qt::AlignVCenter, r.label.toUpper());
        QRectF track(labelW, y + rowH / 2.0 - 8, width() - labelW - textW - 8, 16);
        p.fillRect(track, kBg0);
        p.setPen(QPen(kLine, 1));
        p.drawRect(track);
        const double ratio = r.window > 0 ? r.used / r.window : 1.0;
        QColor c = r.missed ? kRed : (ratio <= 0.4 ? kAccent : (ratio <= 0.75 ? kAmber : kRed));
        QRectF fill = track.adjusted(1, 1, -1, -1);
        fill.setWidth(fill.width() * std::min(1.0, std::max(0.0, ratio)));
        p.fillRect(fill, c);
        // quarter marks
        p.setPen(QPen(QColor(0, 0, 0, 90), 1));
        for (int q = 1; q < 4; ++q) p.drawLine(QPointF(track.left() + track.width() * q / 4, track.top() + 1),
                                               QPointF(track.left() + track.width() * q / 4, track.bottom() - 1));
        p.setFont(monoFont(12, true));
        p.setPen(c);
        p.drawText(QRect(width() - textW, y, textW, rowH), Qt::AlignRight | Qt::AlignVCenter,
                   r.missed ? QString("MISSED / %1 s").arg(qRound(r.window)) : QString("%1 s / %2 s").arg(r.used, 0, 'f', 1).arg(qRound(r.window)));
        y += rowH;
    }
}

}  // namespace cmd::ui
