#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <include/public_types.h>
#include <include/tri_stripper.h>

#define MDL_HAS_NORMALS  (1 << 0)
#define MDL_HAS_UVS      (1 << 1)
#define MDL_HAS_COLORS   (1 << 2)
#define MDL_IS_STRIPPED   (1 << 3)

struct Vec3f { float x, y, z; };
struct UV { float u, v; };
struct Color4 { uint8_t b, g, r, a; };

struct MdlVertex {
    float x, y, z;
    float nx, ny, nz;
};

struct MdlMesh {
    uint8_t flags;
    uint8_t faceVertCount;
    uint16_t vertCount;
    uint16_t frameCount;
    uint16_t indexCount;
    std::vector<UV> uvs;
    std::vector<Color4> colors;
    std::vector<uint16_t> indices;
    std::vector<std::vector<MdlVertex>> frames;
};

struct Vertex_Tristripped {
    float position[3];
    float normal[3];
    float texcoord[2];
    uint32_t color;
    uint32_t vertexId;
    uint32_t normalId;
};

struct Triangle {
    Vertex_Tristripped vertices[3];
    uint32_t materialId;
};

static std::vector<Triangle> g_triangles;
static std::vector<std::vector<uint32_t>> g_raw_strips;
static std::vector<uint32_t> g_loose_triangles;

// =====================================================================
// RSDK Crypto
// =====================================================================

static void md5(const uint8_t *data, size_t len, uint8_t out[16]) {
    uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;

    static const uint32_t s[] = {
        7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
        5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
        4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
        6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
    };
    static const uint32_t K[] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
    };

    uint64_t bitlen = len * 8;
    std::vector<uint8_t> msg(data, data + len);
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) msg.push_back(0);
    for (int i = 0; i < 8; i++) msg.push_back((bitlen >> (i * 8)) & 0xff);

    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        uint32_t M[16];
        for (int i = 0; i < 16; i++)
            M[i] = msg[chunk+i*4] | (msg[chunk+i*4+1]<<8) | (msg[chunk+i*4+2]<<16) | (msg[chunk+i*4+3]<<24);

        uint32_t A=a0, B=b0, C=c0, D=d0;
        for (int i = 0; i < 64; i++) {
            uint32_t F, g;
            if (i < 16)      { F = (B&C)|((~B)&D); g = i; }
            else if (i < 32) { F = (D&B)|((~D)&C); g = (5*i+1)%16; }
            else if (i < 48) { F = B^C^D;           g = (3*i+5)%16; }
            else              { F = C^(B|(~D));      g = (7*i)%16; }
            F += A + K[i] + M[g];
            A = D; D = C; C = B;
            B += (F << s[i]) | (F >> (32 - s[i]));
        }
        a0 += A; b0 += B; c0 += C; d0 += D;
    }

    for (int i = 0; i < 4; i++) { out[i]    = (a0>>(i*8))&0xff; }
    for (int i = 0; i < 4; i++) { out[4+i]  = (b0>>(i*8))&0xff; }
    for (int i = 0; i < 4; i++) { out[8+i]  = (c0>>(i*8))&0xff; }
    for (int i = 0; i < 4; i++) { out[12+i] = (d0>>(i*8))&0xff; }
}

static void swap_hash_endian(const uint8_t in[16], uint8_t out[16]) {
    for (int i = 0; i < 4; i++) {
        out[i*4+0] = in[i*4+3];
        out[i*4+1] = in[i*4+2];
        out[i*4+2] = in[i*4+1];
        out[i*4+3] = in[i*4+0];
    }
}

static void md5_str(const char *str, uint8_t out[16]) {
    md5((const uint8_t*)str, strlen(str), out);
}

static std::vector<uint8_t> rsdk_decrypt(const std::vector<uint8_t> &data, const char *filename) {
    // uppercase filename for key1
    std::string upper(filename);
    for (auto &c : upper) c = toupper(c);

    uint8_t hash1_raw[16], hash2_raw[16];
    md5_str(upper.c_str(), hash1_raw);
    std::string lenStr = std::to_string(data.size());
    md5_str(lenStr.c_str(), hash2_raw);

    uint8_t key1[16], key2[16];
    swap_hash_endian(hash1_raw, key1);
    swap_hash_endian(hash2_raw, key2);

    int key1_index = 0, key2_index = 8, swap_nibbles = 0;
    int xor_value = ((int)data.size() >> 2) & 0x7f;

    std::vector<uint8_t> out(data.size());
    for (size_t i = 0; i < data.size(); i++) {
        uint8_t c = data[i];
        c ^= xor_value ^ key2[key2_index];
        if (swap_nibbles)
            c = (c >> 4) | ((c & 0xf) << 4);
        c ^= key1[key1_index];
        out[i] = c;

        key1_index++;
        key2_index++;
        if (key1_index > 15 && key2_index > 8) {
            xor_value += 2;
            xor_value &= 0x7f;
            if (swap_nibbles) {
                key1_index = xor_value % 7;
                key2_index = (xor_value % 12) + 2;
            } else {
                key1_index = (xor_value % 12) + 3;
                key2_index = xor_value % 7;
            }
            swap_nibbles ^= 1;
        } else {
            if (key1_index > 15) { swap_nibbles ^= 1; key1_index = 0; }
            if (key2_index > 12) { swap_nibbles ^= 1; key2_index = 0; }
        }
    }
    return out;
}

// =====================================================================
// RSDK Archive
// =====================================================================

struct RSDKEntry {
    uint8_t hash[16];
    uint32_t offset;
    uint32_t size;
    bool encrypted;
};

struct RSDKArchive {
    std::vector<RSDKEntry> entries;
    std::vector<uint8_t> fileData;

    bool load(const char *path) {
        FILE *f = fopen(path, "rb");
        if (!f) return false;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        fileData.resize(sz);
        fread(fileData.data(), 1, sz, f);
        fclose(f);

        if (memcmp(fileData.data(), "RSDK", 4) != 0) return false;
        // skip sig(4) + ver(2)
        uint16_t count = fileData[6] | (fileData[7] << 8);

        size_t pos = 8;
        entries.resize(count);
        for (int i = 0; i < count; i++) {
            // hash is stored big-endian per u32, swap to match python
            uint8_t rawHash[16];
            memcpy(rawHash, &fileData[pos], 16);
            swap_hash_endian(rawHash, entries[i].hash);
            pos += 16;
            entries[i].offset = fileData[pos] | (fileData[pos+1]<<8) | (fileData[pos+2]<<16) | (fileData[pos+3]<<24);
            pos += 4;
            uint32_t sf = fileData[pos] | (fileData[pos+1]<<8) | (fileData[pos+2]<<16) | (fileData[pos+3]<<24);
            pos += 4;
            entries[i].encrypted = (sf >> 31) != 0;
            entries[i].size = sf & 0x7FFFFFFF;
        }
        return true;
    }

    std::vector<uint8_t> readEntry(int idx) {
        auto &e = entries[idx];
        return std::vector<uint8_t>(fileData.begin() + e.offset,
                                     fileData.begin() + e.offset + e.size);
    }

    std::vector<uint8_t> readEntryDecrypted(int idx, const char *filename) {
        auto raw = readEntry(idx);
        if (entries[idx].encrypted)
            return rsdk_decrypt(raw, filename);
        return raw;
    }

    bool save(const char *path, const std::map<int, std::vector<uint8_t>> &replacements) {
        FILE *f = fopen(path, "wb");
        if (!f) return false;

        uint16_t count = entries.size();
        fwrite("RSDKv5", 1, 6, f);
        fwrite(&count, 2, 1, f);

        uint32_t headerSize = 8 + count * 0x18;
        uint32_t offset = headerSize;

        // collect all data
        std::vector<std::vector<uint8_t>> allData(count);
        std::vector<bool> allEnc(count);
        for (int i = 0; i < count; i++) {
            auto it = replacements.find(i);
            if (it != replacements.end()) {
                allData[i] = it->second;
                allEnc[i] = false; // replacements are unencrypted
            } else {
                allData[i] = readEntry(i);
                allEnc[i] = entries[i].encrypted;
            }
        }

        // write header
        for (int i = 0; i < count; i++) {
            uint8_t beHash[16];
            swap_hash_endian(entries[i].hash, beHash);
            fwrite(beHash, 1, 16, f);
            fwrite(&offset, 4, 1, f);
            uint32_t sf = (uint32_t)allData[i].size() | ((uint32_t)allEnc[i] << 31);
            fwrite(&sf, 4, 1, f);
            offset += allData[i].size();
        }

        // write data
        for (int i = 0; i < count; i++)
            fwrite(allData[i].data(), 1, allData[i].size(), f);

        long total = ftell(f);
        fclose(f);
        printf("Written %ld bytes to %s\n", total, path);
        return true;
    }
};

// =====================================================================
// File list → hash map
// =====================================================================

static std::map<std::string, std::string> load_file_list(const char *path) {
    std::map<std::string, std::string> hashToName; // hex hash → filename
    FILE *f = fopen(path, "r");
    if (!f) return hashToName;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        size_t len = strlen(p);
        while (len > 0 && (p[len-1] == '\n' || p[len-1] == '\r')) p[--len] = 0;
        if (len == 0 || p[0] == '#') continue;

        // lowercase for hash
        std::string lower(p);
        for (auto &c : lower) c = tolower(c);

        uint8_t h[16];
        md5_str(lower.c_str(), h);

        char hex[33];
        for (int i = 0; i < 16; i++) sprintf(&hex[i*2], "%02x", h[i]);
        hashToName[hex] = std::string(p);
    }
    fclose(f);
    return hashToName;
}

static std::string hash_to_hex(const uint8_t h[16]) {
    char hex[33];
    for (int i = 0; i < 16; i++) sprintf(&hex[i*2], "%02x", h[i]);
    return hex;
}

// =====================================================================
// Strip joining
// =====================================================================

static bool can_join_strips(const std::vector<size_t> &a, const std::vector<size_t> &b) {
    if (a.size() < 3 || b.size() < 3) return false;
    return (a[a.size()-1] == b[1] && a[a.size()-2] == b[0]);
}

static std::vector<std::vector<size_t>>
join_strips(const triangle_stripper::primitive_vector &originalStrips) {
    std::vector<std::vector<size_t>> result;
    std::vector<bool> used(originalStrips.size(), false);
    std::vector<std::vector<size_t>> strips;

    for (const auto &prim : originalStrips)
        if (prim.Type == triangle_stripper::TRIANGLE_STRIP)
            strips.emplace_back(prim.Indices.begin(), prim.Indices.end());

    for (size_t i = 0; i < strips.size(); i++) {
        if (used[i]) continue;
        auto current = strips[i];
        used[i] = true;
        bool found;
        do {
            found = false;
            for (size_t j = 0; j < strips.size(); j++) {
                if (!used[j] && can_join_strips(current, strips[j])) {
                    current.insert(current.end(), strips[j].begin()+2, strips[j].end());
                    used[j] = true; found = true; break;
                }
            }
        } while (found);
        result.push_back(current);
    }
    return result;
}

// =====================================================================
// Stripifier
// =====================================================================

static void optimize_mesh() {
    if (g_triangles.empty()) return;
    using namespace triangle_stripper;

    indices Indices;
    for (const auto &tri : g_triangles)
        for (int i = 0; i < 3; i++)
            Indices.push_back(tri.vertices[i].vertexId);

    primitive_vector primitives;
    tri_stripper stripper(Indices);
    stripper.SetMinStripSize(0);
    stripper.SetCacheSize(0);
    stripper.SetBackwardSearch(true);
    stripper.SetPushCacheHits(true);
    stripper.Strip(&primitives);

    auto joined = join_strips(primitives);

    size_t stripTris = 0, looseTris = 0;
    for (const auto &prim : primitives)
        if (prim.Type == TRIANGLES) looseTris += prim.Indices.size() / 3;
    for (const auto &s : joined)
        if (s.size() >= 3) stripTris += s.size() - 2;

    g_raw_strips.clear();
    for (const auto &s : joined) {
        std::vector<uint32_t> raw;
        for (size_t idx : s) raw.push_back(idx);
        g_raw_strips.push_back(raw);
    }

    g_loose_triangles.clear();
    for (const auto &prim : primitives)
        if (prim.Type == TRIANGLES)
            for (size_t idx : prim.Indices)
                g_loose_triangles.push_back(idx);

    size_t totalStripIdx = 0;
    for (auto &s : g_raw_strips) totalStripIdx += s.size();

    printf("    strips:%zu loose:%zu ratio:%.0f%% idx:%zu\n",
           joined.size(), looseTris,
           100.0f * stripTris / std::max((size_t)1, stripTris + looseTris),
           totalStripIdx);
}

// =====================================================================
// MDL parse from buffer
// =====================================================================

static bool parse_mdl(const uint8_t *data, size_t dataLen, MdlMesh &m) {
    if (dataLen < 10) return false;
    if (memcmp(data, "MDL\x00", 4) != 0) return false;

    size_t pos = 4;
    m.flags = data[pos++];
    m.faceVertCount = data[pos++];
    if (m.faceVertCount != 3 && m.faceVertCount != 4) return false;

    memcpy(&m.vertCount, &data[pos], 2); pos += 2;
    memcpy(&m.frameCount, &data[pos], 2); pos += 2;

    if (m.flags & MDL_HAS_UVS) {
        m.uvs.resize(m.vertCount);
        for (int i = 0; i < m.vertCount; i++) {
            memcpy(&m.uvs[i].u, &data[pos], 4); pos += 4;
            memcpy(&m.uvs[i].v, &data[pos], 4); pos += 4;
        }
    }

    if (m.flags & MDL_HAS_COLORS) {
        m.colors.resize(m.vertCount);
        for (int i = 0; i < m.vertCount; i++) {
            m.colors[i].b = data[pos++]; m.colors[i].g = data[pos++];
            m.colors[i].r = data[pos++]; m.colors[i].a = data[pos++];
        }
    }

    memcpy(&m.indexCount, &data[pos], 2); pos += 2;
    m.indices.resize(m.indexCount);
    for (int i = 0; i < m.indexCount; i++) {
        memcpy(&m.indices[i], &data[pos], 2); pos += 2;
    }

    bool hasN = m.flags & MDL_HAS_NORMALS;
    m.frames.resize(m.frameCount);
    for (int fr = 0; fr < m.frameCount; fr++) {
        m.frames[fr].resize(m.vertCount);
        for (int v = 0; v < m.vertCount; v++) {
            memcpy(&m.frames[fr][v].x, &data[pos], 4); pos += 4;
            memcpy(&m.frames[fr][v].y, &data[pos], 4); pos += 4;
            memcpy(&m.frames[fr][v].z, &data[pos], 4); pos += 4;
            if (hasN) {
                memcpy(&m.frames[fr][v].nx, &data[pos], 4); pos += 4;
                memcpy(&m.frames[fr][v].ny, &data[pos], 4); pos += 4;
                memcpy(&m.frames[fr][v].nz, &data[pos], 4); pos += 4;
            } else {
                m.frames[fr][v].nx = m.frames[fr][v].ny = m.frames[fr][v].nz = 0;
            }
        }
    }
    return true;
}

// =====================================================================
// Build triangles for stripifier
// =====================================================================

static std::vector<uint16_t> quads_to_tris(const std::vector<uint16_t> &idx) {
    std::vector<uint16_t> out;
    out.reserve((idx.size() / 4) * 6);
    for (size_t i = 0; i + 3 < idx.size(); i += 4) {
        out.push_back(idx[i]); out.push_back(idx[i+1]); out.push_back(idx[i+2]);
        out.push_back(idx[i]); out.push_back(idx[i+2]); out.push_back(idx[i+3]);
    }
    return out;
}

static void build_triangles(const MdlMesh &m, const std::vector<uint16_t> &triIdx) {
    g_triangles.clear();
    for (size_t i = 0; i + 2 < triIdx.size(); i += 3) {
        Triangle tri;
        tri.materialId = 0;
        for (int j = 0; j < 3; j++) {
            uint16_t idx = triIdx[i + j];
            auto &tv = tri.vertices[j];
            tv.position[0] = m.frames[0][idx].x;
            tv.position[1] = m.frames[0][idx].y;
            tv.position[2] = m.frames[0][idx].z;
            tv.normal[0] = m.frames[0][idx].nx;
            tv.normal[1] = m.frames[0][idx].ny;
            tv.normal[2] = m.frames[0][idx].nz;
            tv.texcoord[0] = m.uvs.empty() ? 0 : m.uvs[idx].u;
            tv.texcoord[1] = m.uvs.empty() ? 0 : m.uvs[idx].v;
            tv.color = 0xFFFFFFFF;
            tv.vertexId = idx;
            tv.normalId = idx;
        }
        g_triangles.push_back(tri);
    }
}

// =====================================================================
// Serialize stripped MDL back to buffer
// =====================================================================

static std::vector<uint8_t> build_stripped_mdl(const MdlMesh &m) {
    std::vector<uint8_t> out;
    auto w8  = [&](uint8_t v)  { out.push_back(v); };
    auto w16 = [&](uint16_t v) { out.insert(out.end(), (uint8_t*)&v, (uint8_t*)&v + 2); };
    auto w32 = [&](uint32_t v) { out.insert(out.end(), (uint8_t*)&v, (uint8_t*)&v + 4); };
    auto wf  = [&](float v)    { out.insert(out.end(), (uint8_t*)&v, (uint8_t*)&v + 4); };

    // header
    w32(0x004C444D); // "MDL\0"
    w8(m.flags | MDL_IS_STRIPPED);
    w8(m.faceVertCount);
    w16(m.vertCount);
    w16(m.frameCount);

    if (m.flags & MDL_HAS_UVS)
        for (auto &uv : m.uvs) { wf(uv.u); wf(uv.v); }

    if (m.flags & MDL_HAS_COLORS)
        for (auto &c : m.colors) { w8(c.b); w8(c.g); w8(c.r); w8(c.a); }

    // strip data
    uint16_t stripCount = (uint16_t)g_raw_strips.size();
    uint16_t looseTris  = (uint16_t)(g_loose_triangles.size() / 3);
    w16(stripCount);
    w16(looseTris);

    for (auto &s : g_raw_strips) w16((uint16_t)s.size());
    for (auto &s : g_raw_strips) for (auto i : s) w16((uint16_t)i);
    for (auto i : g_loose_triangles) w16((uint16_t)i);

    // vertex frames
    bool hasN = m.flags & MDL_HAS_NORMALS;
    for (int fr = 0; fr < m.frameCount; fr++)
        for (int v = 0; v < m.vertCount; v++) {
            wf(m.frames[fr][v].x); wf(m.frames[fr][v].y); wf(m.frames[fr][v].z);
            if (hasN) { wf(m.frames[fr][v].nx); wf(m.frames[fr][v].ny); wf(m.frames[fr][v].nz); }
        }

    return out;
}

// =====================================================================
// Main
// =====================================================================

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <Data.rsdk> [rsdk_file_list.txt]\n", argv[0]);
        return 1;
    }

    const char *rsdkPath = argv[1];
    const char *listPath = argc > 2 ? argv[2] : "rsdk_file_list.txt";

    auto hashToName = load_file_list(listPath);
    printf("Loaded %zu known filenames\n", hashToName.size());

    RSDKArchive archive;
    if (!archive.load(rsdkPath)) {
        printf("Failed to load %s\n", rsdkPath);
        return 1;
    }
    printf("Archive: %zu entries\n", archive.entries.size());

    std::map<int, std::vector<uint8_t>> replacements;
    int meshCount = 0;

    for (int i = 0; i < (int)archive.entries.size(); i++) {
        std::string hex = hash_to_hex(archive.entries[i].hash);
        auto it = hashToName.find(hex);
        if (it == hashToName.end()) continue;

        const std::string &fname = it->second;

        // if it isn't a mesh, skip it
        if (fname.find("/Meshes/") == std::string::npos &&
            fname.find("\\Meshes\\") == std::string::npos) continue;

        // don't strip itembox
        if (fname.find("ItemBox") != std::string::npos) continue;

        // don't strip the 3d special stage ring
        if (fname.find("SpecialRing") != std::string::npos) continue;

        // don't strip egg tower (SSZ2 boss fight backdrop)
        if (fname.find("EggTower") != std::string::npos) continue;

        auto data = archive.readEntryDecrypted(i, fname.c_str());

        MdlMesh mdl = {};
        if (!parse_mdl(data.data(), data.size(), mdl)) continue;

        if (mdl.flags & MDL_IS_STRIPPED) {
            printf("  SKIP (already stripped): %s\n", fname.c_str());
            continue;
        }

        printf("  %s: %dv %df %dfaces",
               fname.c_str(), mdl.vertCount, mdl.frameCount,
               mdl.indexCount / mdl.faceVertCount);

        std::vector<uint16_t> triIdx;
        if (mdl.faceVertCount == 4) {
            triIdx = quads_to_tris(mdl.indices);
            printf(" (quads->tris)");
        } else {
            triIdx = mdl.indices;
        }

        build_triangles(mdl, triIdx);
        optimize_mesh();

        auto newData = build_stripped_mdl(mdl);
        int delta = (int)newData.size() - (int)data.size();
        printf(" [%zu->%zu bytes %+d]\n", data.size(), newData.size(), delta);

        replacements[i] = newData;
        meshCount++;
    }

    printf("\nStripped %d meshes\n", meshCount);
    if (replacements.empty()) {
        printf("Nothing to write.\n");
        return 0;
    }

    // backup
    std::string bakPath = std::string(rsdkPath) + ".bak";
    FILE *check = fopen(bakPath.c_str(), "r");
    if (!check) {
        printf("Backing up to %s\n", bakPath.c_str());
        std::ifstream src(rsdkPath, std::ios::binary);
        std::ofstream dst(bakPath, std::ios::binary);
        dst << src.rdbuf();
    } else {
        fclose(check);
    }

    archive.save(rsdkPath, replacements);
    printf("Done.\n");
    return 0;
}
