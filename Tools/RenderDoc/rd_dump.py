# Runs inside qrenderdoc (embedded Python 3.8): qrenderdoc.exe --python rd_dump.py
# Dumps every action of the capture with the pipeline state we care about to JSON lines.
import os, sys, json, traceback
import renderdoc as rd

CAPTURE = os.environ["RD_CAPTURE"]
OUT = os.environ["RD_OUT"]
LOG = OUT.replace(".jsonl", ".log")
log = open(LOG, "w", encoding="utf-8")

def L(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()

def enum_name(v):
    s = str(v)
    return s.split(".")[-1]

def safe(fn, default=None):
    try:
        return fn()
    except Exception as e:
        return default if default is not None else "ERR:" + repr(e)

def main():
    L("capture:", CAPTURE)
    cap = rd.OpenCaptureFile()
    res = cap.OpenFile(CAPTURE, "", None)
    if res != rd.ResultCode.Succeeded:
        L("open failed", res); return
    if not cap.LocalReplaySupport():
        L("no local replay support"); return
    res, ctrl = cap.OpenCapture(rd.ReplayOptions(), None)
    if res != rd.ResultCode.Succeeded:
        L("replay failed", res); return

    sfile = ctrl.GetStructuredFile()
    names = {}
    for r in ctrl.GetResources():
        names[int(r.resourceId)] = r.name
    texinfo = {}
    for t in ctrl.GetTextures():
        texinfo[int(t.resourceId)] = (t.width, t.height, t.format.Name(), int(t.msSamp))
    meta = {"textures": {str(k): v for k, v in texinfo.items()}, "names": {str(k): v for k, v in names.items()}}
    with open(OUT.replace(".jsonl", ".meta.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f)

    actions = []
    def walk(acts, depth):
        for a in acts:
            actions.append((a, depth))
            walk(a.children, depth + 1)
    walk(ctrl.GetRootActions(), 0)
    L("actions:", len(actions))

    out = open(OUT, "w", encoding="utf-8")
    first_err = True
    for a, depth in actions:
        rec = {
            "eid": a.eventId, "depth": depth,
            "name": safe(lambda: a.GetName(sfile)),
            "flags": str(a.flags),
            "numIndices": a.numIndices, "numInstances": a.numInstances,
            "outputs": [int(o) for o in a.outputs if int(o) != 0],
            "depthOut": int(a.depthOut),
        }
        is_draw = bool(a.flags & rd.ActionFlags.Drawcall)
        if is_draw:
            try:
                ctrl.SetFrameEvent(a.eventId, False)
                vk = ctrl.GetVulkanPipelineState()
                pipe = ctrl.GetPipelineState()
                ds = vk.depthStencil
                ff = ds.frontFace
                rec["ds"] = {
                    "zTest": ds.depthTestEnable, "zWrite": ds.depthWriteEnable,
                    "zFunc": enum_name(ds.depthFunction),
                    "stencil": ds.stencilTestEnable,
                    "sFunc": enum_name(ff.function), "sPass": enum_name(ff.passOperation),
                    "sFail": enum_name(ff.failOperation), "sZFail": enum_name(ff.depthFailOperation),
                    "sRef": ff.reference, "sMask": ff.compareMask, "sWrite": ff.writeMask,
                }
                blends = vk.colorBlend.blends
                rec["blend"] = [{"on": b.enabled, "wm": b.writeMask,
                                 "src": enum_name(b.colorBlend.source), "dst": enum_name(b.colorBlend.destination)}
                                for b in blends[:2]]
                rec["vs"] = int(pipe.GetShader(rd.ShaderStage.Vertex))
                rec["ps"] = int(pipe.GetShader(rd.ShaderStage.Pixel))
                vin = pipe.GetVertexInputs()
                rec["vin"] = [(v.name, v.format.Name()) for v in vin if v.used] if vin else []
                ro = safe(lambda: pipe.GetReadOnlyResources(rd.ShaderStage.Pixel, True), [])
                texs = []
                for u in ro or []:
                    rid = int(u.descriptor.resource)
                    if rid:
                        texs.append(rid)
                rec["pstex"] = texs
                rec["rt"] = [int(t.resource) for t in pipe.GetOutputTargets() if int(t.resource)]
                rec["dsv"] = int(pipe.GetDepthTarget().resource)
            except Exception:
                if first_err:
                    L("state error at", a.eventId, traceback.format_exc())
                    first_err = False
                rec["err"] = True
        out.write(json.dumps(rec) + "\n")
    out.close()
    ctrl.Shutdown()
    cap.Shutdown()
    L("done")

try:
    main()
except Exception:
    L(traceback.format_exc())
finally:
    log.close()
    os._exit(0)
