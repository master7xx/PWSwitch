"""Generate the Windows application icon using only the Python standard library."""

import binascii
import math
import struct
import zlib
from pathlib import Path

SIZES = (16, 20, 24, 32, 48, 256)
SCALE = 4
BOLT = ((143, 38), (84, 139), (121, 139), (107, 218),
        (175, 112), (138, 112), (158, 38))
PALETTES = (
    ("pwswitch.ico", (226, 187, 59), (19, 39, 42)),
    ("pwswitch_maximum_saving.ico", (166, 223, 48), (19, 39, 42)),
    ("pwswitch_saving.ico", (41, 169, 102), (19, 39, 42)),
    ("pwswitch_performance.ico", (237, 128, 50), (19, 39, 42)),
    ("pwswitch_ultimate.ico", (228, 56, 56), (248, 251, 255)),
    ("pwswitch_unknown.ico", (96, 125, 139), (248, 251, 255)),
)


def in_bolt(x, y):
    inside = False
    previous = BOLT[-1]
    for point in BOLT:
        x1, y1 = previous
        x2, y2 = point
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            inside = not inside
        previous = point
    return inside


def png_chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", binascii.crc32(tag + data) & 0xffffffff)


def make_png(size, background, bolt):
    rows = []
    step = 256 / (size * SCALE)
    for py in range(size):
        row = bytearray()
        for px in range(size):
            sums = [0, 0, 0, 0]
            for sy in range(SCALE):
                y = (py * SCALE + sy + 0.5) * step
                for sx in range(SCALE):
                    x = (px * SCALE + sx + 0.5) * step
                    distance = math.hypot(x - 128, y - 128)
                    if distance > 118:
                        color = (0, 0, 0, 0)
                    elif distance > 110:
                        color = (23, 36, 42, 255)
                    elif distance > 102:
                        color = (247, 249, 249, 255)
                    elif in_bolt(x, y):
                        color = (*bolt, 255)
                    else:
                        color = (*background, 255)
                    for i in range(4):
                        sums[i] += color[i]
            count = SCALE * SCALE
            alpha = (sums[3] + count // 2) // count
            # All nontransparent samples are opaque; unpremultiply edge pixels.
            if sums[3]:
                rgb = [(sums[i] * 255 + sums[3] // 2) // sums[3] for i in range(3)]
            else:
                rgb = [0, 0, 0]
            row.extend((*rgb, alpha))
        rows.append(b"\0" + row)
    header = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", header)
            + png_chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + png_chunk(b"IEND", b""))


def make_ico(background, bolt):
    images = [(size, make_png(size, background, bolt)) for size in SIZES]
    offset = 6 + 16 * len(images)
    directory = bytearray()
    for size, png in images:
        dimension = size if size < 256 else 0
        directory.extend(struct.pack("<BBBBHHII", dimension, dimension, 0, 0, 1, 32, len(png), offset))
        offset += len(png)
    return struct.pack("<HHH", 0, 1, len(images)) + directory + b"".join(png for _, png in images)


def main():
    for name, background, bolt in PALETTES:
        output = Path(__file__).with_name(name)
        output.write_bytes(make_ico(background, bolt))
        print(output)


if __name__ == "__main__":
    main()
