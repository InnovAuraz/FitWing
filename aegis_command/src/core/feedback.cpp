#include "core/feedback.h"

namespace cmd {

QString actualThreatName(const SceneConfig& s) {
    const QString t = s.get(DThreatType);
    if (t == "single") return "a single drone";
    if (t == "swarm") return QString("one swarm of %1 drones").arg(s.get(DDroneCount));
    return QString("%1 swarms of %2 drones").arg(s.get(DSwarmCount), s.get(DDroneCount));
}

Feedback buildFeedback(const SceneConfig& s, const Performance& p) {
    Feedback f;
    const double wd = detectionWindowSeconds(s.get(DDetWin));
    const double wc = classificationWindowSeconds(s.get(DClsWin));
    const double wr = responseWindowSeconds(s.get(DRspWin));
    const double rd = p.detection / wd, rc = p.classification / wc, rr = p.response / wr;
    const bool single = s.get(DThreatType) == "single";

    // detection
    if (rd >= 0.98) {
        f.issues << "The threat was not detected before the detection window closed.";
        f.tips << "Sweep the horizon in sectors with the binoculars, then cross-check the acoustic and RF tablets - a bearing from either narrows the search.";
    } else if (rd <= 0.35) {
        f.strengths << QString("Detection was fast (%1 s of %2 s).").arg(p.detection, 0, 'f', 1).arg(qRound(wd));
    } else if (rd > 0.7) {
        f.issues << QString("Detection was slow (%1 s of a %2 s window).").arg(p.detection, 0, 'f', 1).arg(qRound(wd));
        f.tips << "Do not stare at one spot: use low zoom to scan, raise zoom only when a contact appears.";
    }
    // classification
    if (p.decision < 40 && p.classification < wc * 0.98) {
        f.issues << "The threat was classified wrongly - it was " + actualThreatName(s) + ".";
        f.tips << "Count contacts before you decide: one point = single, one group = swarm, several groups = multiple swarms.";
    } else if (rc >= 0.98) {
        f.issues << "Classification ran out of time.";
        f.tips << "Commit to a classification as soon as the contact count is clear - a late correct call scores worse than an early one.";
    } else if (rc <= 0.4 && p.decision >= 60) {
        f.strengths << "Quick, correct classification.";
    }
    // response
    if (p.shots + p.missiles == 0) {
        f.issues << "No weapon was fired.";
        f.tips << "Once classified, engage immediately - the response window starts the moment you commit.";
    } else {
        if (rr <= 0.4) f.strengths << "Fast response after classification.";
        else if (rr > 0.8) f.issues << "The response came very late in the window.";
    }
    // weapon choice
    if (single && p.missiles > 0) {
        f.issues << "A missile was spent on a single drone.";
        f.tips << "Single drones are a rifle target; keep the launcher for swarms.";
    }
    if (!single && p.missiles == 0 && p.shots > 0) {
        f.issues << "A swarm cannot be stopped with the rifle alone.";
        f.tips << "Lock the launcher on the swarm centre and fire - then use the rifle on stragglers.";
    }
    if (p.shots > 10 && p.accuracy < 45) {
        f.issues << QString("%1 shots for low accuracy (%2%).").arg(p.shots).arg(qRound(p.accuracy));
        f.tips << "Hold Shift to steady the rifle and fire on the exhale of the sway.";
    } else if (p.accuracy >= 75 && p.shots > 0) {
        f.strengths << "Disciplined, accurate fire.";
    }
    // conditions
    if (s.get(DTime) == "night" && s.get(DBinoculars) != "unavailable")
        f.tips << "At night the binoculars switch to night vision; thermal still shows warm rotors through fog and rain.";
    if (s.get(DThermal) == "unavailable" && s.get(DTime) == "night")
        f.tips << "Without thermal at night rely on the acoustic tablet to get a bearing first.";
    if (!p.completed)
        f.issues.prepend("The post was lost - the threat reached the base before it was neutralised.");
    else
        f.strengths.prepend("Threat neutralised.");

    if (f.strengths.isEmpty()) f.strengths << "You stayed on station for the whole shift.";
    if (f.issues.isEmpty()) f.issues << "No significant errors recorded.";
    if (f.tips.isEmpty()) f.tips << "Raise the difficulty: the next mission will tighten the time windows.";
    return f;
}

}  // namespace cmd
