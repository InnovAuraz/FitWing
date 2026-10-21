#pragma once
#include <QColor>
#include <QFont>
#include <QString>

class QApplication;

namespace cmd::ui {

// Field-terminal palette: near-black green, phosphor accent, amber for warnings.
inline const QColor kBg0(0x05, 0x0b, 0x07);
inline const QColor kBg1(0x0a, 0x15, 0x0e);
inline const QColor kBg2(0x10, 0x1f, 0x16);
inline const QColor kLine(0x1f, 0x3a, 0x2a);
inline const QColor kLineBright(0x2f, 0x6b, 0x46);
inline const QColor kText(0xc2, 0xdf, 0xc9);
inline const QColor kDim(0x6d, 0x8f, 0x77);
inline const QColor kAccent(0x6f, 0xe0, 0x8a);
inline const QColor kAmber(0xe0, 0xa6, 0x3a);
inline const QColor kRed(0xe0, 0x5a, 0x4a);

QFont headingFont(int px, bool bold = true);
QFont monoFont(int px, bool bold = false);
QColor masteryColor(double v);  // red < amber < green
void applyTheme(QApplication& app);

}  // namespace cmd::ui
