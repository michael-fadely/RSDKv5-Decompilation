"""
RSDK mesh lighting baker.

It also simplifies the decoration meshes.

Usage:
  python modelbake.py ./Data.rsdk [options]

Reads Data.rsdk, decrypts MDL meshes, bakes lighting into vertex colors,
writes new archive. Requires rsdk_file_list.txt in same directory.

Options (positional):
  arg1: path to Data.rsdk
  arg2: ambient level 0.0-1.0     (default 0.35)
  arg3: diffuse strength 0.0-1.0  (default 0.55)
  arg4: spec strength 0.0-1.0     (default 0.25)
  arg5: spec power (exponent)     (default 2.5)
  arg6: light direction y,x,z     (default "0.85,0.15,0.1")
"""


import struct, os, sys, shutil, hashlib, math, fnmatch


# ============================================================
# SIMPLIFICATION HELPERS
# ============================================================

def simplify_mdl(mdl, enable_collapse=False):
    print("    Simplifying mesh...")

    faces = mdl['faces']
    verts = mdl['frames'][0]['verts']
    normals = mdl['frames'][0]['normals'] if mdl['has_normals'] else None

    # --------------------------------------------------------
    # Remove degenerate
    faces = [f for f in faces if len(set(f)) == 3]

    # --------------------------------------------------------
    # Remove duplicate
    seen = set()
    unique = []
    for f in faces:
        key = tuple(sorted(f))
        if key not in seen:
            seen.add(key)
            unique.append(f)
    faces = unique

    # --------------------------------------------------------
    # Remove tiny area
    def area(a,b,c):
        ax,ay,az = a
        bx,by,bz = b
        cx,cy,cz = c
        ux,uy,uz = bx-ax, by-ay, bz-az
        vx,vy,vz = cx-ax, cy-ay, cz-az
        nx = uy*vz - uz*vy
        ny = uz*vx - ux*vz
        nz = ux*vy - uy*vx
        return math.sqrt(nx*nx+ny*ny+nz*nz)*0.5

    clean = []
    for f in faces:
        if area(verts[f[0]], verts[f[1]], verts[f[2]]) > 1e-6:
            clean.append(f)
    faces = clean

    # --------------------------------------------------------
    # Remove inward faces
    if normals:
        cleaned = []
        for f in faces:
            v0,v1,v2 = [verts[i] for i in f]
            ux,uy,uz = v1[0]-v0[0], v1[1]-v0[1], v1[2]-v0[2]
            vx,vy,vz = v2[0]-v0[0], v2[1]-v0[1], v2[2]-v0[2]
            fn = (
                uy*vz - uz*vy,
                uz*vx - ux*vz,
                ux*vy - uy*vx
            )
            fl = math.sqrt(fn[0]**2+fn[1]**2+fn[2]**2) + 1e-8
            fn = (fn[0]/fl, fn[1]/fl, fn[2]/fl)

            n0,n1,n2 = [normals[i] for i in f]
            an = (
                (n0[0]+n1[0]+n2[0])/3.0,
                (n0[1]+n1[1]+n2[1])/3.0,
                (n0[2]+n1[2]+n2[2])/3.0
            )
            al = math.sqrt(an[0]**2+an[1]**2+an[2]**2)+1e-8
            an = (an[0]/al, an[1]/al, an[2]/al)

            dot = fn[0]*an[0]+fn[1]*an[1]+fn[2]*an[2]
            if dot > -0.5:
                cleaned.append(f)
        faces = cleaned

    # --------------------------------------------------------
    # Rebuild vertex list
    used = sorted(set(i for f in faces for i in f))
    remap = {old:i for i,old in enumerate(used)}

    mdl['faces'] = [[remap[i] for i in f] for f in faces]
    mdl['vert_count'] = len(used)

    for frame in mdl['frames']:
        frame['verts'] = [frame['verts'][i] for i in used]
        if mdl['has_normals']:
            frame['normals'] = [frame['normals'][i] for i in used]

    if mdl['uvs']:
        mdl['uvs'] = [mdl['uvs'][i] for i in used]
    if mdl['colors']:
        mdl['colors'] = [mdl['colors'][i] for i in used]

    orig_verts = mdl['vert_count']
    orig_faces = len(mdl['faces'])

    print(f"    BEFORE: {orig_verts} verts, {orig_faces} faces")
    print(f"    AFTER:  {len(used)} verts, {len(faces)} faces")
    print(f"    REMOVED: {orig_faces - len(faces)} faces, "
        f"{orig_verts - len(used)} verts")

    return mdl


def decimate_vertex_clustering(mdl, cell_size=1.0):
    print(f"    Running vertex clustering (cell {cell_size})")

    verts = mdl['frames'][0]['verts']
    faces = mdl['faces']

    grid = {}
    mapping = {}

    def cell_key(v):
        return (
            int(v[0] // cell_size),
            int(v[1] // cell_size),
            int(v[2] // cell_size)
        )

    new_index = 0
    new_verts = []
    new_normals = []
    new_uvs = []
    new_colors = []

    for i, v in enumerate(verts):
        key = cell_key(v)
        if key not in grid:
            grid[key] = new_index
            mapping[i] = new_index

            new_verts.append(v)

            if mdl['has_normals']:
                new_normals.append(mdl['frames'][0]['normals'][i])

            if mdl['uvs']:
                new_uvs.append(mdl['uvs'][i])

            if mdl['colors']:
                new_colors.append(mdl['colors'][i])

            new_index += 1
        else:
            mapping[i] = grid[key]

    # Remap faces
    new_faces = []
    for f in faces:
        nf = [mapping[i] for i in f]
        if len(set(nf)) == 3:
            new_faces.append(nf)

    print(f"    BEFORE: {mdl['vert_count']} verts, {len(faces)} faces")
    print(f"    AFTER:  {len(new_verts)} verts, {len(new_faces)} faces")

    mdl['faces'] = new_faces
    mdl['vert_count'] = len(new_verts)

    for frame in mdl['frames']:
        frame['verts'] = new_verts
        if mdl['has_normals']:
            frame['normals'] = new_normals

    if mdl['uvs']:
        mdl['uvs'] = new_uvs
    if mdl['colors']:
        mdl['colors'] = new_colors

    return mdl


def decimate_vertex_clustering_safe(mdl, cell_size=1.0):
    print(f"    Running SAFE multi-frame clustering (cell {cell_size})")

    frame_count = mdl['frame_count']
    vert_count = mdl['vert_count']
    faces = mdl['faces']

    # ---------------------------------------------------------
    # Compute average position across all frames
    avg_positions = []

    for vi in range(vert_count):
        sx = sy = sz = 0.0
        for frame in mdl['frames']:
            x, y, z = frame['verts'][vi]
            sx += x; sy += y; sz += z
        avg_positions.append((
            sx / frame_count,
            sy / frame_count,
            sz / frame_count
        ))

    # ---------------------------------------------------------
    # Cluster based on average position
    grid = {}
    mapping = {}
    new_index = 0

    def cell_key(v):
        return (
            int(v[0] // cell_size),
            int(v[1] // cell_size),
            int(v[2] // cell_size)
        )

    for i, pos in enumerate(avg_positions):
        key = cell_key(pos)
        if key not in grid:
            grid[key] = new_index
            mapping[i] = new_index
            new_index += 1
        else:
            mapping[i] = grid[key]

    # ---------------------------------------------------------
    # Remap faces
    new_faces = []
    for f in faces:
        nf = [mapping[i] for i in f]
        if len(set(nf)) == 3:
            new_faces.append(nf)

    print(f"    BEFORE: {vert_count} verts, {len(faces)} faces")
    print(f"    AFTER:  {new_index} verts, {len(new_faces)} faces")

    # ---------------------------------------------------------
    # Rebuild frames
    for frame in mdl['frames']:
        new_verts = [None] * new_index
        if mdl['has_normals']:
            new_normals = [None] * new_index

        for old_i, new_i in mapping.items():
            if new_verts[new_i] is None:
                new_verts[new_i] = frame['verts'][old_i]
                if mdl['has_normals']:
                    new_normals[new_i] = frame['normals'][old_i]

        frame['verts'] = new_verts
        if mdl['has_normals']:
            frame['normals'] = new_normals

    # ---------------------------------------------------------
    # Rebuild optional arrays
    if mdl['uvs']:
        new_uvs = [None] * new_index
        for old_i, new_i in mapping.items():
            if new_uvs[new_i] is None:
                new_uvs[new_i] = mdl['uvs'][old_i]
        mdl['uvs'] = new_uvs

    if mdl['colors']:
        new_colors = [None] * new_index
        for old_i, new_i in mapping.items():
            if new_colors[new_i] is None:
                new_colors[new_i] = mdl['colors'][old_i]
        mdl['colors'] = new_colors

    mdl['faces'] = new_faces
    mdl['vert_count'] = new_index

    return mdl


# =====================================================================
# RSDK Crypto
# =====================================================================

def swap_hash_endian(data):
    return struct.pack("<4I", *struct.unpack(">4I", data))


def rsdk_decrypt(data, filename):
    key1 = list(swap_hash_endian(hashlib.md5(filename.upper().encode()).digest()))
    key2 = list(swap_hash_endian(hashlib.md5(str(len(data)).encode()).digest()))
    key1_index = 0
    key2_index = 8
    swap_nibbles = 0
    xor_value = (len(data) >> 2) & 0x7f

    out = []
    for c in data:
        c ^= xor_value ^ key2[key2_index]
        if swap_nibbles:
            c = (c >> 4) | ((c & 0xf) << 4)
        c ^= key1[key1_index]
        out.append(c)

        key1_index += 1
        key2_index += 1
        if key1_index > 15 and key2_index > 8:
            xor_value += 2
            xor_value &= 0x7f
            if swap_nibbles:
                key1_index = xor_value % 7
                key2_index = (xor_value % 12) + 2
            else:
                key1_index = (xor_value % 12) + 3
                key2_index = xor_value % 7
            swap_nibbles ^= 1
        else:
            if key1_index > 15:
                swap_nibbles ^= 1
                key1_index = 0
            if key2_index > 12:
                swap_nibbles ^= 1
                key2_index = 0
    return bytes(out)


# =====================================================================
# RSDK Archive reader/writer
# =====================================================================

class RSDKArchive:
    def __init__(self, path):
        self.path = path
        self.entries = []
        with open(path, 'rb') as f:
            sig = f.read(4)
            ver = f.read(2)
            assert sig == b'RSDK', "Not an RSDK archive"
            count = struct.unpack('<H', f.read(2))[0]
            for _ in range(count):
                h = swap_hash_endian(f.read(16))
                offset, size_field = struct.unpack('<II', f.read(8))
                is_enc = bool(size_field >> 31)
                size = size_field & 0x7FFFFFFF
                self.entries.append({
                    'hash': h,
                    'offset': offset,
                    'size': size,
                    'encrypted': is_enc,
                })

    def read_entry_raw(self, idx):
        e = self.entries[idx]
        with open(self.path, 'rb') as f:
            f.seek(e['offset'])
            return f.read(e['size'])

    def read_entry(self, idx, filename=None):
        raw = self.read_entry_raw(idx)
        if self.entries[idx]['encrypted'] and filename:
            return rsdk_decrypt(raw, filename)
        return raw

    def write_new(self, output_path, replacements):
        count = len(self.entries)
        all_data = []
        all_enc = []
        for i in range(count):
            if i in replacements:
                all_data.append(replacements[i])
                all_enc.append(False)
            else:
                all_data.append(self.read_entry_raw(i))
                all_enc.append(self.entries[i]['encrypted'])

        with open(output_path, 'wb') as f:
            f.write(b'RSDKv5')
            f.write(struct.pack('<H', count))
            header_size = 8 + count * 0x18
            offset = header_size
            for i in range(count):
                f.write(swap_hash_endian(self.entries[i]['hash']))
                size_field = len(all_data[i]) | (int(all_enc[i]) << 31)
                f.write(struct.pack('<II', offset, size_field))
                offset += len(all_data[i])
            for d in all_data:
                f.write(d)
        return offset


# =====================================================================
# Filename list -> hash mapping
# =====================================================================

def load_file_list(list_path):
    hash_to_name = {}
    if not os.path.exists(list_path):
        return hash_to_name
    with open(list_path, 'r') as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#'):
                h = hashlib.md5(line.lower().encode()).digest()
                hash_to_name[h] = line
    return hash_to_name


# =====================================================================
# RSDK MDL mesh format
# =====================================================================

FLAG_HAS_NORMALS = 1
FLAG_HAS_UVS     = 2
FLAG_HAS_COLORS  = 4

def parse_mdl(data):
    if len(data) < 10 or data[:4] != b'MDL\x00':
        return None
    pos = 4
    flags = data[pos]; pos += 1
    fvc = data[pos]; pos += 1
    if fvc not in (3, 4):
        return None
    vert_count = struct.unpack_from('<H', data, pos)[0]; pos += 2
    frame_count = struct.unpack_from('<H', data, pos)[0]; pos += 2

    uvs = None
    if flags & FLAG_HAS_UVS:
        uvs = []
        for _ in range(vert_count):
            u, v = struct.unpack_from('<ff', data, pos); pos += 8
            uvs.append((u, v))

    colors = None
    if flags & FLAG_HAS_COLORS:
        colors = []
        for _ in range(vert_count):
            b, g, r, a = struct.unpack_from('BBBB', data, pos); pos += 4
            colors.append((r, g, b, a))

    fi_count = struct.unpack_from('<H', data, pos)[0]; pos += 2
    fi_flat = []
    for _ in range(fi_count):
        fi_flat.append(struct.unpack_from('<H', data, pos)[0]); pos += 2
    faces = [fi_flat[i:i+fvc] for i in range(0, len(fi_flat), fvc)]

    has_normals = bool(flags & FLAG_HAS_NORMALS)
    frames = []
    for _ in range(frame_count):
        verts, normals = [], []
        for __ in range(vert_count):
            x, y, z = struct.unpack_from('<fff', data, pos); pos += 12
            verts.append((x, y, z))
            if has_normals:
                nx, ny, nz = struct.unpack_from('<fff', data, pos); pos += 12
                normals.append((nx, ny, nz))
        frames.append({'verts': verts, 'normals': normals})

    return {
        'flags': flags, 'face_vert_count': fvc,
        'vert_count': vert_count, 'frame_count': frame_count,
        'uvs': uvs, 'colors': colors, 'faces': faces,
        'frames': frames, 'has_normals': has_normals,
    }


def build_mdl(mdl):
    p = [b'MDL\x00',
         struct.pack('BB', mdl['flags'], mdl['face_vert_count']),
         struct.pack('<HH', mdl['vert_count'], mdl['frame_count'])]
    if mdl['uvs'] is not None:
        for u, v in mdl['uvs']:
            p.append(struct.pack('<ff', u, v))
    if mdl['colors'] is not None:
        for r, g, b, a in mdl['colors']:
            p.append(struct.pack('BBBB', b, g, r, a))
    total_idx = sum(len(f) for f in mdl['faces'])
    p.append(struct.pack('<H', total_idx))
    for face in mdl['faces']:
        for idx in face:
            p.append(struct.pack('<H', idx))
    for frame in mdl['frames']:
        for i, (x, y, z) in enumerate(frame['verts']):
            p.append(struct.pack('<fff', x, y, z))
            if mdl['has_normals']:
                nx, ny, nz = frame['normals'][i]
                p.append(struct.pack('<fff', nx, ny, nz))
    return b''.join(p)


# =====================================================================
# Lighting
# =====================================================================

def normalize(x, y, z):
    mag = math.sqrt(x*x + y*y + z*z)
    if mag < 1e-7:
        return (0.0, 0.0, 0.0)
    return (x/mag, y/mag, z/mag)


def dot3(ax, ay, az, bx, by, bz):
    return ax*bx + ay*by + az*bz


def bake_lighting(mdl, is_itembox, light_dir, ambient, diffuse_str, spec_str, spec_power):
    """
    Bake per-vertex directional lighting into vertex colors.

    Lighting model (per vertex):
      - normalize the model normal
      - diffuse = max(0, dot(normal, light_dir)) * diffuse_str
      - specular = max(0, dot(normal, light_dir))^spec_power * spec_str
      - final brightness = ambient + diffuse + specular
      - multiply existing vertex color (or white) by brightness, clamp to 0-255

    This is nicer than what the engine does (which is Y-only, fixed point,
    with harsh sgn(n)*n^2 specular). We use a proper directional light
    with a tunable power curve, so you get a softer highlight and can
    point the light wherever you want.
    """
    if not mdl['has_normals']:
        print("    (no normals, skipping)")
        return mdl

    # average normals across all frames for a stable bake on animated meshes
    avg_normals = []
    nframes = len(mdl['frames'])
    for vi in range(mdl['vert_count']):
        sx, sy, sz = 0.0, 0.0, 0.0
        for frame in mdl['frames']:
            if frame['normals']:
                nx, ny, nz = frame['normals'][vi]
                sx += nx; sy += ny; sz += nz
        sx /= nframes; sy /= nframes; sz /= nframes
        avg_normals.append(normalize(sx, sy, sz))

    lx, ly, lz = normalize(*light_dir)
    orig_colors = mdl['colors']
    colors = []

    for i in range(mdl['vert_count']):
        nx, ny, nz = avg_normals[i]
        if is_itembox:
            nx = 1
            nz = 1
            lx = 0
            lz = 0
        ndotl = dot3(nx, ny, nz, lx, ly, lz)

        if is_itembox:
            if (ndotl < 0):
                ndotl = -ndotl

        diffuse = max(0.0, ndotl) * diffuse_str

        # wrap the diffuse slightly so the back side isn't pure ambient
        # this softens the terminator line
        wrap = 0.15
        diffuse_wrapped = max(0.0, (ndotl + wrap) / (1.0 + wrap)) * diffuse_str

        specular = (max(0.0, ndotl) ** spec_power) * spec_str

        brightness = ambient + diffuse_wrapped + specular

        if orig_colors:
            r0, g0, b0, a0 = orig_colors[i]
        else:
            if is_itembox:
                r0, g0, b0, a0 = 255, 255, 64, 255
            else:
                r0, g0, b0, a0 = 255, 255, 255, 255

        r = max(0, min(255, int(r0 * brightness)))
        if is_itembox:
            if (r > 192):
                r = 255
            elif (r < 160):
                r = 160
        g = max(0, min(255, int(g0 * brightness)))
        if is_itembox:
            if (g > 192):
                g = 255
            elif (g < 160):
                g = 160
        b = max(0, min(255, int(b0 * brightness)))
        colors.append((r, g, b, a0))

    mdl['colors'] = colors
    mdl['flags'] |= FLAG_HAS_COLORS
    return mdl


# =====================================================================
# Main
# =====================================================================

def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return

    rsdk_path = os.path.abspath(args[0])
    ambient      = float(args[1]) if len(args) > 1 else 0.5
    diffuse_str  = float(args[2]) if len(args) > 2 else 0.7
    spec_str     = float(args[3]) if len(args) > 3 else 0.7
    spec_power   = float(args[4]) if len(args) > 4 else 1.5

    if len(args) > 5:
        light_dir = tuple(float(x) for x in args[5].split(','))
    else:
        # mostly from above, slight forward and side tilt
        # gives a nice "sunlit from above-right" look
        light_dir = (0.15, 0.85, 0.1)

    # models with fewer verts than this are skipped
    min_verts = int(args[6]) if len(args) > 6 else 0

    # paths containing these strings are skipped
    skip_patterns = ["Shadow.bin", "OrbNet.bin", "ChuteHoop.bin", "Count0.bin", "Count1.bin", "Count2.bin",
                     "Count3.bin", "Count4.bin", "Count5.bin", "Count6.bin", "Count7.bin", "Count8.bin", "Count9.bin", "DDWBall.bin", "DDWEyes.bin", "DDWStache.bin",
                     "DDWStripe1.bin", "DDWStripe2.bin", "SpecialRing.bin"]

    if not os.path.exists(rsdk_path):
        print(f"ERROR: {rsdk_path} not found")
        return

    script_dir = os.path.dirname(os.path.abspath(__file__))

    print(f"Lighting params:")
    print(f"  ambient:  {ambient}")
    print(f"  diffuse:  {diffuse_str}")
    print(f"  specular: {spec_str} (power {spec_power})")
    print(f"  light:    {light_dir}")
    print()

    # Backup
    backup_path = rsdk_path + ".bak"
    if not os.path.exists(backup_path):
        shutil.copy2(rsdk_path, backup_path)
        print(f"Backed up to {backup_path}")
    else:
        print(f"Using existing backup {backup_path}")

    src_path = backup_path if os.path.exists(backup_path) else rsdk_path

    # Load filename list
    list_path = os.path.join(script_dir, "rsdk_file_list.txt")
    print(f"Loading file list from {list_path}")
    hash_to_name = load_file_list(list_path)
    print(f"  {len(hash_to_name)} known filenames")

    # Find mesh filenames
    mesh_names = {h: n for h, n in hash_to_name.items()
                  if '/Meshes/' in n or '\\Meshes\\' in n}
    print(f"  {len(mesh_names)} mesh files identified")

    if not mesh_names:
        print("ERROR: No mesh filenames found in file list!")
        print("  Make sure rsdk_file_list.txt contains paths like "
              "Data/Meshes/Special/SonicBall.bin")
        return

    print(f"\n=== Reading {src_path} ===")
    archive = RSDKArchive(src_path)
    print(f"Archive has {len(archive.entries)} files")

    # Decorate entries with known names
    entry_names = {}
    for i, e in enumerate(archive.entries):
        if e['hash'] in hash_to_name:
            entry_names[i] = hash_to_name[e['hash']]

    # Bake lighting into meshes
    replacements = {}
    mesh_count = 0
    skipped_no_normals = 0

    for i, e in enumerate(archive.entries):
        if e['hash'] not in mesh_names:
            continue

        fname = mesh_names[e['hash']]

        if "Pinball" in fname:
            continue

        if any(pat in fname for pat in skip_patterns):
            print(f"  SKIP (excluded): {fname}")
            continue

        data = archive.read_entry(i, fname)
        mdl = parse_mdl(data)
        if mdl is None:
            print(f"  SKIP (not valid MDL): {fname}")
            continue

        if not mdl['has_normals']:
            print(f"  SKIP (no normals): {fname}")
            skipped_no_normals += 1
            continue

        if mdl['vert_count'] < min_verts:
            print(f"  SKIP ({mdl['vert_count']}v < {min_verts}): {fname}")
            continue


        had_colors = bool(mdl['colors'])

        if "Emerald" not in fname and fnmatch.fnmatch(fname, "*Meshes/Decoration/*.bin"):
            f_cell_size = 0.5
            mdl = decimate_vertex_clustering_safe(mdl, cell_size=f_cell_size)

        bake_lighting(mdl, ("ItemBox" in fname), light_dir, ambient, diffuse_str, spec_str, spec_power)

        new_data = build_mdl(mdl)

        replacements[i] = new_data
        mesh_count += 1

        color_note = " (had colors)" if had_colors else " (added colors)"
        anim_note = f" ({mdl['frame_count']} frames)" if mdl['frame_count'] > 1 else ""
        size_delta = len(new_data) - len(data)
        print(f"  {fname}: {mdl['vert_count']}v{anim_note}{color_note} "
              f"[{len(data)}→{len(new_data)} bytes, {size_delta:+d}]")

    print(f"\n=== Summary ===")
    print(f"  Baked: {mesh_count} meshes")
    print(f"  Skipped (no normals): {skipped_no_normals}")
    if not replacements:
        print("  Nothing to write.")
        return

    print(f"\n=== Writing {rsdk_path} ===")
    total_size = archive.write_new(rsdk_path, replacements)
    print(f"  Written: {total_size} bytes")
    print("=== Done ===")


if __name__ == "__main__":
    main()
