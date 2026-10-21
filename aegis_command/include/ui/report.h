#pragma once
#include <QString>

#include "core/session.h"

namespace cmd::ui {

// Writes the after-action report of one mission as an A4 PDF.
bool exportReport(const QString& path, const QString& operatorName, const MissionRecord& rec, QString* err = nullptr);

}  // namespace cmd::ui
