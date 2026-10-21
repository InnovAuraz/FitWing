#include "ui/theme.h"

#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QPalette>
#include <QStringList>

namespace cmd::ui {

static QString pick(const QStringList& names, const QString& fallback) {
    const QStringList have = QFontDatabase::families();
    for (const QString& n : names)
        if (have.contains(n, Qt::CaseInsensitive)) return n;
    return fallback;
}

QFont headingFont(int px, bool bold) {
    static const QString fam = pick({"Bahnschrift", "Segoe UI Semibold", "Segoe UI", "Roboto Condensed", "DejaVu Sans"}, "sans-serif");
    QFont f(fam);
    f.setPixelSize(px);
    f.setBold(bold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, px >= 20 ? 2.0 : 1.2);
    return f;
}

QFont monoFont(int px, bool bold) {
    static const QString fam = pick({"Cascadia Mono", "Consolas", "DejaVu Sans Mono", "Liberation Mono", "Courier New"}, "monospace");
    QFont f(fam);
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(px);
    f.setBold(bold);
    return f;
}

QColor masteryColor(double v) {
    if (v < 40) return kRed;
    if (v < 60) return kAmber;
    return kAccent;
}

void applyTheme(QApplication& app) {
    app.setStyle("Fusion");
    QPalette p;
    p.setColor(QPalette::Window, kBg0);
    p.setColor(QPalette::WindowText, kText);
    p.setColor(QPalette::Base, kBg1);
    p.setColor(QPalette::AlternateBase, kBg2);
    p.setColor(QPalette::Text, kText);
    p.setColor(QPalette::Button, kBg2);
    p.setColor(QPalette::ButtonText, kText);
    p.setColor(QPalette::Highlight, kLineBright);
    p.setColor(QPalette::HighlightedText, QColor(0xe8, 0xff, 0xee));
    p.setColor(QPalette::ToolTipBase, kBg2);
    p.setColor(QPalette::ToolTipText, kText);
    p.setColor(QPalette::PlaceholderText, kDim);
    p.setColor(QPalette::Disabled, QPalette::Text, kDim);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, kDim);
    app.setPalette(p);
    QFont base = monoFont(13);
    base.setLetterSpacing(QFont::AbsoluteSpacing, 0.2);
    app.setFont(base);
    QFile f(":/theme/aegis.qss");
    if (f.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(f.readAll()));
}

}  // namespace cmd::ui
