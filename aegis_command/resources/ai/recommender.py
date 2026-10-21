#!/usr/bin/env python3
"""
FITWING COMMAND - mission recommender (PyTorch layer).

Protocol (used by the Qt trainer through QProcess):
    python recommender.py --probe      -> prints "torch <version>" and exits 0 (exit 3 when torch is missing)
    python recommender.py              -> reads one JSON request on stdin, writes one JSON answer on stdout

The maths is intentionally identical to the C++ fallback in src/core/recommender.cpp so that
both engines pick the same mission:

  weakness(c)  weighted mean of (100 - mastery)/100 over the 19 options that c activates
  prior(c)     mean mastery over the activated options minus 0.5 * (difficulty index - 50)  (expected score)
  ridge        online ridge regression on the session history that corrects prior(c)
  fit(c)       1 - |predicted score - target| / 45   (stay in the learning zone)
  novelty(c)   mean of 1 / (1 + times the option was already trained)
  total(c)     0.40 * weakness_normalised + 0.45 * fit + 0.15 * novelty
"""
import json
import sys


def probe():
    try:
        import torch  # noqa: F401
    except Exception as exc:  # pragma: no cover
        sys.stderr.write("torch missing: %s\n" % exc)
        return 3
    print("torch " + torch.__version__)
    return 0


def run(req):
    import torch

    torch.set_grad_enabled(False)
    dt = torch.float64
    nfeat = int(req["feature_count"])
    stats = torch.tensor(req["stats"], dtype=dt)
    fw = torch.tensor(req["feature_weights"], dtype=dt)
    target = float(req.get("target", 62.0))
    lam = float(req.get("lambda", 2.0))
    top_k = int(req.get("top", 5))

    def onehot(active):
        x = torch.zeros(nfeat, dtype=dt)
        if len(active):
            x[torch.tensor(active, dtype=torch.long)] = 1.0
        return x

    cands = torch.stack([onehot(c) for c in req["candidates"]])  # [N, F]
    n = cands.shape[0]

    # --- weakness and prior -------------------------------------------------
    weak_vec = (100.0 - stats) / 100.0
    weakness = (cands * fw * weak_vec).sum(1) / (cands * fw).sum(1).clamp_min(1e-9)
    cdiff = torch.tensor(req["difficulty"], dtype=dt)
    prior = (cands * stats).sum(1) / cands.sum(1).clamp_min(1.0) - 0.5 * (cdiff - 50.0)

    # --- online ridge on the history ----------------------------------------
    hist = req.get("history", [])
    counts = torch.zeros(nfeat, dtype=dt)
    pred = prior.clone()
    if len(hist):
        hx = torch.stack([onehot(h["active"]) for h in hist])
        hy = torch.tensor([float(h["score"]) for h in hist], dtype=dt)
        hdiff = torch.tensor([float(h["difficulty"]) for h in hist], dtype=dt)
        hprior = (hx * stats).sum(1) / hx.sum(1).clamp_min(1.0) - 0.5 * (hdiff - 50.0)
        resid = hy - hprior
        counts = hx.sum(0)
        a = hx.T @ hx + lam * torch.eye(nfeat, dtype=dt)
        b = hx.T @ resid
        w = torch.linalg.solve(a, b)
        pred = (prior + cands @ w).clamp(0.0, 100.0)

    fit = (1.0 - (pred - target).abs() / 45.0).clamp_min(0.0)
    novelty = (cands * (1.0 / (1.0 + counts))).sum(1) / cands.sum(1).clamp_min(1.0)

    lo, hi = weakness.min(), weakness.max()
    wn = (weakness - lo) / (hi - lo + 1e-9)
    total = 0.40 * wn + 0.45 * fit + 0.15 * novelty

    last = req.get("last_active")
    if last is not None:
        lastx = onehot(last)
        same = ((cands - lastx).abs().sum(1) < 0.5)
        total = total - same.to(dt)

    k = min(top_k, n)
    vals, idx = torch.topk(total, k)
    top = []
    for v, i in zip(vals.tolist(), idx.tolist()):
        top.append({
            "index": i,
            "total": v,
            "weakness": float(weakness[i]),
            "fit": float(fit[i]),
            "novelty": float(novelty[i]),
            "predicted": float(pred[i]),
        })
    return {"engine": "PyTorch " + torch.__version__.split("+")[0], "best": top[0]["index"], "top": top}


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--probe":
        return probe()
    try:
        import torch  # noqa: F401
    except Exception as exc:
        sys.stderr.write("torch missing: %s\n" % exc)
        return 3
    req = json.loads(sys.stdin.read())
    sys.stdout.write(json.dumps(run(req)))
    sys.stdout.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main())
