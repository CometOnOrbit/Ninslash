#!/usr/bin/env python3
"""Draw and rig native-style Lost Protocol Spine 3.6 assets.

The generated PNGs are attachment atlases, not character illustrations. Every
moving limb, shield, tool and weapon occupies its own atlas region and is bound
to a dedicated bone in the generated JSON.
"""

from __future__ import annotations

import json
import math
from dataclasses import dataclass, field
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "data" / "anim" / "lost_protocol"
SIZE = 512
INK = (20, 22, 25, 255)
DARK = (43, 46, 51, 255)
MID = (101, 106, 112, 255)
LIGHT = (207, 211, 214, 255)
WHITE = (239, 241, 242, 255)
RED = (245, 42, 32, 255)
ORANGE = (255, 126, 28, 255)
BLUE = (40, 174, 235, 255)
GREEN = (53, 201, 105, 255)


@dataclass
class Part:
    name: str
    image: Image.Image
    bone: str
    x: float = 0
    y: float = 0
    rotation: float = 0


@dataclass
class Bone:
    name: str
    parent: str | None = "root"
    x: float = 0
    y: float = 0
    rotation: float = 0
    length: float = 0


@dataclass
class Rig:
    name: str
    scale: float
    bones: list[Bone] = field(default_factory=list)
    parts: list[Part] = field(default_factory=list)
    animations: dict = field(default_factory=dict)


def canvas(w: int, h: int) -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    return image, ImageDraw.Draw(image)


def outlined_polygon(size: tuple[int, int], points, fill=LIGHT, accent=None) -> Image.Image:
    image, draw = canvas(*size)
    draw.polygon(points, fill=fill, outline=INK, width=4)
    if accent:
        draw.line(accent, fill=WHITE, width=3)
    return image


def armored_body(w: int, h: int, color=LIGHT, lamp=RED) -> Image.Image:
    image, draw = canvas(w, h)
    points = [(5, h // 3), (w // 5, 5), (w * 4 // 5, 5), (w - 5, h // 3), (w - 10, h - 8), (w // 2, h - 3), (10, h - 8)]
    draw.polygon(points, fill=color, outline=INK, width=5)
    draw.line((w // 5, 12, w * 4 // 5, 12), fill=WHITE, width=3)
    draw.rounded_rectangle((w // 2 - 16, h // 2 - 9, w // 2 + 16, h // 2 + 9), 6, fill=INK)
    draw.rounded_rectangle((w // 2 - 11, h // 2 - 4, w // 2 + 11, h // 2 + 4), 3, fill=lamp)
    return image


def limb(length: int, width: int, color=LIGHT, lamp=None) -> Image.Image:
    image, draw = canvas(length + 8, width + 8)
    pts = [(4, width // 3 + 4), (12, 4), (length - 5, 7), (length + 3, width // 2 + 4), (length - 5, width + 1), (12, width + 4)]
    draw.polygon(pts, fill=color, outline=INK, width=4)
    draw.line((14, 9, length - 8, 11), fill=WHITE, width=2)
    if lamp:
        draw.line((length // 2, width // 2 + 4, length - 10, width // 2 + 4), fill=lamp, width=3)
    return image


def joint(radius: int, lamp=RED) -> Image.Image:
    s = radius * 2 + 8
    image, draw = canvas(s, s)
    draw.ellipse((4, 4, s - 5, s - 5), fill=DARK, outline=INK, width=4)
    draw.ellipse((radius // 2 + 4, radius // 2 + 4, s - radius // 2 - 5, s - radius // 2 - 5), fill=MID, outline=INK, width=2)
    draw.ellipse((radius, radius, s - radius - 1, s - radius - 1), fill=lamp)
    return image


def foot(w=34, h=18) -> Image.Image:
    return outlined_polygon((w, h), [(2, h - 3), (8, 3), (w - 10, 3), (w - 2, h - 3)], DARK, [(9, 6), (w - 12, 6)])


def barrel(length: int, width: int, lamp=RED) -> Image.Image:
    image, draw = canvas(length + 8, width + 8)
    draw.rounded_rectangle((3, 5, length + 4, width + 3), width // 3, fill=DARK, outline=INK, width=4)
    draw.line((16, 8, length - 6, 8), fill=LIGHT, width=4)
    draw.line((length // 3, width // 2 + 4, length - 8, width // 2 + 4), fill=lamp, width=4)
    return image


def shield(w: int, h: int, color=BLUE) -> Image.Image:
    image, draw = canvas(w, h)
    pts = [(w // 2, 3), (w - 5, h // 4), (w - 10, h * 3 // 4), (w // 2, h - 3), (6, h * 3 // 4), (3, h // 4)]
    draw.polygon(pts, fill=DARK, outline=INK, width=4)
    inner = [(w // 2, 10), (w - 13, h // 3), (w - 17, h * 2 // 3), (w // 2, h - 10), (14, h * 2 // 3), (10, h // 3)]
    draw.polygon(inner, fill=color, outline=WHITE, width=2)
    return image


def claw(w=32, h=28, color=GREEN) -> Image.Image:
    image, draw = canvas(w, h)
    draw.ellipse((8, 7, 24, 23), fill=DARK, outline=INK, width=3)
    draw.arc((0, 0, 23, 25), 260, 80, fill=color, width=5)
    draw.arc((9, 0, 32, 25), 100, 280, fill=color, width=5)
    return image


def canister(w=34, h=64, color=GREEN) -> Image.Image:
    image, draw = canvas(w, h)
    draw.rounded_rectangle((3, 3, w - 4, h - 4), 10, fill=DARK, outline=INK, width=4)
    draw.rounded_rectangle((8, 11, w - 9, h - 14), 7, fill=(75, 82, 87, 255), outline=LIGHT, width=2)
    draw.line((w // 2, 17, w // 2, h - 21), fill=color, width=7)
    draw.ellipse((w // 2 - 6, h // 2 - 6, w // 2 + 6, h // 2 + 6), fill=WHITE, outline=color, width=3)
    return image


def add_leg(rig: Rig, prefix: str, x: float, y: float, upper_len=34, lower_len=31, angle=90, color=LIGHT, lamp=RED):
    upper = f"{prefix}_upper"
    lower = f"{prefix}_lower"
    ankle = f"{prefix}_ankle"
    knee_rotation = -38 if angle > -90 else 38 if angle < -90 else 0
    ankle_rotation = -(angle + knee_rotation)
    rig.bones += [Bone(upper, "body", x, y, angle, upper_len), Bone(lower, upper, upper_len, 0, knee_rotation, lower_len), Bone(ankle, lower, lower_len, 0, ankle_rotation, 18)]
    rig.parts += [Part(f"{prefix}_joint", joint(8, lamp), upper), Part(f"{prefix}_upper", limb(upper_len, 15, color, lamp), upper, upper_len / 2), Part(f"{prefix}_lower", limb(lower_len, 13, DARK, lamp), lower, lower_len / 2), Part(f"{prefix}_foot", foot(), ankle, 12)]


def base_anims(extra=None):
    anims = {
        "idle": {"bones": {"body": {"translate": [{"time": 0, "y": 0}, {"time": .6, "y": 2}, {"time": 1.2, "y": 0}]}}},
        "attack": {"bones": {"weapon": {"translate": [{"time": 0, "x": 0}, {"time": .08, "x": -7}, {"time": .3, "x": 0}], "rotate": [{"time": 0, "angle": 0}, {"time": .08, "angle": -4}, {"time": .3, "angle": 0}]}}},
        "hit": {"bones": {"body": {"rotate": [{"time": 0, "angle": 0}, {"time": .06, "angle": -7}, {"time": .2, "angle": 0}]}}},
        "emp": {"bones": {"body": {"translate": [{"time": 0, "x": -2}, {"time": .06, "x": 2}, {"time": .12, "x": -2}, {"time": .2, "x": 0}]}}},
        "destroyed": {"bones": {"body": {"rotate": [{"time": 0, "angle": 0}, {"time": .8, "angle": 82}], "translate": [{"time": 0, "y": 0}, {"time": .8, "y": -18}]}}},
    }
    if extra:
        anims.update(extra)
    return anims


def walker_rig(name: str, body_color, lamp, feature: str, boss=False) -> Rig:
    rig = Rig(name, .72 if not boss else 1.0)
    body_w = 100 if feature == "shield" else 76 if feature == "rail" else 88
    body_h = 62 if feature == "shield" else 48 if feature == "rail" else 58
    if boss:
        body_w, body_h = 142, 64
    rig.bones = [Bone("body"), Bone("weapon", "body", body_w * .28, 13, 0, 70)]
    rig.parts = [Part("body", armored_body(body_w, body_h, body_color, lamp), "body")]
    upper = 25 if feature == "shield" else 45 if feature == "rail" else 34
    lower = 23 if feature == "shield" else 39 if feature == "rail" else 31
    if boss:
        upper, lower = 42, 38
    add_leg(rig, "front", body_w * .27, -body_h * .30, upper, lower, -68, body_color, lamp)
    add_leg(rig, "back", -body_w * .25, -body_h * .30, upper, lower, -112, body_color, lamp)
    if feature == "shield":
        rig.bones += [Bone("shield_top", "body", 46, 22, -8), Bone("shield_bottom", "body", 47, -24, 8)]
        rig.parts += [Part("shield_top", shield(50, 88, BLUE), "shield_top"), Part("shield_bottom", shield(50, 88, BLUE), "shield_bottom")]
    elif feature == "repair":
        rig.bones += [Bone("tank", "body", -30, 34), Bone("tool_upper", "body", 25, 22, 22, 38), Bone("tool_lower", "tool_upper", 38, 0, -48, 33), Bone("tool2_upper", "body", 20, -4, -18, 35), Bone("tool2_lower", "tool2_upper", 35, 0, 45, 29)]
        rig.parts += [Part("repair_tank", canister(38, 72, GREEN), "tank"), Part("tool_upper", limb(39, 13, LIGHT, GREEN), "tool_upper", 19), Part("tool_lower", limb(34, 11, DARK, GREEN), "tool_lower", 17), Part("tool_claw", claw(38, 32, GREEN), "tool_lower", 36), Part("tool2_upper", limb(36, 12, LIGHT, GREEN), "tool2_upper", 18), Part("tool2_lower", limb(30, 10, DARK, GREEN), "tool2_lower", 15), Part("tool2_claw", claw(34, 28, GREEN), "tool2_lower", 32)]
    elif feature == "rail":
        rig.parts += [Part("rail", barrel(148, 17, RED), "weapon", 71)]
    elif feature == "siege":
        rig.parts += [Part("cannon", barrel(190, 30, RED), "weapon", 90)]
        rig.bones += [Bone("mine_rack", "body", -38, 6)]
        rig.parts += [Part("mine_rack", armored_body(48, 28, DARK, ORANGE), "mine_rack")]
        add_leg(rig, "middle", 0, -22, 44, 39, -90, body_color, lamp)
    rig.animations = base_anims({"phase": {"bones": {"weapon": {"rotate": [{"time": 0, "angle": 0}, {"time": .35, "angle": -18}, {"time": .7, "angle": 0}]}}}} if boss else None)
    return rig


def saboteur_rig() -> Rig:
    rig = Rig("saboteur", .68, [Bone("body"), Bone("weapon", "body", 5, 28)])
    rig.parts = [Part("body", armored_body(104, 40, DARK, ORANGE), "body"), Part("emp_coil", joint(24, ORANGE), "weapon")]
    add_leg(rig, "front", 35, -10, 34, 28, -48, DARK, ORANGE)
    add_leg(rig, "front_mid", 12, -13, 31, 26, -70, DARK, ORANGE)
    add_leg(rig, "back_mid", -12, -13, 31, 26, -110, DARK, ORANGE)
    add_leg(rig, "back", -35, -10, 34, 28, -132, DARK, ORANGE)
    rig.bones += [Bone("antenna_left", "body", -18, 18, -72), Bone("antenna_right", "body", 18, 18, -108)]
    rig.parts += [Part("antenna_left", limb(38, 7, DARK, ORANGE), "antenna_left", 19), Part("antenna_right", limb(38, 7, DARK, ORANGE), "antenna_right", 19)]
    rig.animations = base_anims()
    return rig


def overseer_rig() -> Rig:
    rig = Rig("overseer_core", 1.0, [Bone("body"), Bone("weapon", "body", 22, 0)])
    rig.parts = [Part("body", joint(60, RED), "body"), Part("iris", joint(30, RED), "body")]
    for i, (x, y, rot) in enumerate(((0, 43, 90), (-38, -10, 205), (38, -10, -25))):
        bone = f"arm{i}"
        rig.bones.append(Bone(bone, "body", x, y, rot, 38))
        rig.parts += [Part(bone, limb(43, 15, LIGHT, BLUE), bone, 20), Part(f"node{i}", joint(11, BLUE), bone, 42)]
    rig.animations = base_anims({"phase": {"bones": {f"arm{i}": {"rotate": [{"time": 0, "angle": 0}, {"time": .4, "angle": 120}, {"time": .8, "angle": 0}]} for i in range(3)}}})
    return rig


def drone_rig(name: str, module: str) -> Rig:
    color = RED if module == "assault" else BLUE if module == "guardian" else GREEN
    rig = Rig(name, .52, [Bone("body"), Bone("weapon", "body", 22, 0), Bone("left", "body", -16, 0), Bone("right", "body", 16, 0)])
    rig.parts = [Part("core", joint(24, color), "body")]
    if module == "assault":
        rig.parts += [Part("pulse_pod", barrel(45, 15, RED), "weapon", 22)]
    elif module == "guardian":
        rig.parts += [Part("shield_left", shield(32, 58, BLUE), "left", -14), Part("shield_right", shield(32, 58, BLUE), "right", 14)]
    else:
        rig.bones += [Bone("tool", "weapon", 24, 0, -20, 25)]
        rig.parts += [Part("repair_upper", limb(34, 12, LIGHT, GREEN), "weapon", 17), Part("repair_claw", claw(38, 32, GREEN), "tool", 28)]
    rig.animations = base_anims({
        "deploy": {"bones": {"body": {"scale": [{"time": 0, "x": .1, "y": .1}, {"time": .5, "x": 1.1, "y": 1.1}, {"time": .75, "x": 1, "y": 1}], "rotate": [{"time": 0, "angle": -120}, {"time": .75, "angle": 0}]}}},
        "follow": {"bones": {"body": {"translate": [{"time": 0, "y": -2}, {"time": .45, "y": 2}, {"time": .9, "y": -2}]}}},
        "shield": {"bones": {"left": {"rotate": [{"time": 0, "angle": 0}, {"time": .2, "angle": -30}]}, "right": {"rotate": [{"time": 0, "angle": 0}, {"time": .2, "angle": 30}]}}},
        "repair": {"bones": {"tool": {"rotate": [{"time": 0, "angle": -15}, {"time": .3, "angle": 18}, {"time": .6, "angle": -15}]}}},
        "rebuild": {"bones": {"body": {"scale": [{"time": 0, "x": 0, "y": 0}, {"time": .7, "x": 1.15, "y": .7}, {"time": 1, "x": 1, "y": 1}]}}},
    })
    return rig


def pack(rig: Rig):
    atlas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    regions = {}
    x = y = 4
    row = 0
    for part in rig.parts:
        w, h = part.image.size
        if x + w + 4 > SIZE:
            x, y, row = 4, y + row + 4, 0
        if y + h + 4 > SIZE:
            raise ValueError(f"{rig.name}: atlas overflow")
        atlas.alpha_composite(part.image, (x, y))
        regions[part.name] = (x, y, w, h)
        x += w + 4
        row = max(row, h)
    return atlas, regions


def spine_json(rig: Rig, regions):
    bones = [{"name": "root"}]
    for bone in rig.bones:
        row = {"name": bone.name, "parent": bone.parent or "root"}
        for key, value in (("x", bone.x), ("y", bone.y), ("rotation", bone.rotation), ("length", bone.length)):
            if value:
                row[key] = value
        bones.append(row)
    slots, skin = [], {}
    for part in rig.parts:
        _x, _y, w, h = regions[part.name]
        slots.append({"name": part.name, "bone": part.bone, "attachment": part.name})
        attachment = {"name": part.name, "width": w, "height": h}
        if part.x: attachment["x"] = part.x
        if part.y: attachment["y"] = part.y
        if part.rotation: attachment["rotation"] = part.rotation
        skin[part.name] = {part.name: attachment}
    return {"skeleton": {"hash": "ninslash-native-parts", "spine": "3.6.36", "width": 180, "height": 150, "images": ""}, "bones": bones, "slots": slots, "skins": {"default": skin}, "animations": rig.animations}


def atlas_text(rig: Rig, regions):
    lines = [f"lost_protocol/{rig.name}.png", f"size: {SIZE},{SIZE}", "format: RGBA8888", "filter: Linear,Linear", "repeat: none"]
    for name, (x, y, w, h) in regions.items():
        lines += [name, "  rotate: false", f"  xy: {x}, {y}", f"  size: {w}, {h}", f"  orig: {w}, {h}", "  offset: 0, 0", "  index: -1"]
    return "\n".join(lines) + "\n"


def build(rig: Rig):
    atlas, regions = pack(rig)
    atlas.save(OUT / f"{rig.name}.png")
    (OUT / f"{rig.name}.json").write_text(json.dumps(spine_json(rig, regions), separators=(",", ":")) + "\n", encoding="utf-8")
    (OUT / f"{rig.name}.atlas").write_text(atlas_text(rig, regions), encoding="utf-8")
    print(f"built {rig.name}: {len(rig.parts)} attachments, {len(rig.bones) + 1} bones")


def preview(rig: Rig, path: Path) -> None:
    world = {"root": (0.0, 0.0, 0.0)}
    for bone in rig.bones:
        px, py, prot = world[bone.parent or "root"]
        radians = math.radians(prot)
        wx = px + math.cos(radians) * bone.x - math.sin(radians) * bone.y
        wy = py + math.sin(radians) * bone.x + math.cos(radians) * bone.y
        world[bone.name] = (wx, wy, prot + bone.rotation)
    output = Image.new("RGBA", (640, 420), (35, 38, 43, 255))
    scale = 1.6 if "siege" in rig.name or "overseer" in rig.name else 2.0
    origin = (250, 235)
    for part in rig.parts:
        bx, by, brot = world[part.bone]
        radians = math.radians(brot)
        cx = bx + math.cos(radians) * part.x - math.sin(radians) * part.y
        cy = by + math.sin(radians) * part.x + math.cos(radians) * part.y
        sprite = part.image.resize((max(1, round(part.image.width * scale)), max(1, round(part.image.height * scale))), Image.Resampling.LANCZOS)
        sprite = sprite.rotate(-brot - part.rotation, expand=True, resample=Image.Resampling.BICUBIC)
        x = round(origin[0] + cx * scale - sprite.width / 2)
        y = round(origin[1] - cy * scale - sprite.height / 2)
        output.alpha_composite(sprite, (x, y))
    path.parent.mkdir(parents=True, exist_ok=True)
    output.save(path)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    rigs = [
        walker_rig("bulwark", LIGHT, BLUE, "shield"),
        walker_rig("assembler", LIGHT, GREEN, "repair"),
        saboteur_rig(),
        walker_rig("railgunner", LIGHT, RED, "rail"),
        walker_rig("siege_engine", LIGHT, RED, "siege", True),
        overseer_rig(),
        drone_rig("pve_drone_assault", "assault"),
        drone_rig("pve_drone_guardian", "guardian"),
        drone_rig("pve_drone_repair", "repair"),
    ]
    for rig in rigs:
        build(rig)
        preview(rig, ROOT / "build-win" / "art-preview" / f"{rig.name}.png")


if __name__ == "__main__":
    main()
