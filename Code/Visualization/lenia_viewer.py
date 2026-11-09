#!/usr/bin/env python3
"""
Lenia frame viewer.

Shows the PGM frames written by the simulation (frame_00000.pgm,
frame_00010.pgm, ...). Scrub through them with the slider or play them
back as a frame-by-frame movie.

Only the Python standard library (tkinter) is used, so nothing has to be
installed with pip.

Usage:
    python lenia_viewer.py [FOLDER] [--fps N] [--zoom N]

Keyboard:
    Space         play / pause
    Left / Right  previous / next frame
    Home / End    first / last frame
    C             toggle colour map
    R             reload the folder (picks up new frames)
    Esc           quit
"""

import argparse
import re
import sys
import tkinter as tk
from pathlib import Path

FRAME_PATTERN = re.compile(r"^frame_(\d+)\.pgm$", re.IGNORECASE)

# Anchor colours of the colour map (black -> purple -> orange -> pale yellow),
# similar to matplotlib's "inferno". They are interpolated to 256 entries.
COLOUR_ANCHORS = [
    (0, 0, 4), (40, 11, 84), (101, 21, 110), (159, 42, 99),
    (212, 72, 66), (245, 125, 21), (250, 193, 39), (252, 255, 164),
]

# Target size of the image on screen when --zoom is not given.
AUTO_ZOOM_PIXELS = 800


def build_lut(anchors):
    """Interpolate anchor colours into three 256-byte lookup tables (R, G, B)."""
    channels = ([], [], [])
    segments = len(anchors) - 1
    for value in range(256):
        t = value / 255 * segments
        k = min(int(t), segments - 1)
        f = t - k
        for c in range(3):
            a, b = anchors[k][c], anchors[k + 1][c]
            channels[c].append(round(a + (b - a) * f))
    return tuple(bytes(ch) for ch in channels)


GREY_LUT = (bytes(range(256)),) * 3
COLOUR_LUT = build_lut(COLOUR_ANCHORS)


def find_frames(folder):
    """Return [(number, path), ...] of all frame_*.pgm files, sorted by number."""
    if not folder.is_dir():
        return []
    frames = []
    for path in folder.iterdir():
        match = FRAME_PATTERN.match(path.name)
        if match and path.is_file():
            frames.append((int(match.group(1)), path))
    frames.sort()
    return frames


def read_pgm(path):
    """Read a binary (P5) PGM file. Return (width, height, pixels), one byte per pixel."""
    data = path.read_bytes()
    fields = []
    pos = 0
    while len(fields) < 4:  # magic, width, height, maxval
        if pos >= len(data):
            raise ValueError("truncated header")
        ch = data[pos:pos + 1]
        if ch.isspace():
            pos += 1
        elif ch == b"#":  # comment until the end of the line
            while pos < len(data) and data[pos:pos + 1] not in b"\r\n":
                pos += 1
        else:
            start = pos
            while pos < len(data) and not data[pos:pos + 1].isspace():
                pos += 1
            fields.append(data[start:pos])
    pos += 1  # exactly one whitespace byte separates the header from the pixels

    if fields[0] != b"P5":
        raise ValueError("not a binary PGM (P5) file")
    width, height, maxval = (int(f) for f in fields[1:])
    if not 0 < maxval < 256:
        raise ValueError("only 8-bit PGM files are supported")
    pixels = data[pos:pos + width * height]
    if len(pixels) != width * height:
        raise ValueError("file is shorter than its header says")
    if maxval != 255:  # stretch to the full 0..255 range
        scale = bytes(min(255, round(v * 255 / maxval)) for v in range(256))
        pixels = pixels.translate(scale)
    return width, height, pixels


def to_ppm(width, height, pixels, lut):
    """Colour greyscale pixels with a lookup table and pack them as a binary PPM (P6)."""
    rgb = bytearray(len(pixels) * 3)
    for c in range(3):
        rgb[c::3] = pixels.translate(lut[c])
    return b"P6\n%d %d\n255\n" % (width, height) + bytes(rgb)


class Viewer:
    """Tk window with the image, a frame slider and playback controls."""

    def __init__(self, root, folder, fps, zoom):
        self.root = root
        self.folder = folder
        self.zoom = zoom  # 0 = choose automatically from the first frame
        self.frames = []
        self.index = 0
        self.playing = False
        self.after_id = None
        self.photo = None  # keep a reference, otherwise Tk throws the image away

        root.title(f"Lenia viewer - {folder}")
        root.configure(padx=8, pady=8)

        self.image_label = tk.Label(root, bg="black")
        self.image_label.pack()

        self.info = tk.Label(root, font=("Consolas", 10), anchor="w")
        self.info.pack(fill=tk.X, pady=(6, 0))

        self.slider = tk.Scale(root, orient=tk.HORIZONTAL, showvalue=False,
                               from_=0, to=0, command=self.on_slider)
        self.slider.pack(fill=tk.X)

        bar = tk.Frame(root)
        bar.pack(pady=(4, 0))

        def button(text, command, width=4):
            b = tk.Button(bar, text=text, width=width, command=command, takefocus=0)
            b.pack(side=tk.LEFT, padx=1)
            return b

        button("|<", self.first)
        button("<", self.prev)
        self.play_button = button("Play", self.toggle_play, width=6)
        button(">", self.next)
        button(">|", self.last)

        tk.Label(bar, text="   FPS:").pack(side=tk.LEFT)
        self.fps = tk.IntVar(value=fps)
        tk.Spinbox(bar, from_=1, to=60, width=4, textvariable=self.fps).pack(side=tk.LEFT)

        self.loop = tk.BooleanVar(value=True)
        tk.Checkbutton(bar, text="Loop", variable=self.loop,
                       takefocus=0).pack(side=tk.LEFT, padx=(10, 0))
        self.colour = tk.BooleanVar(value=True)
        tk.Checkbutton(bar, text="Colour", variable=self.colour, command=self.redraw,
                       takefocus=0).pack(side=tk.LEFT)
        tk.Button(bar, text="Reload", command=self.reload,
                  takefocus=0).pack(side=tk.LEFT, padx=(10, 0))

        # Navigation keys are ignored while the slider or the FPS box has focus,
        # because those widgets already react to them on their own.
        self.bind_key("<space>", self.toggle_play)
        self.bind_key("<Left>", self.prev, nav=True)
        self.bind_key("<Right>", self.next, nav=True)
        self.bind_key("<Home>", self.first, nav=True)
        self.bind_key("<End>", self.last, nav=True)
        self.bind_key("<c>", self.toggle_colour)
        self.bind_key("<r>", self.reload)
        root.bind("<Escape>", lambda event: root.destroy())

        self.reload()

    def bind_key(self, sequence, action, nav=False):
        def handler(event):
            if isinstance(event.widget, tk.Spinbox):
                return
            if nav and event.widget is self.slider:
                return
            action()
        self.root.bind(sequence, handler)

    # ----- frames -----------------------------------------------------------

    def reload(self):
        """Scan the folder again, e.g. while the simulation is still running."""
        self.frames = find_frames(self.folder)
        if not self.frames:
            self.pause()
            self.slider.config(to=0)
            self.info.config(text=f"No frame_*.pgm files in {self.folder.resolve()}")
            return
        self.slider.config(to=len(self.frames) - 1)
        self.show(min(self.index, len(self.frames) - 1))

    def show(self, index):
        if not self.frames:
            return
        self.index = max(0, min(index, len(self.frames) - 1))
        number, path = self.frames[self.index]
        try:
            width, height, pixels = read_pgm(path)
        except (OSError, ValueError) as error:
            self.info.config(text=f"Cannot read {path.name}: {error}")
            return

        if self.zoom <= 0:
            self.zoom = max(1, AUTO_ZOOM_PIXELS // max(width, height))

        lut = COLOUR_LUT if self.colour.get() else GREY_LUT
        ppm = to_ppm(width, height, pixels, lut)
        photo = tk.PhotoImage(data=ppm, format="PPM")  # Tk 8.6 reads binary PPM data
        if self.zoom > 1:
            photo = photo.zoom(self.zoom)
        self.photo = photo
        self.image_label.config(image=photo)

        self.slider.set(self.index)
        self.info.config(text=f"frame {self.index + 1}/{len(self.frames)}"
                              f"    iteration {number}    {path.name}")

    def redraw(self):
        self.show(self.index)

    def on_slider(self, value):
        index = int(float(value))
        if index != self.index:
            self.show(index)

    def first(self):
        self.show(0)

    def last(self):
        self.show(len(self.frames) - 1)

    def prev(self):
        self.show(self.index - 1)

    def next(self):
        self.show(self.index + 1)

    def toggle_colour(self):
        self.colour.set(not self.colour.get())
        self.redraw()

    # ----- playback ---------------------------------------------------------

    def toggle_play(self):
        if self.playing:
            self.pause()
        else:
            self.play()

    def play(self):
        if not self.frames:
            return
        if self.index >= len(self.frames) - 1:  # at the end: start from the beginning
            self.show(0)
        self.playing = True
        self.play_button.config(text="Pause")
        self.schedule()

    def pause(self):
        self.playing = False
        self.play_button.config(text="Play")
        if self.after_id is not None:
            self.root.after_cancel(self.after_id)
            self.after_id = None

    def schedule(self):
        try:
            fps = max(1, min(60, int(self.fps.get())))
        except (tk.TclError, ValueError):  # FPS box empty or not a number
            fps = 10
        self.after_id = self.root.after(round(1000 / fps), self.tick)

    def tick(self):
        self.after_id = None
        if not self.playing:
            return
        if self.index < len(self.frames) - 1:
            self.show(self.index + 1)
        elif self.loop.get():
            self.show(0)
        else:
            self.pause()
            return
        self.schedule()


def main():
    parser = argparse.ArgumentParser(description="Browse and play Lenia PGM frames.")
    parser.add_argument("folder", nargs="?", default="Results",
                        help="folder with frame_*.pgm files (default: Results)")
    parser.add_argument("--fps", type=int, default=10,
                        help="playback speed in frames per second (default: 10)")
    parser.add_argument("--zoom", type=int, default=0,
                        help=f"integer image scale (default: fit about {AUTO_ZOOM_PIXELS} px)")
    args = parser.parse_args()

    if sys.platform == "win32":  # sharp, not blurry, on high-DPI screens
        try:
            import ctypes
            ctypes.windll.shcore.SetProcessDpiAwareness(1)
        except (AttributeError, OSError):
            pass

    root = tk.Tk()
    Viewer(root, Path(args.folder), args.fps, args.zoom)
    root.mainloop()


if __name__ == "__main__":
    main()
