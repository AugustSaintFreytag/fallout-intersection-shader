# Probes one draw of a RenderDoc capture: bound pixel shader textures with their binding slots, the pixel shader's
# constants, an 8x8 grid of the bound 1024x1024 texture (the height field), and the draw's output and input depth at
# a few screen positions. Written for the ground pass. Adjust the positions and texture picks for other draws.
# Runs inside qrenderdoc (embedded Python):
#   $env:RD_CAPTURE = "<capture.rdc>"; $env:RD_OUT = "<out.txt>"; $env:RD_EID = "<event id>"
#   & "C:\Program Files\RenderDoc\qrenderdoc.exe" --python rd_probe.py
import os, traceback
import renderdoc as rd

CAPTURE = os.environ["RD_CAPTURE"]
OUT = os.environ["RD_OUT"]
EID = int(os.environ.get("RD_EID", "5934"))
log = open(OUT, "w", encoding="utf-8")

def L(*a):
    log.write(" ".join(str(x) for x in a) + "\n"); log.flush()

def pick(ctrl, rid, x, y):
    v = ctrl.PickPixel(rid, x, y, rd.Subresource(0, 0, 0), rd.CompType.Typeless)
    return tuple(round(f, 6) for f in v.floatValue)

def dumpvars(vars, indent=""):
    for v in vars:
        if v.members:
            L(indent + v.name)
            dumpvars(v.members, indent + "  ")
        else:
            vals = [v.value.f32v[i] for i in range(v.rows * v.columns)]
            L(indent + v.name, [round(x, 6) for x in vals])

def main():
    cap = rd.OpenCaptureFile()
    cap.OpenFile(CAPTURE, "", None)
    res, ctrl = cap.OpenCapture(rd.ReplayOptions(), None)
    L("open", res)
    ctrl.SetFrameEvent(EID, True)
    pipe = ctrl.GetPipelineState()

    texinfo = {int(t.resourceId): t for t in ctrl.GetTextures()}
    ro = pipe.GetReadOnlyResources(rd.ShaderStage.Pixel, True)
    for u in ro:
        rid = u.descriptor.resource
        t = texinfo.get(int(rid))
        L("PS tex", int(rid), t.width if t else None, t.height if t else None, t.format.Name() if t else None,
          "binding", u.access.index, "array", u.access.arrayElement, "type", u.access.type, "stage", u.access.stage)
    refl0 = pipe.GetShaderReflection(rd.ShaderStage.Pixel)
    for i, r in enumerate(refl0.readOnlyResources):
        L("reflection ro", i, r.name, "fixedBindNumber", r.fixedBindNumber, "fixedBindSetOrSpace", r.fixedBindSetOrSpace)

    # Constants
    stage = rd.ShaderStage.Pixel
    refl = pipe.GetShaderReflection(stage)
    for i, cb in enumerate(refl.constantBlocks):
        bind = pipe.GetConstantBlock(stage, i, 0)
        vars = ctrl.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(), refl.resourceId, stage, pipe.GetShaderEntryPoint(stage), i,
                                               bind.descriptor.resource, bind.descriptor.byteOffset, bind.descriptor.byteSize)
        L("cbuffer", i, cb.name)
        dumpvars(vars, "  ")

    # Height Field Contents on a Grid
    textures = [u.descriptor.resource for u in ro]
    field = [r for r in textures if texinfo.get(int(r)) and texinfo[int(r)].width == 1024]
    if field:
        f = field[0]
        L("height field grid (8x8):")
        nonzero = 0
        for gy in range(8):
            row = []
            for gx in range(8):
                v = pick(ctrl, f, gx * 128 + 64, gy * 128 + 64)
                row.append(v[0])
                nonzero += v[0] != 0
            L("  ", row)
        L("nonzero samples", nonzero)

    # Ground Pass Inputs and Output at a Few Screen Positions
    out = pipe.GetOutputTargets()[0].resource
    depth = [r for r in textures if texinfo.get(int(r)) and texinfo[int(r)].width != 1024]
    ctrl.SetFrameEvent(EID + 1, True)
    for (x, y) in [(720, 850), (720, 700), (300, 800), (1100, 800), (720, 450), (720, 600)]:
        o = pick(ctrl, out, x, y)
        d = pick(ctrl, depth[0], x, y) if depth else None
        L("screen", x, y, "depth", d, "ground", o)

    ctrl.Shutdown()
    cap.Shutdown()

try:
    main()
except Exception:
    L(traceback.format_exc())
finally:
    log.close()
    os._exit(0)
