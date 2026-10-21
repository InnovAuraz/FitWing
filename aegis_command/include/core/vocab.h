#pragma once
// Locked vocabulary shared with the simulator (scene_definition.json / player_stats.json).
#include <QString>
#include <QStringList>
#include <QVector>

namespace cmd {

enum Dim {
    DTime, DWeather, DTerrain, DVisibility,
    DBinoculars, DAudio, DSignal, DThermal,
    DClue, DDecoys,
    DThreatType, DSwarmCount, DDroneCount, DPosition, DApproach, DSpeed,
    DDetWin, DClsWin, DRspWin,
    DimCount
};

struct DimInfo {
    QString group;      // json object name, e.g. "environment"
    QString key;        // json key, e.g. "time"
    QString label;      // human readable
    QString category;   // UI category: Environment / Sensors / Intel / Threat / Time pressure
    QStringList values; // vocabulary, in json order (numbers stored as text: "1", "5", ...)
    QVector<double> hardness;  // 0..1 per value (0 easiest)
    double weight;             // contribution to scene difficulty
    bool numeric;              // serialised as a JSON number in scene_definition.json
};

const DimInfo& dim(int d);
int valueIndex(int d, const QString& v);          // -1 when unknown
QString valueLabel(int d, const QString& v);      // pretty text
QStringList categories();                         // ordered UI categories
QVector<int> dimsOfCategory(const QString& cat);

// flat feature index (one-hot layout used by the recommender): offset of dim d, value i
int featureOffset(int d);
int featureCount();

// fixed windows used by the simulator, in seconds
double detectionWindowSeconds(const QString& v);
double classificationWindowSeconds(const QString& v);
double responseWindowSeconds(const QString& v);

QStringList modes();   // campaign, ai_training

}  // namespace cmd
