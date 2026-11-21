#!/usr/bin/env python3
"""
Lenia GIF maker.

Turns the PGM frames written by the simulation (FOLDER/frames/frame_00000.pgm,
...) into one looping animated GIF, saved by default as FOLDER/animation.gif.
FOLDER is the results folder ("Results"); a folder with the frames directly
inside works too.

Only the Python standard library is used: the GIF (with its LZW compression)
is written by hand, so nothing has to be installed with pip. The grey value
of every PGM pixel (0..255) is used directly as an index into a 256-colour
palette, the same colour map as in lenia_viewer.py.

Usage:
    python make_gif.py [FOLDER] [--fps N] [--grey] [--out FILE]
"""

import argparse
import struct
import sys
from pathlib import Path

from lenia_viewer import COLOUR_LUT, GREY_LUT, find_frames, frames_folder, read_pgm

GIF_NAME = "animation.gif"
MAX_CODE = 4095  # GIF codes are at most 12 bits


def lzw_compress(indices):
    """Compress 8-bit palette indices with GIF's variable-length LZW."""
    clear, end = 256, 257
    out = bytearray()
    bit_buffer = 0
    bit_count = 0
    code_size = 9
    next_code = end + 1
    table = {}

    def emit(code):
        nonlocal bit_buffer, bit_count
        bit_buffer |= code << bit_count
        bit_count += code_size
        while bit_count >= 8:
            out.append(bit_buffer & 0xFF)
            bit_buffer >>= 8
            bit_count -= 8

    emit(clear)
    prefix = indices[0]
    for pixel in indices[1:]:
        key = (prefix << 8) | pixel
        code = table.get(key)
        if code is not None:  # sequence already known, keep extending it
            prefix = code
            continue
        emit(prefix)
        if next_code >= (1 << code_size) and code_size < 12:
            code_size += 1
        if next_code < MAX_CODE:
            table[key] = next_code
            next_code += 1
        else:  # table full: start a new one
            emit(clear)
            table.clear()
            code_size = 9
            next_code = end + 1
        prefix = pixel

    emit(prefix)
    if next_code >= (1 << code_size) and code_size < 12:
        code_size += 1
    emit(end)
    if bit_count:
        out.append(bit_buffer & 0xFF)
    return bytes(out)


def sub_blocks(data):
    """Split data into GIF sub-blocks (length byte + up to 255 bytes), ending with 0."""
    out = bytearray()
    for start in range(0, len(data), 255):
        chunk = data[start:start + 255]
        out.append(len(chunk))
        out += chunk
    out.append(0)
    return bytes(out)


def write_gif(path, width, height, frames, lut, fps):
    """Write frames (iterable of width*height index bytes) as a looping GIF."""
    delay = max(2, round(100 / fps))  # in 1/100 s; browsers treat < 2 as slow
    palette = bytes(value for rgb in zip(*lut) for value in rgb)

    with open(path, "wb") as f:
        f.write(b"GIF89a")
        # logical screen: size, global 256-colour table, background 0, no aspect
        f.write(struct.pack("<HHBBB", width, height, 0xF7, 0, 0))
        f.write(palette)
        # NETSCAPE2.0 extension: loop forever
        f.write(b"\x21\xFF\x0BNETSCAPE2.0\x03\x01\x00\x00\x00")
        count = 0
        for pixels in frames:
            # graphic control extension: frame delay
            f.write(struct.pack("<4BHBB", 0x21, 0xF9, 4, 0, delay, 0, 0))
            # image descriptor: full frame at (0, 0), no local colour table
            f.write(struct.pack("<BHHHHB", 0x2C, 0, 0, width, height, 0))
            f.write(b"\x08")  # LZW minimum code size
            f.write(sub_blocks(lzw_compress(pixels)))
            count += 1
        f.write(b"\x3B")  # trailer
    return count


def main():
    parser = argparse.ArgumentParser(description="Make an animated GIF from Lenia PGM frames.")
    parser.add_argument("folder", nargs="?", default="Results",
                        help="results folder; its frames/ subfolder holds frame_*.pgm "
                             "(default: Results)")
    parser.add_argument("--fps", type=int, default=10,
                        help="frames per second in the GIF (default: 10, max 50)")
    parser.add_argument("--grey", action="store_true",
                        help="greyscale instead of the colour map")
    parser.add_argument("--out", type=Path,
                        help=f"output file (default: FOLDER/{GIF_NAME})")
    args = parser.parse_args()

    folder = Path(args.folder)
    frames = find_frames(folder)
    if not frames:
        print(f"No frame_*.pgm files in {frames_folder(folder).resolve()}",
              file=sys.stderr)
        return 1

    width, height, _ = read_pgm(frames[0][1])

    def pixel_frames():
        for number, path in frames:
            w, h, pixels = read_pgm(path)
            if (w, h) != (width, height):
                raise ValueError(f"{path.name} is {w}x{h}, expected {width}x{height}")
            yield pixels

    out = args.out or folder / GIF_NAME
    fps = max(1, min(50, args.fps))
    try:
        count = write_gif(out, width, height, pixel_frames(),
                          GREY_LUT if args.grey else COLOUR_LUT, fps)
    except (OSError, ValueError) as error:
        print(f"Cannot create GIF: {error}", file=sys.stderr)
        return 1
    print(f"Saved {out} ({count} frames, {fps} fps, {out.stat().st_size // 1024} KB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
