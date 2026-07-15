#!/usr/bin/env python3
"""Export the Lost Protocol redraw templates as positioned component packages."""

from __future__ import annotations

import base64
import math
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from html import escape
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "docs" / "concepts" / "layered"
PARTS = SOURCE / "parts"
EDGE_CANDIDATES = (
    Path(r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"),
    Path(r"C:\Program Files\Microsoft\Edge\Application\msedge.exe"),
)
FONT_CANDIDATES = (
    Path(r"C:\Windows\Fonts\msyh.ttc"),
    Path(r"C:\Windows\Fonts\segoeui.ttf"),
    Path(r"C:\Windows\Fonts\arial.ttf"),
)
BOLD_FONT_CANDIDATES = (
    Path(r"C:\Windows\Fonts\msyhbd.ttc"),
    Path(r"C:\Windows\Fonts\segoeuib.ttf"),
    Path(r"C:\Windows\Fonts\arialbd.ttf"),
)
SVG_NS = "{http://www.w3.org/2000/svg}"
INK_NS = "{http://www.inkscape.org/namespaces/inkscape}"
CANVAS_SIZE = (1024, 768)


def edge_path() -> Path:
    for path in EDGE_CANDIDATES:
        if path.is_file():
            return path
    raise FileNotFoundError("Microsoft Edge is required to rasterize the SVG layers")


def ui_font(size: int, *, bold: bool = False) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    for path in BOLD_FONT_CANDIDATES if bold else FONT_CANDIDATES:
        if path.is_file():
            return ImageFont.truetype(str(path), size=size)
    return ImageFont.load_default(size=size)


def safe_name(text: str) -> str:
    result = "".join(char.lower() if char.isalnum() else "_" for char in text)
    while "__" in result:
        result = result.replace("__", "_")
    return result.strip("_")[:72]


def screenshot_svg(
    edge: Path,
    source: Path,
    output: Path,
    *,
    width: int = CANVAS_SIZE[0],
    height: int = CANVAS_SIZE[1],
    transparent: bool = False,
) -> None:
    command = [
        str(edge),
        "--headless=new",
        "--disable-gpu",
        "--disable-extensions",
        "--hide-scrollbars",
        "--no-first-run",
        "--force-device-scale-factor=1",
        f"--window-size={width},{height}",
    ]
    if transparent:
        command.append("--default-background-color=00000000")
    command += [f"--screenshot={output}", source.as_uri()]
    completed = subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=30)
    if completed.returncode != 0 or not output.is_file():
        raise RuntimeError(f"failed to render {source.name}")


def render_layer(
    edge: Path,
    source: Path,
    defs: ET.Element,
    layer_node: ET.Element,
    output: Path,
    temp: Path,
) -> None:
    layer_id = layer_node.get("id", "layer")
    svg_text = (
        '<svg xmlns="http://www.w3.org/2000/svg" xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" '
        'width="1024" height="768" viewBox="0 0 1024 768">'
        + ET.tostring(defs, encoding="unicode")
        + ET.tostring(layer_node, encoding="unicode")
        + "</svg>"
    )
    temp_svg = temp / f"{layer_id}.svg"
    temp_png = temp / f"{layer_id}.png"
    temp_svg.write_text(svg_text, encoding="utf-8")
    screenshot_svg(edge, temp_svg, temp_png, transparent=True)
    image = Image.open(temp_png).convert("RGBA")
    alpha_min, alpha_max = image.getchannel("A").getextrema()
    if alpha_min != 0 or alpha_max == 0:
        raise RuntimeError(f"transparent layer export failed: {source.name}:{layer_id}")
    image.save(output, optimize=True)


def label_lines(label: str) -> list[str]:
    if " [" in label:
        name, metadata = label.split(" [", 1)
        return [name, "[" + metadata]
    return [label]


def make_component_sheet(
    unit: str,
    items: list[tuple[str, Path, Path, tuple[int, int, int, int]]],
    output_png: Path,
    output_svg: Path,
) -> None:
    columns = 4
    cell_w, cell_h = 390, 280
    header_h = 112
    rows = math.ceil(len(items) / columns)
    width, height = columns * cell_w, header_h + rows * cell_h
    background = (243, 240, 232)
    sheet = Image.new("RGB", (width, height), background)
    draw = ImageDraw.Draw(sheet)
    font = ui_font(16)
    title_font = ui_font(28, bold=True)
    subtitle_font = ui_font(17)
    unit_title = unit.replace("_", " ").title()
    title = f"{unit_title} — 分拆组件原图"
    subtitle = "cropped 用于查看；full_canvas 全部置于 (0,0) 即可还原组装位置"
    draw.text((24, 17), title, fill=(30, 34, 38), font=title_font)
    draw.text((24, 63), subtitle, fill=(82, 95, 104), font=subtitle_font)
    svg_images: list[str] = []

    for index, (label, _canvas_path, crop_path, _bounds) in enumerate(items):
        source_image = Image.open(crop_path).convert("RGBA")
        max_image_w, max_image_h = cell_w - 44, cell_h - 86
        scale = min(max_image_w / source_image.width, max_image_h / source_image.height, 2.4)
        image_size = (
            max(1, round(source_image.width * scale)),
            max(1, round(source_image.height * scale)),
        )
        image = source_image.resize(image_size, Image.Resampling.LANCZOS) if image_size != source_image.size else source_image
        col, row = index % columns, index // columns
        cell_x, cell_y = col * cell_w, header_h + row * cell_h
        image_x = cell_x + (cell_w - image.width) // 2
        image_y = cell_y + 72 + (cell_h - 82 - image.height) // 2
        sheet.paste(image, (image_x, image_y), image)
        draw.rectangle(
            (cell_x + 8, cell_y + 4, cell_x + cell_w - 8, cell_y + cell_h - 8),
            outline=(177, 174, 166),
            width=2,
        )
        lines = label_lines(label)
        for line_index, line in enumerate(lines):
            draw.text((cell_x + 18, cell_y + 12 + line_index * 23), line, fill=(52, 64, 73), font=font)

        encoded = base64.b64encode(crop_path.read_bytes()).decode("ascii")
        svg_text_lines = "".join(
            f'<text x="{cell_x+18}" y="{cell_y+30+line_index*23}" '
            f'font-family="Microsoft YaHei,Segoe UI,sans-serif" font-size="16" fill="#344049">{escape(line)}</text>'
            for line_index, line in enumerate(lines)
        )
        svg_images.append(
            f'<g inkscape:groupmode="layer" inkscape:label="{escape(label)}">'
            f'<rect x="{cell_x+8}" y="{cell_y+4}" width="{cell_w-16}" height="{cell_h-12}" '
            'fill="none" stroke="#b1aea6" stroke-width="2"/>'
            + svg_text_lines
            + f'<image href="data:image/png;base64,{encoded}" x="{image_x}" y="{image_y}" '
            f'width="{image.width}" height="{image.height}"/></g>'
        )

    sheet.save(output_png, optimize=True)
    output_svg.write_text(
        '<svg xmlns="http://www.w3.org/2000/svg" xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" '
        f'width="{width}" height="{height}" viewBox="0 0 {width} {height}">'
        '<rect width="100%" height="100%" fill="#f3f0e8"/>'
        f'<text x="24" y="44" font-family="Microsoft YaHei,Segoe UI,sans-serif" font-size="28" '
        f'font-weight="700" fill="#202328">{escape(title)}</text>'
        f'<text x="24" y="78" font-family="Microsoft YaHei,Segoe UI,sans-serif" font-size="17" '
        f'fill="#525f68">{escape(subtitle)}</text>'
        + "".join(svg_images)
        + "</svg>",
        encoding="utf-8",
        newline="\n",
    )


def write_placement_manifest(
    unit: str,
    items: list[tuple[str, Path, Path, tuple[int, int, int, int]]],
    output: Path,
) -> None:
    rows = [
        f"# {unit.replace('_', ' ').title()} 组件定位清单",
        "",
        "- 画布：`1024×768`，左上角为原点。",
        "- 导入：使用 `full_canvas/` 内的 PNG，每一层都放在 `(0,0)`。",
        "- 层级：按文件名前两位编号从小到大叠放，编号越大越靠前。",
        "- 注意：不要裁切、居中、缩放或修改透明边距。",
        "",
        "## 图层列表（从后向前）",
        "",
    ]
    for label, canvas_path, _crop_path, bounds in items:
        left, top, right, bottom = bounds
        rows.append(
            f"- `{canvas_path.name}`：{label}；可见范围 `x={left}..{right-1}, y={top}..{bottom-1}`。"
        )
    rows += [
        "",
        "叠放结果必须与同名 `*_assembled_transparent.png` 完全一致。",
        "",
    ]
    output.write_text("\n".join(rows), encoding="utf-8", newline="\n")


def write_clean_assembly(
    items: list[tuple[str, Path, Path, tuple[int, int, int, int]]],
    output: Path,
) -> None:
    assembled = Image.new("RGBA", CANVAS_SIZE, (0, 0, 0, 0))
    for _label, canvas_path, _crop_path, _bounds in items:
        image = Image.open(canvas_path).convert("RGBA")
        if image.size != CANVAS_SIZE:
            raise ValueError(f"wrong full-canvas size: {canvas_path}")
        if image.getchannel("A").getbbox() is None:
            raise ValueError(f"empty full-canvas component: {canvas_path}")
        assembled.alpha_composite(image)
    assembled.save(output, optimize=True)

    verification = Image.new("RGBA", CANVAS_SIZE, (0, 0, 0, 0))
    for _label, canvas_path, _crop_path, _bounds in items:
        verification.alpha_composite(Image.open(canvas_path).convert("RGBA"))
    if ImageChops.difference(assembled, verification).getbbox() is not None:
        raise RuntimeError(f"full-canvas stacking verification failed: {output.name}")


def export_unit(edge: Path, source: Path) -> int:
    tree = ET.parse(source)
    root = tree.getroot()
    defs = root.find(f"{SVG_NS}defs")
    if defs is None:
        raise ValueError(f"{source} has no SVG defs")
    unit = source.stem.removesuffix("_layered")
    unit_dir = PARTS / unit
    canvas_dir = unit_dir / "full_canvas"
    cropped_dir = unit_dir / "cropped"
    if unit_dir.exists():
        shutil.rmtree(unit_dir)
    canvas_dir.mkdir(parents=True)
    cropped_dir.mkdir(parents=True)
    items: list[tuple[str, Path, Path, tuple[int, int, int, int]]] = []

    with tempfile.TemporaryDirectory(prefix=f"ninslash-{unit}-") as temp_name:
        temp = Path(temp_name)
        screenshot_svg(edge, source, SOURCE / f"{unit}_layered.png")
        part_index = 0
        for node in root.findall(f"{SVG_NS}g"):
            if node.get(f"{INK_NS}groupmode") != "layer":
                continue
            label = node.get(f"{INK_NS}label", node.get("id", "layer"))
            if label.startswith("00 ") or label.startswith("99 "):
                continue
            part_index += 1
            filename = f"{part_index:02d}_{safe_name(label)}.png"
            canvas_path = canvas_dir / filename
            render_layer(edge, source, defs, node, canvas_path, temp)
            image = Image.open(canvas_path).convert("RGBA")
            alpha_bounds = image.getchannel("A").getbbox()
            if alpha_bounds is None:
                raise ValueError(f"empty exported layer: {source.name}:{label}")
            margin = 12
            bounds = (
                max(0, alpha_bounds[0] - margin),
                max(0, alpha_bounds[1] - margin),
                min(CANVAS_SIZE[0], alpha_bounds[2] + margin),
                min(CANVAS_SIZE[1], alpha_bounds[3] + margin),
            )
            crop_path = cropped_dir / filename
            image.crop(bounds).save(crop_path, optimize=True)
            items.append((label, canvas_path, crop_path, bounds))
            print(f"{unit}: {part_index:02d}/{label}")

    write_clean_assembly(items, SOURCE / f"{unit}_assembled_transparent.png")
    make_component_sheet(unit, items, SOURCE / f"{unit}_components.png", SOURCE / f"{unit}_components.svg")
    write_placement_manifest(unit, items, unit_dir / "PLACEMENT_CN.md")
    return len(items)


def make_pack_preview(sources: list[Path]) -> None:
    columns = 2
    cell_size = (640, 480)
    rows = math.ceil(len(sources) / columns)
    preview = Image.new("RGB", (columns * cell_size[0], rows * cell_size[1]), (232, 229, 221))
    for index, source in enumerate(sources):
        unit = source.stem.removesuffix("_layered")
        image = Image.open(SOURCE / f"{unit}_layered.png").convert("RGB")
        image = image.resize(cell_size, Image.Resampling.LANCZOS)
        preview.paste(image, ((index % columns) * cell_size[0], (index // columns) * cell_size[1]))
    preview.save(SOURCE / "layered_pack_preview.png", optimize=True)


def main() -> None:
    edge = edge_path()
    PARTS.mkdir(parents=True, exist_ok=True)
    sources = sorted(SOURCE.glob("[0-9][0-9]_*_layered.svg"))
    if not sources:
        raise FileNotFoundError("no layered concept SVGs found")
    total = 0
    for source in sources:
        total += export_unit(edge, source)
    make_pack_preview(sources)
    print(f"verified {len(sources)} units and {total} positioned components")


if __name__ == "__main__":
    main()
