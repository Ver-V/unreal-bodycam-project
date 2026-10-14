"""
Repair the Fab G17 glb so Unreal imports the pistol:
- gun parts are rigid meshes on plain nodes -> make those nodes joints and skin each part 100% to its parent node
- merge everything into one skin (one skeleton)
- add a Muzzle joint at the front of the slide
- rebase every animation to start at t=0, and rebuild Idle from Fire's first frame

Usage: python fix_glock.py <glock17.glb from Fab> <output.glb>
"""
import json, struct, sys, math, array

src, dst = sys.argv[1], sys.argv[2]
data = open(src, 'rb').read()
jlen = struct.unpack('<I', data[12:16])[0]
J = json.loads(data[20:20 + jlen])
bin_off = 20 + jlen
blen = struct.unpack('<I', data[bin_off:bin_off + 4])[0]
BIN = bytearray(data[bin_off + 8: bin_off + 8 + blen])
N = J['nodes']
ACC = J['accessors']
BV = J['bufferViews']

# ---------- matrix helpers (column-major 4x4 as flat lists, glTF style) ----------
def mmul(a, b):
    r = [0.0] * 16
    for c in range(4):
        for rr in range(4):
            r[c * 4 + rr] = sum(a[k * 4 + rr] * b[c * 4 + k] for k in range(4))
    return r

def ident():
    return [1.0 if i % 5 == 0 else 0.0 for i in range(16)]

def trs(n):
    if 'matrix' in n:
        return list(n['matrix'])
    t = n.get('translation', [0, 0, 0]); q = n.get('rotation', [0, 0, 0, 1]); s = n.get('scale', [1, 1, 1])
    x, y, z, w = q
    R = [1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w),
         2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w),
         2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y)]
    return [R[0] * s[0], R[1] * s[0], R[2] * s[0], 0,
            R[3] * s[1], R[4] * s[1], R[5] * s[1], 0,
            R[6] * s[2], R[7] * s[2], R[8] * s[2], 0,
            t[0], t[1], t[2], 1]

def inv(m):
    # general 4x4 inverse
    a = [[m[c * 4 + r] for c in range(4)] for r in range(4)]
    n = 4; b = [[1.0 if i == j else 0.0 for j in range(n)] for i in range(n)]
    for i in range(n):
        p = max(range(i, n), key=lambda r: abs(a[r][i]))
        a[i], a[p] = a[p], a[i]; b[i], b[p] = b[p], b[i]
        d = a[i][i]
        a[i] = [v / d for v in a[i]]; b[i] = [v / d for v in b[i]]
        for r in range(n):
            if r != i:
                f = a[r][i]
                a[r] = [x - f * y for x, y in zip(a[r], a[i])]
                b[r] = [x - f * y for x, y in zip(b[r], b[i])]
    return [b[r][c] for c in range(4) for r in range(4)]

def xform_p(m, v):
    return [m[0] * v[0] + m[4] * v[1] + m[8] * v[2] + m[12],
            m[1] * v[0] + m[5] * v[1] + m[9] * v[2] + m[13],
            m[2] * v[0] + m[6] * v[1] + m[10] * v[2] + m[14]]

def xform_n(m, v):
    r = [m[0] * v[0] + m[4] * v[1] + m[8] * v[2],
         m[1] * v[0] + m[5] * v[1] + m[9] * v[2],
         m[2] * v[0] + m[6] * v[1] + m[10] * v[2]]
    l = math.sqrt(sum(c * c for c in r)) or 1.0
    return [c / l for c in r]

parent = {}
for i, n in enumerate(N):
    for c in n.get('children', []):
        parent[c] = i

def gmat(i):
    m = trs(N[i])
    while i in parent:
        i = parent[i]
        m = mmul(trs(N[i]), m)
    return m

def name_idx(name):
    return next(i for i, n in enumerate(N) if n.get('name') == name)

# ---------- buffer helpers ----------
COMP = {5126: ('f', 4), 5123: ('H', 2), 5121: ('B', 1), 5125: ('I', 4)}
NCOMP = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}

def read_acc(ai):
    a = ACC[ai]; bv = BV[a['bufferView']]
    fmt, size = COMP[a['componentType']]; nc = NCOMP[a['type']]
    stride = bv.get('byteStride', size * nc)
    base = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    out = []
    for k in range(a['count']):
        o = base + k * stride
        out.append(list(struct.unpack_from('<' + fmt * nc, BIN, o)))
    return out

def add_acc(rows, ctype, atype, minmax=False):
    global BIN
    while len(BIN) % 4:
        BIN.append(0)
    fmt, size = COMP[ctype]; nc = NCOMP[atype]
    off = len(BIN)
    for r in rows:
        BIN += struct.pack('<' + fmt * nc, *r)
    BV.append({'buffer': 0, 'byteOffset': off, 'byteLength': len(BIN) - off})
    acc = {'bufferView': len(BV) - 1, 'componentType': ctype, 'count': len(rows), 'type': atype}
    if minmax:
        acc['min'] = [min(r[c] for r in rows) for c in range(nc)]
        acc['max'] = [max(r[c] for r in rows) for c in range(nc)]
    ACC.append(acc)
    return len(ACC) - 1

# ---------- build the joint list ----------
root = name_idx('Glock17_Root_01')
mesh_nodes = {i for i, n in enumerate(N) if 'mesh' in n}
camera_like = {i for i, n in enumerate(N) if 'camera' in n}

joints = []
def dfs(i):
    if i in mesh_nodes or i in camera_like:
        return
    joints.append(i)
    for c in N[i].get('children', []):
        dfs(c)
dfs(root)

# Muzzle joint: front-center of the slide, under the gun body so it doesn't move with the slide
main_j = name_idx('Main_j_050'); slide_j = name_idx('Slide_j_00')
slide_mesh_node = name_idx('Slide_Glock_D_0')
slide_mesh = N[slide_mesh_node]['mesh']
pos = read_acc(J['meshes'][slide_mesh]['primitives'][0]['attributes']['POSITION'])
to_main = mmul(inv(gmat(main_j)), gmat(slide_mesh_node))
pts = [xform_p(to_main, p) for p in pos]
# Barrel axis: the trigger sits in front of the magazine/grip, so "forward" is mag -> trigger along the slide axis.
# (The slide keyframes can't be trusted for this: the rest pose may already be mid-cycle.)
fire = next(a for a in J['animations'] if a['name'] == 'Fire')
to_main_node = lambda name: xform_p(mmul(inv(gmat(main_j)), gmat(name_idx(name))), [0, 0, 0])
z_sign = 1.0 if to_main_node('Trigger_j_058')[2] > to_main_node('Mag1_051')[2] else -1.0
axis = [0.0, 0.0, z_sign]
proj = [sum(p[k] * axis[k] for k in range(3)) for p in pts]
front = max(proj); span = front - min(proj)
tip = [p for p, d in zip(pts, proj) if d > front - span * 0.03]
muzzle_t = [sum(p[k] for p in tip) / len(tip) for k in range(3)]
N.append({'name': 'Muzzle', 'translation': muzzle_t})
muzzle = len(N) - 1
N[main_j].setdefault('children', []).append(muzzle)
parent[muzzle] = main_j
joints.append(muzzle)
print('barrel axis (Main_j space):', [round(c, 3) for c in axis], 'muzzle at', [round(c, 3) for c in muzzle_t])

jindex = {j: k for k, j in enumerate(joints)}

# ---------- bind space: X maps bind-vertex space to world, taken from an existing hand joint ----------
old_ibm = {}
for s in J['skins']:
    ibms = read_acc(s['inverseBindMatrices'])
    for j, m in zip(s['joints'], ibms):
        old_ibm.setdefault(j, m)
ref = name_idx('RightHand_06')
X = mmul(gmat(ref), old_ibm[ref])
# sanity: X should be the same for every hand joint
err = max(abs(a - b) for j in old_ibm for a, b in zip(mmul(gmat(j), old_ibm[j]), X))
print('hand bind consistency error:', err)
Xinv = inv(X)

ibm_rows = []
for j in joints:
    ibm_rows.append(old_ibm[j] if j in old_ibm else mmul(inv(gmat(j)), X))
ibm_acc = add_acc(ibm_rows, 5126, 'MAT4')

# ---------- remap hand meshes ----------
for si, s in enumerate(J['skins']):
    for n in N:
        if n.get('skin') == si and 'mesh' in n:
            for prim in J['meshes'][n['mesh']]['primitives']:
                rows = read_acc(prim['attributes']['JOINTS_0'])
                rows = [[jindex[s['joints'][k]] for k in r] for r in rows]
                prim['attributes']['JOINTS_0'] = add_acc(rows, 5123, 'VEC4')
            n['_newskin'] = True

# ---------- skin gun meshes rigidly to their parent node ----------
holder = parent[name_idx('Object_6')]  # same parent as the hand meshes
for mi in sorted(mesh_nodes):
    n = N[mi]
    if n.get('skin') is not None or '_newskin' in n:
        continue
    bone = parent[mi]
    while bone not in jindex:
        bone = parent[bone]
    to_bind = mmul(Xinv, gmat(mi))
    for prim in J['meshes'][n['mesh']]['primitives']:
        at = prim['attributes']
        p = [xform_p(to_bind, v) for v in read_acc(at['POSITION'])]
        at['POSITION'] = add_acc(p, 5126, 'VEC3', minmax=True)
        if 'NORMAL' in at:
            at['NORMAL'] = add_acc([xform_n(to_bind, v) for v in read_acc(at['NORMAL'])], 5126, 'VEC3')
        if 'TANGENT' in at:
            at['TANGENT'] = add_acc([xform_n(to_bind, v[:3]) + [v[3]] for v in read_acc(at['TANGENT'])], 5126, 'VEC4')
        cnt = len(p)
        at['JOINTS_0'] = add_acc([[jindex[bone], 0, 0, 0]] * cnt, 5123, 'VEC4')
        at['WEIGHTS_0'] = add_acc([[1.0, 0.0, 0.0, 0.0]] * cnt, 5126, 'VEC4')
    # move next to the hand meshes so every skinned mesh shares the same node context
    N[parent[mi]]['children'].remove(mi)
    N[holder]['children'].append(mi)
    n['_newskin'] = True

for n in N:
    if n.pop('_newskin', None):
        n['skin'] = 0
J['skins'] = [{'name': 'glock17_skin', 'joints': joints, 'inverseBindMatrices': ibm_acc, 'skeleton': root}]

# ---------- animations: rebase to t=0, rebuild Idle ----------
new_anims = []
for a in J['animations']:
    if a['name'] == 'Idle':
        continue
    t0 = min(ACC[s['input']]['min'][0] for s in a['samplers'])
    for s in a['samplers']:
        t = [[r[0] - t0] for r in read_acc(s['input'])]
        s['input'] = add_acc(t, 5126, 'SCALAR', minmax=True)
    new_anims.append(a)
    print('anim %-12s %.2fs' % (a['name'], max(ACC[s['input']]['max'][0] for s in a['samplers'])))

idle = {'name': 'Idle', 'samplers': [], 'channels': []}
times = add_acc([[0.0], [1.0]], 5126, 'SCALAR', minmax=True)
for c in fire['channels']:
    s = fire['samplers'][c['sampler']]
    first = read_acc(s['output'])[0]
    out = add_acc([first, first], 5126, ACC[s['output']]['type'])
    idle['samplers'].append({'input': times, 'output': out, 'interpolation': 'LINEAR'})
    idle['channels'].append({'sampler': len(idle['samplers']) - 1, 'target': dict(c['target'])})
new_anims.append(idle)
J['animations'] = new_anims
print('anim Idle         1.00s (Fire first frame)')

# ---------- write ----------
J['buffers'][0]['byteLength'] = len(BIN)
js = json.dumps(J, separators=(',', ':')).encode()
while len(js) % 4:
    js += b' '
while len(BIN) % 4:
    BIN.append(0)
total = 12 + 8 + len(js) + 8 + len(BIN)
with open(dst, 'wb') as f:
    f.write(struct.pack('<III', 0x46546C67, 2, total))
    f.write(struct.pack('<II', len(js), 0x4E4F534A)); f.write(js)
    f.write(struct.pack('<II', len(BIN), 0x004E4942)); f.write(BIN)
print('joints:', len(joints), '->', dst)
