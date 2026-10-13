# Prints the draw sequence of an event range from an rd_dump.py output, run-length grouped by shader pair and state.
# Usage: python draw_runs.py capture.jsonl <first eid> <last eid>
import json,sys
recs=[json.loads(l) for l in open(sys.argv[1])]
lo,hi=int(sys.argv[2]),int(sys.argv[3])
cur=None; runs=[]
for r in recs:
    if 'SetMarker' in r['flags'] and lo<=r['eid']<=hi: cur=r['name'].strip()
    if 'ds' not in r or not (lo<=r['eid']<=hi): continue
    d=r['ds']
    key=(cur, len(r['pstex']), tuple(n for n,_ in r['vin']),
         f"z{'T' if d['zTest'] else '-'}{'W' if d['zWrite'] else '-'} {d['zFunc']}",
         f"st:{d['sFunc']}/{d['sPass']} ref{d['sRef']} m{d['sMask']} w{d['sWrite']}" if d['stencil'] else "st:off",
         "blend" if r['blend'] and r['blend'][0]['on'] else "")
    if runs and runs[-1][0]==key: runs[-1][2]=r['eid']; runs[-1][3]+=1
    else: runs.append([key,r['eid'],r['eid'],1])
for k,a,b,n in runs:
    sh,nt,vin,z,st,bl=k
    print(f"{a:5}-{b:5} n={n:4} {sh:28} tex={nt:2} {z:16} {st:34} {bl:5} vin={','.join(v.replace('in_','') for v in vin)}")
