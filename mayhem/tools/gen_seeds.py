#!/usr/bin/env python3
"""Regenerates the synthetic seeds under mayhem/<target>/testsuite (PARAM.SFO, ZIM, PNG, ATRAC, expressions)."""
import os, random, struct, zlib, shutil, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.dirname(ROOT)


def put(target, name, data):
    d = os.path.join(ROOT, target, "testsuite")
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, name), "wb") as f:
        f.write(data if isinstance(data, bytes) else data.encode())


def sfo(entries):
    keys = b""
    datas = b""
    ents = b""
    for key, fmt, val, maxlen in entries:
        koff = len(keys)
        keys += key.encode() + b"\0"
        if fmt == 0x0404:
            raw = struct.pack("<I", val)
            plen = 4
        else:
            raw = val.encode() + (b"\0" if fmt == 0x0204 else b"")
            plen = len(raw)
        ents += struct.pack("<HHIII", koff, fmt, plen, maxlen, len(datas))
        datas += raw + b"\0" * (maxlen - len(raw))
    while len(keys) % 4:
        keys += b"\0"
    key_start = 20 + len(ents)
    data_start = key_start + len(keys)
    return b"\0PSF" + struct.pack("<IIII", 0x00000101, key_start, data_start, len(entries)) + ents + keys + datas


def png(w, h, color, depth, rows, palette=None, trns=None):
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    raw = b"".join(b"\0" + r for r in rows)
    out = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, depth, color, 0, 0, 0))
    if palette:
        out += chunk(b"PLTE", palette)
    if trns:
        out += chunk(b"tRNS", trns)
    return out + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")


def zstd_raw(data):
    assert len(data) < 256
    return b"\x28\xb5\x2f\xfd" + bytes([0x20, len(data)]) + struct.pack("<I", 1 | (len(data) << 3))[:3] + data


def zim(w, h, flags, payload):
    return b"ZIMG" + struct.pack("<iii", w, h, flags) + payload


def main():
    # expression
    for i, e in enumerate([
        "1+2*3", "(1+2)*3", "0x1000 + 0x20", "a0 + 4", "[0x08800000]", "[sym1+8] == 0", "sp - 0x10",
        "ra & 0xFFFF", "t0 > 3 ? 10 : 20", "(a0 << 2) | (a1 >> 1)", "!0 && ~1", "f0 * 2.5", "-5 / 2 % 3",
        "[a0] + [a1 + 4]", "pc != 0 || lo == 7", "sym12 ^ 0xFF",
    ]):
        put("fuzz_expression", "expr%02d.txt" % i, e)

    # SFO
    put("fuzz_paramsfo", "game.sfo", sfo([
        ("BOOTABLE", 0x0404, 1, 4), ("CATEGORY", 0x0204, "UG", 4), ("DISC_ID", 0x0204, "ULUS10000", 16),
        ("DISC_VERSION", 0x0204, "1.00", 8), ("PARENTAL_LEVEL", 0x0404, 5, 4), ("PSP_SYSTEM_VER", 0x0204, "5.00", 8),
        ("REGION", 0x0404, 32768, 4), ("TITLE", 0x0204, "Test Game", 128)]))
    put("fuzz_paramsfo", "save.sfo", sfo([
        ("CATEGORY", 0x0204, "MS", 4), ("PARENTAL_LEVEL", 0x0404, 1, 4), ("SAVEDATA_DETAIL", 0x0204, "detail", 1024),
        ("SAVEDATA_DIRECTORY", 0x0204, "SAVE0", 32), ("SAVEDATA_TITLE", 0x0204, "Save", 128), ("TITLE", 0x0204, "Game", 128)]))
    put("fuzz_paramsfo", "minimal.sfo", sfo([("TITLE", 0x0204, "T", 8)]))

    # ZIM
    px4 = bytes((i * 37) & 255 for i in range(2 * 2 * 4))
    put("fuzz_zim", "raw_rgba_2x2.zim", zim(2, 2, 0, px4))
    px565 = bytes((i * 11) & 255 for i in range(8 * 8 * 2))
    put("fuzz_zim", "zstd_565_8x8.zim", zim(8, 8, 2 | 512, zstd_raw(px565)))
    mips = bytes((i * 5) & 255 for i in range(4 * 4 * 4 + 2 * 2 * 4 + 1 * 1 * 4))
    put("fuzz_zim", "zlib_rgba_4x4_mips.zim", zim(4, 4, 16 | 256, zlib.compress(mips)))
    px4444 = bytes((i * 3) & 255 for i in range(8 * 4 * 2))
    put("fuzz_zim", "zlib_4444_8x4.zim", zim(8, 4, 1 | 256, zlib.compress(px4444)))

    # PNG
    put("fuzz_png", "rgba_2x2.png", png(2, 2, 6, 8, [bytes([255, 0, 0, 255, 0, 255, 0, 128]), bytes([0, 0, 255, 255, 1, 2, 3, 0])]))
    put("fuzz_png", "indexed_4x2.png", png(4, 2, 3, 8, [bytes([0, 1, 2, 3]), bytes([3, 2, 1, 0])],
        palette=bytes([0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255]), trns=bytes([0, 128, 255, 255])))
    put("fuzz_png", "gray_3x3.png", png(3, 3, 0, 8, [bytes([0, 128, 255])] * 3))
    put("fuzz_png", "rgb16_2x1.png", png(2, 1, 2, 16, [bytes(range(12))]))
    put("fuzz_png", "gray1_8x2.png", png(8, 2, 0, 1, [b"\xa5", b"\x5a"]))
    for rel in ["assets/ui_images/psp_display.png", "icons/hicolor/16x16/apps/ppsspp.png", "assets/ui_images/bg.png", "icons/hicolor/32x32/apps/ppsspp.png"]:
        shutil.copy(os.path.join(SRC, rel), os.path.join(ROOT, "fuzz_png", "testsuite", rel.replace("/", "_")))

    # ini / json from upstream's own small assets
    for rel in ["assets/langregion.ini", "assets/lang/en_US.ini"]:
        data = open(os.path.join(SRC, rel), "rb").read()
        put("fuzz_inifile", os.path.basename(rel), data[:6000])
    comp = open(os.path.join(SRC, "assets/compat.ini"), "rb").read()
    put("fuzz_inifile", "compat_head.ini", comp[comp.index(b"[VertexDepthRounding]"):][:3000])
    kf = open(os.path.join(SRC, "assets/knownfuncs.ini"), "rb").read()
    put("fuzz_inifile", "knownfuncs_head.ini", kf[:3000])
    put("fuzz_inifile", "small.ini", "# comment\n[General]\nName = value\nCount = 42\nRatio = 1.5\nFlag = True\n\n[List]\n1 = a\n2 = b\n$Include = x\n")

    # ATRAC: byte 0 = config, then frames
    rnd = random.Random(1234)
    for cfg in (0x02, 0x03, 0x06, 0x0A, 0x1A, 0x00, 0x01, 0x04, 0x0C):
        put("fuzz_atrac", "cfg%02x.bin" % cfg, bytes([cfg]) + bytes(rnd.randrange(256) for _ in range(400)))
    put("fuzz_atrac", "zeros_at3.bin", bytes([0x02]) + bytes(192))
    put("fuzz_atrac", "zeros_at3p.bin", bytes([0x00]) + bytes(744))


main()
