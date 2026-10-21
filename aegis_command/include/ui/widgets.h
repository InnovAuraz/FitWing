#pragma once
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>

#include "core/player_store.h"

namespace cmd::ui {

// Framed panel with corner brackets and a small caption.
class Panel : public QFrame {
    Q_OBJECT
public:
    explicit Panel(const QString& title = QString(), QWidget* parent = nullptr);
    QVBoxLayout* body() { return body_; }
    void setTitle(const QString& t);
    void setAccent(const QColor& c) { accent_ = c; update(); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QString title_;
    QVBoxLayout* body_;
    QColor accent_;
};

// Large menu button: index, title, description.
class MenuTile : public QPushButton {
    Q_OBJECT
public:
    MenuTile(const QString& index, const QString& title, const QString& desc, QWidget* parent = nullptr);
    QSize sizeHint() const override { return QSize(300, 150); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QString index_, title_, desc_;
};

// Mastery bars for a list of dimensions (one block per dimension).
class StatBars : public QWidget {
    Q_OBJECT
public:
    explicit StatBars(QWidget* parent = nullptr);
    void setData(const Stats& s, const QVector<int>& dims);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return QSize(280, 60); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    Stats stats_;
    QVector<int> dims_;
};

class RadarChart : public QWidget {
    Q_OBJECT
public:
    explicit RadarChart(QWidget* parent = nullptr);
    void setData(const QStringList& labels, const QVector<double>& values);
    QSize minimumSizeHint() const override { return QSize(240, 240); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QStringList labels_;
    QVector<double> values_;
};

class LineChart : public QWidget {
    Q_OBJECT
public:
    explicit LineChart(QWidget* parent = nullptr);
    void setData(const QVector<double>& values, const QString& emptyText = "No data yet");
    QSize minimumSizeHint() const override { return QSize(260, 150); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QVector<double> values_;
    QString empty_;
};

class ScoreRing : public QWidget {
    Q_OBJECT
public:
    ScoreRing(const QString& caption, QWidget* parent = nullptr);
    void setValue(double v) { value_ = v; update(); }
    QSize sizeHint() const override { return QSize(130, 150); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QString caption_;
    double value_ = 0;
};

struct StageRow {
    QString label;
    double used = 0;
    double window = 1;
    bool missed = false;
};

class StageBars : public QWidget {
    Q_OBJECT
public:
    explicit StageBars(QWidget* parent = nullptr);
    void setRows(const QVector<StageRow>& rows) { rows_ = rows; update(); }
    QSize sizeHint() const override { return QSize(420, 150); }
    QSize minimumSizeHint() const override { return QSize(300, 130); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QVector<StageRow> rows_;
};

QLabel* makeLabel(const QString& text, const char* role = nullptr, QWidget* parent = nullptr);
QFrame* makeSeparator(QWidget* parent = nullptr);
QPushButton* makeButton(const QString& text, const char* role = nullptr, QWidget* parent = nullptr);

}  // namespace cmd::ui
