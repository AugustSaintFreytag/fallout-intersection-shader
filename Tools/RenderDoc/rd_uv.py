# Reads vertex positions and TEXCOORD0 of terrain draws and fits uv = A * pos.xy + b (capture 02: uv = model.xy / 256, + 8 on negative quadrants).
# Runs inside qrenderdoc: $env:RD_CAPTURE, $env:RD_OUT, $env:RD_EIDS = "<comma-separated event ids>"; qrenderdoc.exe --python rd_uv.py
import os, struct, traceback
import renderdoc as rd

CAPTURE = os.environ["RD_CAPTURE"]
OUT = os.environ["RD_OUT"]
EIDS = [int(x) for x in os.environ["RD_EIDS"].split(",")]
log = open(OUT, "w", encoding="utf-8")

def L(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()

def solve3(m, v):
    # Cramer's rule for a 3x3 system.
    def det(a):
        return (a[0][0]*(a[1][1]*a[2][2]-a[1][2]*a[2][1]) - a[0][1]*(a[1][0]*a[2][2]-a[1][2]*a[2][0]) + a[0][2]*(a[1][0]*a[2][1]-a[1][1]*a[2][0]))
    d = det(m)
    res = []
    for c in range(3):
        mc = [row[:] for row in m]
        for r in range(3):
            mc[r][c] = v[r]
        res.append(det(mc) / d)
    return res

def fit(xs, ys, ts):
    # Least squares t = a*x + b*y + c.
    n = len(xs)
    sxx = sum(x*x for x in xs); syy = sum(y*y for y in ys); sxy = sum(x*y for x, y in zip(xs, ys))
    sx = sum(xs); sy = sum(ys)
    stx = sum(t*x for t, x in zip(ts, xs)); sty = sum(t*y for t, y in zip(ts, ys)); st = sum(ts)
    a, b, c = solve3([[sxx, sxy, sx], [sxy, syy, sy], [sx, sy, n]], [stx, sty, st])
    resid = max(abs(a*x + b*y + c - t) for x, y, t in zip(xs, ys, ts))
    return a, b, c, resid

def main():
    cap = rd.OpenCaptureFile()
    cap.OpenFile(CAPTURE, "", None)
    res, ctrl = cap.OpenCapture(rd.ReplayOptions(), None)
    L("open", res)
    actions = {}
    def walk(lst):
        for a in lst:
            actions[a.eventId] = a
            walk(a.children)
    walk(ctrl.GetRootActions())
    for eid in EIDS:
        ctrl.SetFrameEvent(eid, True)
        pipe = ctrl.GetPipelineState()
        vbs = pipe.GetVBuffers()
        attrs = pipe.GetVertexInputs()
        ib = pipe.GetIBuffer()
        act = actions.get(eid)
        L("eid", eid, "attrs", [(a.name, a.vertexBuffer, a.byteOffset, a.format.Name()) for a in attrs])
        L("  vbs", [(int(v.resourceId), v.byteOffset, v.byteStride) for v in vbs])
        L("  ib", int(ib.resourceId), ib.byteOffset, ib.byteStride, "action", act.indexOffset if act else None, act.baseVertex if act else None, act.numIndices if act else None)

        # Indices used by the draw.
        idata = ctrl.GetBufferData(ib.resourceId, ib.byteOffset + (act.indexOffset * ib.byteStride if act else 0), (act.numIndices if act else 0) * ib.byteStride)
        fmt = "<%d%s" % (len(idata) // ib.byteStride, "H" if ib.byteStride == 2 else "I")
        indices = sorted(set(struct.unpack(fmt, idata)))
        base = act.baseVertex if act else 0
        L("  unique vertices", len(indices), "range", indices[0], indices[-1])

        def read(attr, vi):
            vb = vbs[attr.vertexBuffer]
            off = vb.byteOffset + (vi + base) * vb.byteStride + attr.byteOffset
            n = attr.format.compCount
            return struct.unpack("<%df" % n, ctrl.GetBufferData(vb.resourceId, off, 4 * n))

        pa = [a for a in attrs if "position" in a.name.lower()][0]
        ta = [a for a in attrs if "texcoord0" in a.name.lower()][0]
        pick = indices[:: max(1, len(indices) // 60)]
        pts = [(read(pa, i), read(ta, i)) for i in pick]
        for p, t in pts[:6]:
            L("   pos", [round(x, 2) for x in p], "uv", [round(x, 5) for x in t])
        xs = [p[0] for p, t in pts]; ys = [p[1] for p, t in pts]
        L("  pos x range", round(min(xs), 1), round(max(xs), 1), "y range", round(min(ys), 1), round(max(ys), 1))
        for k in range(2):
            a, b, c, r = fit(xs, ys, [t[k] for p, t in pts])
            L("  uv%d = %.8f*x + %.8f*y + %.6f, max residual %.6g" % (k, a, b, c, r))

        # World placement: the VS constant ModelViewProj alone does not give it, so log the model matrix if reflected.
    L("done")

try:
    main()
except Exception:
    L(traceback.format_exc())
log.close()
import sys
sys.exit(0)
