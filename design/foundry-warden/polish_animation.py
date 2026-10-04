"""Author and bake the Warden motion pass. No generation API or credentials.

python design/foundry-warden/polish_animation.py
Uses the Sunburst texture; exports native region animations at 60 Hz with
error-bounded key reduction. The original rig and texture remain untouched.
"""

from __future__ import annotations

import base64
import copy
import json
import math
import re
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
ASSET = ROOT / "data/anim/foundry_warden"
SOURCE = HERE / "source"
STEM = "warden-motion-v2"
FPS = 60
FOOT_Y = 10.5
TAU = math.tau
IDENTITY = (1.0, 0.0, 0.0, 1.0, 0.0, 0.0)
BASE = json.loads((SOURCE / "reference-rig.json").read_text(encoding="utf-8"))
RIG = copy.deepcopy(BASE)
RIG["animations"] = {}
SKIN = RIG["skins"]["default"]
INDEX = {b["name"]: b for b in RIG["bones"]}
TEXTURE = Image.open(SOURCE / "sunburst.png").convert("RGBA")
RECTS = {}
for m in re.finditer(
    r"\n([^\n ]+)\n  rotate: false\n  xy: (\d+), (\d+)\n  size: (\d+), (\d+)",
    (SOURCE / "sunburst.atlas").read_text(),
):
    RECTS[m[1]] = tuple(map(int, m.groups()[1:]))


def mul(a, b):
    return (
        a[0] * b[0] + a[2] * b[1],
        a[1] * b[0] + a[3] * b[1],
        a[0] * b[2] + a[2] * b[3],
        a[1] * b[2] + a[3] * b[3],
        a[0] * b[4] + a[2] * b[5] + a[4],
        a[1] * b[4] + a[3] * b[5] + a[5],
    )


def mat(x=0, y=0, angle=0, sx=1, sy=1):
    a = math.radians(angle)
    c, s = math.cos(a), math.sin(a)
    return c * sx, s * sx, -s * sy, c * sy, x, y


def inverse_point(m, x, y):
    a, b, c, d, tx, ty = m
    det = a * d - b * c
    x -= tx
    y -= ty
    return ((d * x - c * y) / det, (-b * x + a * y) / det)


def angle_delta(a, b):
    return (a - b + 180) % 360 - 180


def smooth(x):
    x = max(0, min(1, x))
    return x * x * x * (x * (x * 6 - 15) + 10)


def c(t, keys):
    """An authored scalar channel; outgoing interpolation belongs to its key."""
    if t <= keys[0][0]:
        return keys[0][1]
    if t >= keys[-1][0]:
        return keys[-1][1]
    for a, b in zip(keys, keys[1:]):
        if a[0] <= t <= b[0]:
            f = (t - a[0]) / (b[0] - a[0])
            mode = a[2] if len(a) > 2 else "smooth"
            if mode == "smooth":
                f = smooth(f)
            elif mode == "in":
                f = f**3
            elif mode == "out":
                f = 1 - (1 - f) ** 3
            elif mode == "hold":
                f = 0
            return a[1] + (b[1] - a[1]) * f
    raise ValueError("Unreachable curve interval")


def ring(t, at, amplitude, frequency=5, damping=9):
    age = t - at
    return (
        amplitude * math.exp(-damping * age) * math.sin(TAU * frequency * age)
        if age >= 0
        else 0
    )


def recoil(t, at, amplitude, rate=13):
    age = t - at
    return (
        -amplitude * 1.5 * (1 - math.exp(-160 * age)) * math.exp(-rate * age)
        if age >= 0
        else 0
    )


def add_bone(name, parent, x=0, y=0, rotation=0, length=0):
    b = dict(name=name, parent=parent, x=x, y=y, rotation=rotation, length=length)
    RIG["bones"].append(b)
    INDEX[name] = b


def add_slot(name, bone, region, x=0, y=0, width=1, height=1, **extra):
    RIG["slots"].append(dict(name=name, bone=bone, attachment=region))
    SKIN[name] = {
        region: dict(type="region", x=x, y=y, width=width, height=height, **extra)
    }


def attachment(slot):
    return next(iter(SKIN[slot].values()))


def set_region(slot, new):
    old = next(s for s in RIG["slots"] if s["name"] == slot)
    old["attachment"] = new
    SKIN[slot] = {new: attachment(slot)}


def crop(name):
    x, y, w, h = RECTS[name]
    return TEXTURE.crop((x, y, x + w, y + h))


# Derive mechanical subdivisions from the SAME generated art. The engine does
# not render Spine slot tint, so rear-layer shading is baked into small regions.
ADDED = {}
for name in ["eye", "core"]:
    im = crop(name)
    w, h = im.size
    mask = Image.new("L", im.size)
    draw = ImageDraw.Draw(mask)
    r = w * (0.34 if name == "eye" else 0.365)
    draw.ellipse((w / 2 - r, h / 2 - r, w / 2 + r, h / 2 + r), fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(1.1))
    dark = Image.new("RGBA", im.size, (15, 23, 26, 255))
    housing = Image.composite(dark, im, mask)
    housing.putalpha(im.getchannel("A"))
    glow = im.copy()
    glow.putalpha(ImageChops.multiply(mask, im.getchannel("A")))
    ADDED[name + "_housing"] = housing
    ADDED[name + "_glow"] = glow
foot = crop("foot")
ADDED["heel"] = foot.crop((0, 0, 80, 58))
ADDED["toe"] = foot.crop((80, 0, 124, 58))
for name in ["upper_link", "shin", "heel", "toe"]:
    im = ADDED[name].copy() if name in ADDED else crop(name)
    a = im.getchannel("A")
    rgb = im.convert("RGB").point(lambda v: round(v * 0.68))
    rgb.putalpha(a)
    ADDED["rear_" + name] = rgb
x, y, row = 4, 704, 0
for name, im in sorted(ADDED.items(), key=lambda pair: (-pair[1].height, pair[0])):
    w, h = im.size
    if x + w + 4 > 1024:
        x, y, row = 4, y + row + 8, 0
    if y + h + 4 > 1024:
        raise ValueError("Additional regions exceed the atlas")
    TEXTURE.paste(im, (x, y))
    RECTS[name] = (x, y, w, h)
    x += w + 8
    row = max(row, h)

# A load-bearing pelvis plus a suspended upper body removes the single-cardboard
# torso behavior. Existing parent-local attachment positions are preserved.
add_bone("pelvis", "root", 0, 112)
INDEX["chassis"].update(parent="pelvis", y=24)
LEGS = {}
for name in ["rear_l", "rear_r", "front_l", "front_r"]:
    side = -1 if name.endswith("l") else 1
    hip = INDEX[name + "_hip"]
    hip["parent"] = "pelvis"
    hip["y"] += 24
    l1 = hip["length"]
    l2 = INDEX[name + "_knee"]["length"]
    LEGS[name] = dict(
        hip=(hip["x"], hip["y"]),
        l1=l1,
        l2=l2,
        side=side,
        rest=(side * (142 if name.startswith("rear") else 103), FOOT_Y),
    )


def solve_leg(spec, target):
    hx, hy = spec["hip"]
    x, y = target[0] - hx, target[1] - hy
    l1, l2 = spec["l1"], spec["l2"]
    co = max(
        -0.99999, min(0.99999, (x * x + y * y - l1 * l1 - l2 * l2) / (2 * l1 * l2))
    )
    b = math.acos(co) * -spec["side"]
    a = math.atan2(y, x) - math.atan2(l2 * math.sin(b), l1 + l2 * math.cos(b))
    return math.degrees(a), math.degrees(b)


for name, s in LEGS.items():
    a, b = solve_leg(s, (s["rest"][0], FOOT_Y - 112))
    INDEX[name + "_hip"]["rotation"] = a
    INDEX[name + "_knee"]["rotation"] = b
    INDEX[name + "_foot"]["rotation"] = -a - b
    sign = s["side"]
    add_bone(
        name + "_toe", name + "_foot", sign * (80 / 124 - 0.5) * 49, 0, length=17.3871
    )
    set_region(name + "_pad", "rear_heel" if name.startswith("rear") else "heel")
    afoot = attachment(name + "_pad")
    afoot.update(x=sign * (-24.5 + 80 / 124 * 49 / 2), width=80 / 124 * 49, scaleX=sign)
    add_slot(
        name + "_toe",
        name + "_toe",
        "rear_toe" if name.startswith("rear") else "toe",
        sign * (44 / 124 * 49 / 2),
        -1,
        44 / 124 * 49,
        25,
        scaleX=sign,
    )
    if name.startswith("rear"):
        set_region(name + "_upper", "rear_upper_link")
        set_region(name + "_lower", "rear_shin")

# A relaxed, slightly asymmetric fighting stance replaces the horizontal T pose.
INDEX["arm_l"]["rotation"] = 198
INDEX["arm_r"]["rotation"] = -24
INDEX["forearm_l"]["rotation"] = -16
INDEX["forearm_r"]["rotation"] = 22
INDEX["claw_l"]["rotation"] = 4
INDEX["claw_r"]["rotation"] = -4
for side, sgn in [("l", -1), ("r", 1)]:
    INDEX["finger_upper_" + side]["rotation"] = 28
    INDEX["finger_lower_" + side]["rotation"] = -28
    add_bone("guard_" + side, "arm_" + side, 34, 0)
    next(s for s in RIG["slots"] if s["name"] == "arm_guard_" + side)["bone"] = (
        "guard_" + side
    )
    attachment("arm_guard_" + side)["x"] = 0
    add_bone("piston_" + side, "chassis", sgn * 89, 1, length=49)
    add_bone("rod_" + side, "piston_" + side, 29, 0, length=27)
    add_slot("piston_" + side, "piston_" + side, "upper_link", 21, 0, 53, 13)
    add_slot("rod_" + side, "rod_" + side, "upper_link", 11, 0, 34, 7)
    add_bone("muzzle_" + side, "cannon_" + side, 73, 0, length=8)
    add_bone("claw_tip_" + side, "claw_" + side, 94, 0, length=5)
add_bone("sensor_glow", "eye")
add_bone("core_rotor", "core")
set_region("eye", "eye_housing")
set_region("core", "core_housing")
add_slot("sensor_glow", "sensor_glow", "eye_glow", width=55, height=55)
add_slot("core_rotor", "core_rotor", "core_glow", width=84, height=84)

# Resolve parent order, then place the added slots at their correct depth.
ordered = []
seen = set()


def visit(b):
    if b["name"] in seen:
        return
    if b.get("parent"):
        visit(INDEX[b["parent"]])
    ordered.append(b)
    seen.add(b["name"])


for b in RIG["bones"]:
    visit(b)
RIG["bones"] = ordered


def move_slot(name, after=None, before=None):
    item = next(s for s in RIG["slots"] if s["name"] == name)
    RIG["slots"].remove(item)
    at = next(i for i, s in enumerate(RIG["slots"]) if s["name"] == (after or before))
    RIG["slots"].insert(at + (1 if after else 0), item)


for name in LEGS:
    move_slot(name + "_toe", after=name + "_pad")
for side in ["l", "r"]:
    move_slot("piston_" + side, before="arm_link_" + side)
    move_slot("rod_" + side, before="piston_" + side)
move_slot("sensor_glow", after="eye")
move_slot("core_rotor", after="core")

META = copy.deepcopy(
    json.loads((SOURCE / "reference-timing.json").read_text(encoding="utf-8"))
)
META.update(
    version=2,
    baked_fps=FPS,
    foot_contact_height=FOOT_Y,
    rig_bones=len(RIG["bones"]),
    rig_slots=len(RIG["slots"]),
    atlas_regions=len(RECTS),
    status="polished_animation_assets_not_registered_in_game",
)
META["sockets"].update(
    left_muzzle="muzzle_l", right_muzzle="muzzle_r", claw_tip="claw_tip_r"
)
META["clips"] = {
    "idle": dict(
        label="承重待机",
        duration=3.6,
        loop=True,
        hint="底盘承重，上身悬挂缓动；独眼巡视，钳指错相收放。",
    ),
    "walk": dict(
        label="四足重步",
        duration=1.4,
        loop=True,
        stride=44,
        stance=0.68,
        hint="对角承重、脚尖先离地，足底落稳后身体才跟进。地面同步滚动以展示足锁定。",
    ),
    "clamp_sweep": dict(
        label="液压钳击",
        duration=2.05,
        loop=False,
        tell=0.64,
        impact=0.775,
        active_end=0.9,
        hit_hold=0.045,
        hint="重心反向蓄力 → 肩肘甩出 → 钳指闭合 → 冲击停顿 → 惯性回收。",
    ),
    "mortar_barrage": dict(
        label="交替迫击",
        duration=2.8,
        loop=False,
        tell=0.72,
        active_end=1.61,
        shots=[0.72, 1.08, 1.44],
        shot_sides=["r", "l", "r"],
        hint="左右炮交替开火；炮口先亮、炮管后坐，再传递到支架和躯干。",
    ),
    "furnace_slam": dict(
        label="跃起重踏",
        duration=2.5,
        loop=False,
        tell=1.12,
        impact=1.12,
        active_end=1.39,
        hit_hold=0.06,
        hint="压低重心，离地收腿；落地先压底盘，再压上身，护板与炮管最后收住。",
    ),
    "vent": dict(
        label="解锁散热",
        duration=3.1,
        loop=False,
        weak_start=0.52,
        weak_end=2.55,
        hint="先卸压，左右胸甲错开解锁；炉芯独立旋转，排气结束后依次合甲。",
    ),
    "stagger": dict(
        label="受击传导",
        duration=0.85,
        loop=False,
        impact=0.09,
        hint="受击从躯干传到肩甲与钳臂；足部稳住，弹性逐步衰减。",
    ),
    "enrage": dict(
        label="过载启动",
        duration=2.2,
        loop=False,
        hint="压低身体、锁定双钳，炉芯加速；护甲和散热鳍依次响应过载。",
    ),
    "death": dict(
        label="失压瘫毁",
        duration=3.6,
        loop=False,
        impacts=[0.78, 1.56],
        hint="先失衡，再失压折膝；炮管与钳臂分段落下，独眼最后熄灭。",
    ),
}
META["motion_pass"] = {
    "features": [
        "independent pelvis and upper suspension",
        "baked planted-foot IK",
        "toe roll and alternating support",
        "telescoping hydraulic links",
        "overlapping secondary motion",
        "split luminous rotors",
        "authored easing and contact holds",
    ],
    "preview_only": [
        "clip crossfade",
        "impact particles",
        "camera kick",
        "emissive glow",
        "contact shadows",
        "walk ground scroll",
    ],
}


def world_pose(delta):
    world = {}
    for b in RIG["bones"]:
        q = delta.get(b["name"], {})
        p = q.get("translate", (0, 0))
        s = q.get("scale", (1, 1))
        r = q.get("rotate", 0)
        world[b["name"]] = mul(
            world.get(b.get("parent"), IDENTITY),
            mat(
                b.get("x", 0) + p[0],
                b.get("y", 0) + p[1],
                b.get("rotation", 0) + r,
                s[0],
                s[1],
            ),
        )
    return world


def authored(clip, t):
    duration = META["clips"][clip]["duration"]
    d = {}

    def r(name, value):
        d.setdefault(name, {})["rotate"] = value

    def p(name, x=0, y=0):
        d.setdefault(name, {})["translate"] = (x, y)

    def s(name, x=1, y=None):
        d.setdefault(name, {})["scale"] = (x, x if y is None else y)

    # Idle motion fades out during attacks, so the recoil never competes with an
    # unrelated sinusoidal bob. All offsets return to the same fighting stance.
    ambient = 1 if clip in ("idle", "walk") else (1 - smooth(t / 0.22)) * 0.5
    phase = TAU * t / duration
    p("pelvis", 0.6 * math.sin(phase) * ambient, 1.1 * math.sin(phase) * ambient)
    p("chassis", 0, 1.6 * math.sin(phase - 0.3) * ambient)
    r("chassis", 0.55 * math.sin(phase) * ambient)
    for side, sign in [("l", -1), ("r", 1)]:
        delay = 0.23 if side == "l" else -0.21
        r("arm_" + side, sign * 1.8 * math.sin(phase + delay) * ambient)
        r("forearm_" + side, -sign * 2.3 * math.sin(phase + delay - 0.35) * ambient)
        r("claw_" + side, sign * 1.1 * math.sin(phase + delay - 0.6) * ambient)
        r("finger_upper_" + side, 2.4 * math.sin(phase + delay - 0.4) * ambient)
        r("finger_lower_" + side, -1.8 * math.sin(phase + delay - 0.8) * ambient)
        r("guard_" + side, sign * 0.7 * math.sin(phase - 0.6) * ambient)
    s("sensor_glow", 0.97 + 0.03 * math.cos(phase))
    if clip == "idle":
        look = c(
            t,
            [
                (0, 0),
                (0.5, 0),
                (0.92, 2.4),
                (1.45, 2.4),
                (2.05, -2),
                (2.6, -2),
                (3.3, 0),
                (3.6, 0),
            ],
        )
        p("eye", look, abs(look) * 0.2)
        r("sensor_glow", 12 * math.sin(phase))
        for i, name in enumerate(["vent_l", "vent_mid", "vent_r"]):
            p(name, 0, 0.8 * math.sin(phase - i * 0.65))
            r(name, 0.6 * math.sin(phase - i * 0.8))
    elif clip == "walk":
        p("pelvis", 1.5 * math.sin(phase), -1.6 + 1.6 * math.cos(phase * 2))
        r("pelvis", 0.8 * math.sin(phase))
        p("chassis", 0, 1.8 * math.cos(phase * 2 - 0.5))
        r("chassis", -1.1 * math.sin(phase - 0.3))
        for side, sign in [("l", -1), ("r", 1)]:
            r("arm_" + side, sign * 4.8 * math.sin(phase - 0.25))
            r("forearm_" + side, -sign * 5.3 * math.sin(phase - 0.8))
            r("claw_" + side, sign * 3.5 * math.sin(phase - 1.05))
            r("guard_" + side, sign * 1.8 * math.sin(phase - 0.7))
            r("cannon_mount_" + side, sign * 1.0 * math.sin(phase * 2 - 0.8))
    elif clip == "clamp_sweep":
        p(
            "pelvis",
            c(
                t,
                [
                    (0, 0),
                    (0.48, -7),
                    (0.64, -8, "in"),
                    (0.775, 8),
                    (0.82, 8),
                    (1.13, 5),
                    (1.85, 0),
                ],
            ),
            c(
                t,
                [
                    (0, 0),
                    (0.48, -5),
                    (0.64, -6),
                    (0.775, -11),
                    (0.82, -11),
                    (1.12, -5),
                    (1.85, 0),
                ],
            ),
        )
        r("pelvis", c(t, [(0, 0), (0.6, -1.5), (0.775, 2), (0.82, 2), (1.9, 0)]))
        r(
            "chassis",
            c(
                t,
                [
                    (0, 0),
                    (0.52, -6),
                    (0.64, -6, "in"),
                    (0.775, 7),
                    (0.82, 7),
                    (1.1, 3),
                    (1.9, 0),
                ],
            ),
        )
        p("chassis", 0, ring(t, 0.82, 3.5, 5, 9))
        r(
            "arm_r",
            c(
                t,
                [
                    (0, 0),
                    (0.18, 9),
                    (0.5, 76),
                    (0.64, 76, "in"),
                    (0.775, -29),
                    (0.82, -29),
                    (0.99, -35),
                    (1.36, -9),
                    (1.94, 0),
                ],
            ),
        )
        r(
            "forearm_r",
            c(
                t,
                [
                    (0, 0),
                    (0.28, 12),
                    (0.55, 30),
                    (0.64, 30, "in"),
                    (0.775, -10),
                    (0.82, -10),
                    (1.02, -5),
                    (1.4, 12),
                    (1.98, 0),
                ],
            ),
        )
        r(
            "claw_r",
            c(
                t,
                [
                    (0, 0),
                    (0.31, -5),
                    (0.6, -52),
                    (0.68, -52, "in"),
                    (0.775, 39),
                    (0.82, 39),
                    (1.06, 45),
                    (1.5, -5),
                    (2.05, 0),
                ],
            ),
        )
        r(
            "arm_l",
            c(t, [(0, 0), (0.5, 9), (0.64, 9), (0.82, -12), (1.3, -8), (1.9, 0)]),
        )
        r("forearm_l", c(t, [(0, 0), (0.6, -14), (0.9, 11), (1.4, 6), (2, 0)]))
        for upper, sign in [("upper", 1), ("lower", -1)]:
            r(
                "finger_" + upper + "_r",
                sign
                * c(
                    t,
                    [
                        (0, 0),
                        (0.43, 21),
                        (0.68, 21, "in"),
                        (0.775, -19),
                        (0.87, -19),
                        (1.13, -13),
                        (1.76, 0),
                    ],
                ),
            )
        r("guard_r", ring(t, 0.775, -10, 6, 8))
        r("cannon_mount_r", ring(t, 0.81, 5, 4.3, 7))
        r("cannon_mount_l", ring(t, 0.85, 3, 4, 8))
    elif clip == "mortar_barrage":
        brace = c(t, [(0, 0), (0.45, 1), (1.63, 1), (2.7, 0)])
        p("pelvis", 0, -8 * brace)
        p("chassis", 0, -2 * brace)
        for side, sign in [("l", -1), ("r", 1)]:
            shots = [
                at
                for at, which in zip(
                    META["clips"][clip]["shots"], META["clips"][clip]["shot_sides"]
                )
                if which == side
            ]
            back = sum(recoil(t, at, 13) for at in shots)
            p("cannon_" + side, back, 0)
            r(
                "cannon_mount_" + side,
                -sign * 12 * brace
                + sum(ring(t, at, sign * 3.1, 6, 13) for at in shots),
            )
            r("arm_" + side, sign * -8 * brace)
            r("forearm_" + side, sign * 12 * brace)
            r(
                "guard_" + side,
                sum(ring(t, at + 0.035, sign * 3, 8, 13) for at in shots),
            )
        impact = sum(
            recoil(t, at + 0.025, 3, 15) for at in META["clips"][clip]["shots"]
        )
        p("chassis", 0, -2 * brace + impact)
        r(
            "chassis",
            sum(
                ring(t, at + 0.02, 1.4 if which == "r" else -1.4, 5, 12)
                for at, which in zip(
                    META["clips"][clip]["shots"], META["clips"][clip]["shot_sides"]
                )
            ),
        )
    elif clip == "furnace_slam":
        # Ballistic flight is separate from the landing compression. The pause
        # at contact is in the exported animation, not merely a camera effect.
        if 0.52 <= t < 1.12:
            u = (t - 0.52) / 0.60
            rise = 4 * 86 * u * (1 - u)
        else:
            rise = 0
        crouch = c(
            t,
            [
                (0, 0),
                (0.34, -22),
                (0.48, -22),
                (0.52, 0),
                (1.105, 0),
                (1.12, -19),
                (1.18, -19),
                (1.31, 4),
                (1.51, -2),
                (2.3, 0),
            ],
        )
        p("pelvis", 0, rise + crouch)
        p(
            "chassis",
            0,
            c(
                t,
                [
                    (0, 0),
                    (0.38, -5),
                    (0.52, -5),
                    (0.65, 3),
                    (0.98, 2),
                    (1.12, 0),
                    (1.18, -8),
                    (1.25, -8),
                    (1.4, 4),
                    (1.72, -1),
                    (2.35, 0),
                ],
            ),
        )
        r(
            "chassis",
            c(
                t,
                [
                    (0, 0),
                    (0.48, -1),
                    (0.74, 2),
                    (1.0, 1),
                    (1.18, -2),
                    (1.4, 1),
                    (2.2, 0),
                ],
            ),
        )
        for side, sign in [("l", -1), ("r", 1)]:
            delay = 0.025 if side == "l" else 0
            tt = max(0, t - delay)
            r(
                "arm_" + side,
                sign
                * c(
                    tt,
                    [
                        (0, 0),
                        (0.38, -12),
                        (0.5, -12),
                        (0.71, 62),
                        (0.91, 62),
                        (1.12, -7),
                        (1.2, -7),
                        (1.45, 8),
                        (2.25, 0),
                    ],
                ),
            )
            r(
                "forearm_" + side,
                -sign
                * c(
                    tt,
                    [
                        (0, 0),
                        (0.48, -16),
                        (0.74, -16),
                        (0.98, -18),
                        (1.18, -3),
                        (1.48, -4),
                        (2.4, 0),
                    ],
                ),
            )
            r(
                "claw_" + side,
                sign
                * c(
                    tt,
                    [
                        (0, 0),
                        (0.5, -8),
                        (0.76, 18),
                        (1.06, 12),
                        (1.21, -8),
                        (1.45, 4),
                        (2.45, 0),
                    ],
                ),
            )
            r("guard_" + side, ring(t, 1.16, sign * 9, 6, 9))
            r("cannon_mount_" + side, ring(t, 1.19, -sign * 6, 4.5, 8))
            r(
                "finger_upper_" + side,
                c(
                    t,
                    [
                        (0, 0),
                        (0.48, -9),
                        (0.75, 15),
                        (0.96, 15),
                        (1.18, -12),
                        (1.43, -4),
                        (2.4, 0),
                    ],
                ),
            )
            r("finger_lower_" + side, -d["finger_upper_" + side]["rotate"])
    elif clip in ("vent", "enrage"):
        enraged = clip == "enrage"
        duration = META["clips"][clip]["duration"]
        close = duration - 0.63
        full = 0.62 if not enraged else 0.48
        open_l = c(
            t,
            [
                (0, 0),
                (0.18, 0),
                (0.26, 0.10),
                (full, 0.99),
                (close, 1),
                (duration - 0.13, 0),
                (duration, 0),
            ],
        )
        open_r = c(
            t,
            [
                (0, 0),
                (0.27, 0),
                (0.36, 0.10),
                (full + 0.12, 1),
                (close + 0.08, 1),
                (duration, 0),
            ],
        )
        for side, sign, opening in [("l", -1, open_l), ("r", 1, open_r)]:
            p("shell_" + side, sign * 32 * opening, 3 * opening)
            r(
                "shell_" + side,
                -sign * 14 * opening
                + ring(t, full + (0 if side == "l" else 0.12), sign * 1.4, 6, 12),
            )
            r("arm_" + side, sign * -6 * opening)
            r("forearm_" + side, sign * 8 * opening)
            r("claw_" + side, sign * 3 * opening)
            r("finger_upper_" + side, (-12 if enraged else 5) * opening)
            r("finger_lower_" + side, (12 if enraged else -5) * opening)
            r("cannon_mount_" + side, sign * 7 * opening)
        p("jaw", 0, -10 * max(open_l, open_r))
        p("pelvis", 0, (-10 if enraged else -5) * max(open_l, open_r))
        p(
            "chassis",
            0,
            ring(t, 0.52, 2, 5, 4)
            + (math.sin(t * TAU * 9) * 0.38 if enraged else 0) * open_l,
        )
        r("core_rotor", (720 if enraged else 360) * t / duration)
        glow = 1 + 0.035 * math.sin(t * TAU * (5 if enraged else 2.5)) * max(
            open_l, open_r
        )
        s("core_rotor", glow)
        for i, name in enumerate(["vent_l", "vent_mid", "vent_r"]):
            rise = c(
                t,
                [
                    (0, 0),
                    (0.16 + i * 0.045, 0),
                    (0.4 + i * 0.045, 5),
                    (close, 5),
                    (duration, 0),
                ],
            )
            p(name, 0, rise)
            r(name, ring(t, 0.4 + i * 0.045, 1.5, 7, 10))
    elif clip == "stagger":
        p(
            "pelvis",
            c(t, [(0, 0), (0.10, -4), (0.18, -4), (0.35, 1), (0.82, 0)]),
            c(t, [(0, 0), (0.13, -3), (0.3, -1), (0.85, 0)]),
        )
        r(
            "chassis",
            c(t, [(0, 0), (0.09, 7), (0.14, 7), (0.28, -3), (0.45, 1.2), (0.85, 0)]),
        )
        p("chassis", c(t, [(0, 0), (0.09, -6), (0.15, -6), (0.32, 2), (0.85, 0)]), 0)
        for side, sign in [("l", -1), ("r", 1)]:
            r("arm_" + side, ring(t, 0.12, -sign * 9, 3.5, 7))
            r("forearm_" + side, ring(t, 0.16, sign * 8, 4, 7))
            r("claw_" + side, ring(t, 0.19, -sign * 6, 4, 8))
            r("guard_" + side, ring(t, 0.11, -sign * 8, 8, 12))
            r("cannon_mount_" + side, ring(t, 0.13, sign * 4, 6, 10))
        s(
            "sensor_glow",
            1,
            c(t, [(0, 1), (0.09, 0.5), (0.16, 0.5), (0.33, 1.1), (0.85, 1)]),
        )
    elif clip == "death":
        p(
            "pelvis",
            c(
                t,
                [
                    (0, 0),
                    (0.38, -3),
                    (0.78, -7),
                    (1.2, -2),
                    (1.56, 3),
                    (2.1, 3),
                    (3.6, 3),
                ],
            ),
            c(
                t,
                [
                    (0, 0),
                    (0.11, 2),
                    (0.35, 0),
                    (0.78, -27),
                    (0.86, -27),
                    (1.02, -22),
                    (1.56, -58),
                    (1.66, -58),
                    (1.85, -54),
                    (2.3, -58),
                    (3.6, -58),
                ],
            ),
        )
        r(
            "pelvis",
            c(t, [(0, 0), (0.35, 3), (0.78, -5), (1.12, -3), (1.56, -6), (3.6, -6)]),
        )
        r(
            "chassis",
            c(
                t,
                [
                    (0, 0),
                    (0.10, 5),
                    (0.35, 3),
                    (0.82, -7),
                    (1.04, 2),
                    (1.62, 0),
                    (2.2, -1),
                    (3.6, -1),
                ],
            ),
        )
        p("chassis", 0, ring(t, 0.78, 3, 4, 7) + ring(t, 1.59, 3, 4, 6))
        for side, sign in [("l", -1), ("r", 1)]:
            delay = 0.18 if side == "l" else 0
            tt = max(0, t - delay)
            r(
                "arm_" + side,
                sign
                * c(
                    tt,
                    [
                        (0, 0),
                        (0.22, 14),
                        (0.52, 3),
                        (1.02, -2),
                        (1.65, 7),
                        (2.1, 5),
                        (3.6, 5),
                    ],
                ),
            )
            r(
                "forearm_" + side,
                sign * c(tt, [(0, 0), (0.4, -10), (1.04, 21), (1.65, 22), (3.6, 22)]),
            )
            r(
                "claw_" + side,
                -sign * c(tt, [(0, 0), (0.58, 4), (1.13, 13), (1.8, 9), (3.6, 9)]),
            )
            r(
                "finger_upper_" + side,
                c(tt, [(0, 0), (0.32, -7), (0.55, 0), (1.8, 8), (3.6, 8)]),
            )
            r("finger_lower_" + side, -d["finger_upper_" + side]["rotate"])
            r(
                "cannon_mount_" + side,
                sign
                * c(
                    tt,
                    [
                        (0, 0),
                        (0.36, 9),
                        (0.79, 28),
                        (1.1, 23),
                        (1.68, 44),
                        (1.98, 40),
                        (3.6, 40),
                    ],
                ),
            )
            p("cannon_" + side, c(tt, [(0, 0), (0.42, -4), (1.66, -14), (3.6, -14)]), 0)
            p(
                "shell_" + side,
                sign * c(tt, [(0, 0), (0.5, 5), (1.2, 10), (1.8, 15), (3.6, 15)]),
                0,
            )
            r(
                "guard_" + side,
                ring(t, 0.83, sign * 11, 5, 5) + ring(t, 1.64, sign * 7, 6, 7),
            )
        dying = c(
            t,
            [
                (0, 1),
                (0.42, 1),
                (0.5, 0.35),
                (0.56, 1),
                (0.64, 0.25),
                (0.73, 0.8),
                (1.36, 0.35),
                (1.44, 0.7),
                (1.62, 0.02),
                (3.6, 0.02),
            ],
        )
        s("sensor_glow", dying)
        s("core_rotor", max(0.02, dying * 0.8))
    # Solve planted feet after BOTH body channels have been authored. During
    # flight, foot targets follow a tuck arc; on ground they remain in world space.
    world = world_pose(d)
    pelvis = world["pelvis"]
    for name, spec in LEGS.items():
        tx, ty = spec["rest"]
        roll = 0
        toe = 0
        if clip == "walk":
            u = (t / duration + (0 if name in ("rear_l", "front_r") else 0.5)) % 1
            if u < 0.68:
                tx += 22 - 44 * u / 0.68
                release = smooth((u - 0.57) / 0.11)
                roll = -spec["side"] * 6 * release
                toe = spec["side"] * 6 * release
                # The toe stays on the floor as the heel rolls off it.
                ty += abs(INDEX[name + "_toe"]["x"]) * math.sin(math.radians(abs(roll)))
            else:
                q = (u - 0.68) / 0.32
                tx += -22 + 44 * smooth(q)
                ty += 21 * math.sin(math.pi * q) ** 1.5
                roll = spec["side"] * c(q, [(0, -6), (0.35, -16), (0.75, 8), (1, 0)])
                toe = spec["side"] * c(q, [(0, 6), (0.3, 23), (0.72, 5), (1, 0)])
                ty += (
                    abs(INDEX[name + "_toe"]["x"])
                    * math.sin(math.radians(6))
                    * (1 - smooth(q / 0.18))
                )
        elif clip == "furnace_slam" and 0.52 < t < 1.12:
            u = (t - 0.52) / 0.60
            tuck = math.sin(math.pi * u)
            tx *= 1 - 0.18 * tuck
            ty += 4 * 86 * u * (1 - u) + 24 * tuck
            roll = -spec["side"] * 19 * tuck
            toe = -spec["side"] * 22 * tuck
        elif clip == "death":
            tx *= 1 + 0.035 * smooth(t / 1.8)
        target = inverse_point(pelvis, tx, ty)
        a, b = solve_leg(spec, target)
        r(name + "_hip", angle_delta(a, INDEX[name + "_hip"]["rotation"]))
        r(name + "_knee", angle_delta(b, INDEX[name + "_knee"]["rotation"]))
        body_angle = math.degrees(math.atan2(pelvis[1], pelvis[0]))
        r(
            name + "_foot",
            angle_delta(roll - body_angle - a - b, INDEX[name + "_foot"]["rotation"]),
        )
        r(name + "_toe", toe)
    # Aiming/scaling each hydraulic shaft from its actual anchors keeps the
    # sleeve and sliding rod attached through the complete attack arc.
    world = world_pose(d)
    for side in ["l", "r"]:
        target = world["forearm_" + side]
        point = inverse_point(world["chassis"], target[4], target[5])
        mount = INDEX["piston_" + side]
        dx, dy = point[0] - mount["x"], point[1] - mount["y"]
        distance = math.hypot(dx, dy)
        r("piston_" + side, math.degrees(math.atan2(dy, dx)))
        p("rod_" + side, distance - 55, 0)
    return d


def reduce_keys(times, values, epsilon, rotation=False):
    kept = {0, len(times) - 1}

    def simplify(lo, hi):
        if hi - lo <= 1:
            return
        dt = times[hi] - times[lo]
        worst = 0
        at = lo
        for i in range(lo + 1, hi):
            f = (times[i] - times[lo]) / dt
            error = max(
                abs(
                    values[i][j] - (values[lo][j] + (values[hi][j] - values[lo][j]) * f)
                )
                for j in range(len(values[i]))
            )
            if error > worst:
                worst, at = error, i
        if worst > epsilon or (rotation and abs(values[hi][0] - values[lo][0]) > 70):
            if worst <= epsilon:
                at = (lo + hi) // 2
            kept.add(at)
            simplify(lo, at)
            simplify(at, hi)

    simplify(0, len(times) - 1)
    return sorted(kept)


def build():
    total_keys = 0
    ground_errors = {}
    for clip, info in META["clips"].items():
        duration = info["duration"]
        # Timing landmarks are sampled exactly, including the contact holds.
        count = round(duration * FPS)
        times = {round(i * duration / count, 6) for i in range(count + 1)}
        times.update(
            float(info[k])
            for k in ("tell", "impact", "active_end", "weak_start", "weak_end")
            if k in info
        )
        times.update(info.get("shots", []))
        times.update(info.get("impacts", []))
        if "impact" in info and "hit_hold" in info:
            times.add(info["impact"] + info["hit_hold"])
        times = sorted(times)
        poses = [authored(clip, t) for t in times]
        tracks = {}
        for name in INDEX:
            for channel, default, eps in [
                ("rotate", 0, 0.07),
                ("translate", (0, 0), 0.055),
                ("scale", (1, 1), 0.0008),
            ]:
                v = [p.get(name, {}).get(channel, default) for p in poses]
                values = [(x,) if channel == "rotate" else x for x in v]
                reference = (default,) if channel == "rotate" else default
                if all(
                    max(abs(a - b) for a, b in zip(value, reference)) < 0.00001
                    for value in values
                ):
                    continue
                # Unwrap only periodic, non-spinning channels. Rotor spins are
                # intentional and retain intermediate <70 degree keys.
                if channel == "rotate" and name not in ("core_rotor",):
                    values = [values[0]] + values[1:]
                    for i in range(1, len(values)):
                        values[i] = (
                            values[i - 1][0]
                            + angle_delta(values[i][0], values[i - 1][0]),
                        )
                indices = reduce_keys(times, values, eps, channel == "rotate")
                keys = []
                for i in indices:
                    key = {"time": times[i]}
                    if channel == "rotate":
                        key["angle"] = round(values[i][0], 5)
                    else:
                        key.update(x=round(values[i][0], 5), y=round(values[i][1], 5))
                    keys.append(key)
                total_keys += len(keys)
                tracks.setdefault(name, {})[channel] = keys
        RIG["animations"][clip] = {"bones": tracks}
        error = 0
        for t, pose in zip(times, poses):
            if clip == "furnace_slam" and 0.52 < t < 1.12:
                continue
            world = world_pose(pose)
            for name in LEGS:
                if clip == "walk":
                    phase = (
                        t / duration + (0 if name in ("rear_l", "front_r") else 0.5)
                    ) % 1
                    if phase >= 0.68:
                        continue
                error = max(error, abs(world[name + "_toe"][5] - FOOT_Y))
        ground_errors[clip] = round(error, 4)
    if max(ground_errors.values()) > 1.0:
        raise ValueError(f"Foot constraint exceeded: {ground_errors}")
    TEXTURE.save(ASSET / (STEM + ".png"), optimize=True)
    lines = [
        f"foundry_warden/{STEM}.png",
        "size: 1024,1024",
        "format: RGBA8888",
        "filter: Linear,Linear",
        "repeat: none",
    ]
    for name, (x, y, w, h) in RECTS.items():
        lines.extend(
            [
                name,
                "  rotate: false",
                f"  xy: {x}, {y}",
                f"  size: {w}, {h}",
                f"  orig: {w}, {h}",
                "  offset: 0, 0",
                "  index: -1",
            ]
        )
    (ASSET / (STEM + ".atlas")).write_text("\n".join(lines) + "\n", encoding="utf-8")
    (ASSET / (STEM + ".json")).write_text(
        json.dumps(RIG, ensure_ascii=False, separators=(",", ":")) + "\n",
        encoding="utf-8",
    )
    (ASSET / (STEM + ".combat.json")).write_text(
        json.dumps(META, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    report = dict(
        bones=len(RIG["bones"]),
        slots=len(RIG["slots"]),
        regions=len(RECTS),
        animations=len(RIG["animations"]),
        baked_fps=FPS,
        exported_keys=total_keys,
        max_planted_foot_height_error=ground_errors,
    )
    (HERE / "motion-validation.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    template = HERE / "preview-motion.template.html"
    if template.exists():
        old_meta = json.loads(
            (SOURCE / "reference-timing.json").read_text(encoding="utf-8")
        )
        payload = dict(
            rig=RIG,
            meta=META,
            previous=BASE,
            previousMeta=old_meta,
            regions=RECTS,
            texture="data:image/png;base64,"
            + base64.b64encode((ASSET / (STEM + ".png")).read_bytes()).decode(),
        )
        preview = template.read_text(encoding="utf-8").replace(
            "__WARDEN_DATA__",
            json.dumps(payload, ensure_ascii=False, separators=(",", ":")),
        )
        (HERE / "preview-motion-v2.html").write_text(preview, encoding="utf-8")
        prior = HERE / "preview-sunburst-v1.html"
        if not prior.exists() and (HERE / "preview-sunburst.html").exists():
            prior.write_bytes((HERE / "preview-sunburst.html").read_bytes())
        (HERE / "preview-sunburst.html").write_text(preview, encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    build()
