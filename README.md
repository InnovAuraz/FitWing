# AI-Enabled Drone & Counter-Drone Threat Simulation Trainer

> **An immersive first-person training simulator that makes human judgment under uncertainty observable, explainable, repeatable, and improvable.**

The AI-Enabled Drone & Counter-Drone Threat Simulation Trainer is a desktop-first, offline-capable training concept for practising drone-threat observation, classification, and decision-making in controlled synthetic scenarios. It combines a first-person 3D simulation with a deterministic assessment engine, evidence-linked scoring, and an After-Action Review (AAR) workflow.

> **Project status:** This README describes the planned architecture and MVP. Update the implementation status as modules are completed and tested. Planned capabilities should not be interpreted as validated operational performance.

## Table of Contents

- [Problem](#problem)
- [Our Approach](#our-approach)
- [Key Features](#key-features)
- [Training Workflow](#training-workflow)
- [Architecture](#architecture)
- [Technology Stack](#technology-stack)
- [Assessment and Decision X-Ray](#assessment-and-decision-x-ray)
- [AI and Adaptive Learning](#ai-and-adaptive-learning)
- [Scenario Design](#scenario-design)
- [Data and Replay](#data-and-replay)
- [MVP Roadmap](#mvp-roadmap)
- [Deployment Goals](#deployment-goals)
- [Project Boundaries](#project-boundaries)
- [Contributing](#contributing)

## Problem

Drone and swarm scenarios can be difficult to practise repeatedly through live drills because they can be costly, weather-dependent, and difficult to reproduce consistently. Classroom instruction alone may not provide enough varied repetitions for personnel to practise judgment under uncertainty.

The proposed trainer provides repeatable synthetic scenarios and records the learner's decisions so instructors can review **what evidence was available, what was considered, when the decision was made, and why the assessment was assigned**.

## Our Approach

The system combines three ideas:

1. **Immersive simulation:** A fixed first-person viewpoint presents synthetic environments, contacts, sensor cues, and distractions.
2. **Auditable assessment:** A deterministic, versioned decision tree evaluates learner choices against the evidence available at the time.
3. **Targeted practice:** After each mission, the AAR highlights a learning gap and recommends a suitable next scenario.

The central loop is:

```text
Generate Scenario
      ↓
Simulate Environment and Cues
      ↓
Observe → Investigate → Classify → Decide
      ↓
Record Events and Snapshots
      ↓
Evaluate Decision Quality
      ↓
Decision X-Ray / After-Action Review
      ↓
Recommend Next Mission
      ↺
```

## Key Features

- **First-person desktop simulation** with day, twilight, and night conditions; weather, visibility, and terrain variations.
- **Synthetic sensor presentation** with visual/binocular, audio, signal, and thermal modes, including degraded or unavailable states.
- **Scripted and bounded procedural scenarios** generated from reviewed templates and a locked vocabulary.
- **Single-contact and swarm scenarios**, with controlled configurations for threat counts, directions, approaches, and speeds.
- **Evidence-linked assessment** of detection latency, classification, confidence calibration, review behaviour, and decision timing.
- **Decision X-Ray** showing the evidence trail, decision-tree path, score rationale, timing, and a concrete improvement action.
- **Bounded counterfactual replay** to compare one alternate decision from a saved pre-decision state. It is a simulated comparison, not a prediction.
- **Readiness Passport** showing training progress across defined dimensions. It is a progress indicator, not a qualification or certification.
- **Offline-first operation** targeted at standard Windows desktops or laptops, without mandatory specialised simulation hardware.

## Training Workflow

1. **Create:** Select a reviewed template and difficulty profile; generate a seeded scenario.
2. **Challenge:** Present a synthetic environment with controlled uncertainty, visibility limits, distractions, and sensor conditions.
3. **Decide:** The learner detects a contact, reviews available cues, classifies it, declares confidence, and chooses a generic simulated response, monitoring, abstention, or escalation option.
4. **Measure:** The assessment engine evaluates the action using the scenario rubric and evidence available at the decision time.
5. **Improve:** The AAR explains the outcome of the assessment and recommends a targeted next mission.

## Architecture

```text
┌──────────────────────────────────────────────┐
│                 C++ / Qt 6                   │
│ Profiles · Mission selection · Validation    │
│ Assessment · AAR · Readiness Passport         │
└──────────────────────┬───────────────────────┘
                       │ mission package + seed
                       ▼
┌──────────────────────────────────────────────┐
│              C++ / OpenGL App                │
│ First-person scene · Synthetic sensors       │
│ Threat movement · Input · Fixed-tick core    │
│ Events and snapshots                         │
└──────────────────────┬───────────────────────┘
                       │ events.jsonl + result.json
                       ▼
┌──────────────────────────────────────────────┐
│             Qt 6 Assessment Core              │
│ Validate · Ingest · Evaluate · Explain       │
│ Update training history · Select next mission│
└──────────────────────┬───────────────────────┘
                       │ aggregate features
                       ▼
┌──────────────────────────────────────────────┐
│             Python / PyTorch                 │
│ Advisory model evaluation · Learner-gap       │
│ analysis · Candidate mission ranking          │
└──────────────────────────────────────────────┘

      SQLite local persistence + JSON / JSONL data contracts
```

### Component Responsibilities

- **Qt 6 controller:** Owns profiles, mission and campaign selection, difficulty settings, package validation, result ingestion, assessment, AAR, and instructor views.
- **OpenGL mission app:** Loads one complete mission package, runs the mission, presents synthetic cues, records learner inputs, and writes events, snapshots, and a result summary. It does not manage campaign history or choose the next mission.
- **Assessment core:** Validates and ingests the event history, applies the versioned decision tree, and produces explainable assessment outputs.
- **Python/PyTorch layer:** Supports bounded model training and analysis outside the real-time rendering path. Deterministic rules remain the scoring authority and fallback.

## Technology Stack

| Technology | Planned Role |
|---|---|
| **C++** | Simulation core, mission runtime, state updates, event and snapshot writing |
| **Qt 6** | Desktop application, controller UI, assessment views, AAR, instructor interface |
| **OpenGL** | First-person 3D rendering, environment presentation, visual effects, and HUD |
| **Python** | AI orchestration, offline analysis, model training and evaluation workflows |
| **PyTorch** | Training and evaluating bounded advisory and learner-analysis models |
| **SQLite** | Local learner profiles, mission history, and training records |
| **JSON** | Mission manifests, schedules, and result summaries |
| **JSONL** | Chronological simulation and learner event stream |
| **Windows Desktop** | Primary offline-capable deployment target |

**VR is a future extension, not an MVP dependency.** The initial goal is a stable desktop experience.

## Assessment and Decision X-Ray

The assessment engine is designed to explain decisions rather than return an opaque score. A versioned decision tree considers factors such as:

- Was there sufficient relevant evidence at the time of the decision?
- Was the learner's declared confidence appropriately calibrated?
- Was the decision made within the applicable time window?
- Was evidence reviewed where review was feasible?
- Did the learner commit, abstain, escalate, or miss the decision window?

The Decision X-Ray is intended to show:

- Evidence available versus evidence reviewed.
- Detection, classification, and response timing.
- Learner classification and declared confidence.
- The decision-tree path, rubric version, evidence references, and reason codes.
- The simulated response and later outcome as separate concepts.
- One concrete improvement action and, where supported, one bounded alternate-decision replay.

**Decision quality is scored separately from the eventual simulated outcome.** A sound decision should not automatically be marked poor because a random simulated event turned out unfavourably; a weak decision should not be excused because the outcome happened to be favourable.

## AI and Adaptive Learning

### Analyst Assist

The planned Analyst Assist provides a **synthetic, fallible, non-authoritative advisory** based on simulated, role-visible cues. The learner may follow, challenge, or override the advice. The advisory does not determine the score, permitted response, or mission solvability.

### Adaptive Mission Selection

The first adaptive layer uses transparent rules to select candidate missions from the approved scenario vocabulary. Python/PyTorch may support learner-gap analysis and candidate ranking, but recommendations should identify the attempts and performance patterns that triggered them. Instructor override and periodic untargeted missions remain supported design goals.

The AI layer does **not** run inside the OpenGL frame loop. A deterministic rule-based fallback is required if the Python/AI layer is unavailable.

## Scenario Design

The initial scenario generator uses a bounded vocabulary rather than unrestricted generation.

| Category | Planned Options |
|---|---|
| Time | Day, twilight, night |
| Weather | Clear, cloudy, rain, fog, storm |
| Terrain | Rural, urban, forest, coastal |
| Visibility | Excellent, good, moderate, poor, very poor |
| Sensor state | Available, degraded, unavailable, or defined quality levels |
| Threat configuration | Single, swarm, multiple swarms |
| Approach | Stationary, crossing, converging |
| Speed | Slow, normal, fast |
| Decision windows | Long, moderate, short, very short |

A seeded generator selects a reviewed template and difficulty profile, materialises the scenario, checks for unsupported or contradictory combinations, and writes a complete manifest. The same mission seed and versioned rules are intended to support reproducible review.

## Data and Replay

Each mission uses its own directory so that concurrent or later missions do not overwrite a shared set of files.

```text
missions/
└── mission_<id>/
    ├── manifest.json
    ├── advisory_schedule.json
    ├── handoff_schedule.json
    ├── events.jsonl
    ├── result.json
    └── snapshots/
        ├── snapshot_<id>.json
        └── ...
```

- `manifest.json` describes the concrete scenario, seed, allowed controls, timing windows, and version information.
- `events.jsonl` stores the canonical chronological event stream, including learner actions and relevant scenario events.
- `snapshots/` stores state references for replay and review.
- `result.json` summarises completion and integrity information; it is not the source of truth for scoring.

The application is planned to validate mission IDs, schema versions, manifest hashes, and event-file hashes before accepting results. Writes should be atomic, and incomplete packages should be rejected safely.

Simulation state advances on a **fixed tick** (planned default: 100 ms). Rendering interpolation must not mutate simulation state or change assessment results.

## MVP Roadmap

The implementation plan prioritises a working end-to-end training loop over breadth.

### P0A — Hero Vertical Slice

- One first-person mission, prioritising a night/twilight urban scenario with reduced visibility and a degraded sensor.
- A primary contact and limited decoys.
- Detection, investigation, classification, confidence, abstention/escalation, and generic response controls.
- Mission package, event log, deterministic assessment, Decision X-Ray, bounded alternate replay, and one targeted next-mission recommendation.

### P0B — Competition Breadth

- Add a clear day/rural baseline and a degraded-sensor scenario with conflicting advisory information.
- Add a five-drone swarm mission if the core experience is stable.
- Provide a minimum Readiness Passport and seeded regeneration.

### Later Priorities

- Multiple-swarm variants and instructor cohort views.
- Advisory model card, learner-gap model ranking, and a multi-mission campaign.
- Additional asset packs, richer effects, advanced authoring, and an optional VR rendering adapter.

If the end-to-end hero mission is not stable, advanced visuals, multiple swarms, and runtime AI ranking should be deferred.

## Deployment Goals

- Windows desktop/laptop deployment.
- Offline operation with no mandatory network connection.
- No administrator dependency where practical.
- Pinned dependency versions and repeatable packaging.
- CPU-only PyTorch unless a GPU requirement is explicitly accepted.
- A rule-based fallback if the Python AI sidecar fails.

## Project Boundaries

This project is a **synthetic training simulator**. The MVP is not intended to be:

- A live sensor integration.
- A weapon-control system or real-world targeting aid.
- An autonomous decision-maker.
- A real map or command-and-control interface.
- A physically accurate flight or electronic-warfare simulator.
- An operational qualification or certification authority.

Threats, sensor cues, advisories, responses, and outcomes are synthetic and should be labelled as simulated. The system is designed to support training and review, not replace field training or real-world operational procedures.

## Contributing

Contributions should preserve the project's core principles:

- Keep scenario generation bounded, seeded, and reproducible.
- Keep rendering separate from simulation state and assessment.
- Make score assignments traceable to evidence, a rule version, and a decision-tree leaf.
- Keep AI advisory-only and outside the real-time render loop.
- Add tests for schema validation, event ordering, deterministic replay, assessment boundaries, and crash recovery.
- Prioritise a stable end-to-end mission before expanding scope.

Build and installation instructions will be added once the repository's build configuration and pinned dependency versions are finalised.

---

**Project Positioning:** An immersive first-person drone-threat trainer that combines the visual experience of a simulation with the auditability of a transparent decision-assessment system.
