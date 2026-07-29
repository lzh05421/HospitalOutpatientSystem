from __future__ import annotations

import html
import math
from pathlib import Path
from xml.etree import ElementTree as ET

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "output" / "redrawn_course_figures"
PNG_PATH = OUT_DIR / "figure_4_4_prescription_flow.png"
SVG_PATH = OUT_DIR / "figure_4_4_prescription_flow.svg"
DRAWIO_PATH = OUT_DIR / "figure_4_4_prescription_flow.drawio"

FONT_CN = Path(r"C:\Windows\Fonts\simhei.ttf")
FONT_SONG = Path(r"C:\Windows\Fonts\STSONG.TTF")

SCALE = 2
W, H = 1880, 1760


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    path = FONT_CN if bold else FONT_SONG
    return ImageFont.truetype(str(path), size * SCALE)


def s(v: int | float) -> int:
    return int(round(v * SCALE))


def box(rect: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
    return tuple(s(v) for v in rect)


def draw_centered(
    draw: ImageDraw.ImageDraw,
    rect: tuple[int, int, int, int],
    lines: list[str],
    fill: str = "#111111",
    title: bool = False,
) -> None:
    f = font(28 if title else 25, bold=title)
    line_gap = s(8)
    measurements = [draw.textbbox((0, 0), line, font=f) for line in lines]
    heights = [m[3] - m[1] for m in measurements]
    total_h = sum(heights) + line_gap * (len(lines) - 1)
    x1, y1, x2, y2 = box(rect)
    y = y1 + (y2 - y1 - total_h) / 2
    for line, m, h in zip(lines, measurements, heights):
        w_line = m[2] - m[0]
        draw.text((x1 + (x2 - x1 - w_line) / 2, y), line, font=f, fill=fill)
        y += h + line_gap


def arrow_head(draw: ImageDraw.ImageDraw, end: tuple[int, int], angle: float, color: str) -> None:
    size = s(18)
    spread = math.radians(26)
    x, y = end
    pts = [
        (x, y),
        (x - size * math.cos(angle - spread), y - size * math.sin(angle - spread)),
        (x - size * math.cos(angle + spread), y - size * math.sin(angle + spread)),
    ]
    draw.polygon(pts, fill=color)


def draw_arrow(
    draw: ImageDraw.ImageDraw,
    pts: list[tuple[int, int]],
    color: str = "#111111",
) -> None:
    scaled = [(s(x), s(y)) for x, y in pts]
    draw.line(scaled, fill=color, width=s(3), joint="curve")
    (x1, y1), (x2, y2) = scaled[-2], scaled[-1]
    angle = math.atan2(y2 - y1, x2 - x1)
    arrow_head(draw, (x2, y2), angle, color)


def draw_process(draw: ImageDraw.ImageDraw, rect: tuple[int, int, int, int], lines: list[str]) -> None:
    draw.rectangle(box(rect), outline="#111111", width=s(3), fill="#FFFFFF")
    draw_centered(draw, rect, lines)


def draw_terminator(draw: ImageDraw.ImageDraw, rect: tuple[int, int, int, int], lines: list[str]) -> None:
    draw.rounded_rectangle(box(rect), radius=s(36), outline="#111111", width=s(3), fill="#FFFFFF")
    draw_centered(draw, rect, lines)


def draw_diamond(draw: ImageDraw.ImageDraw, cx: int, cy: int, w: int, h: int, lines: list[str]) -> tuple[int, int, int, int]:
    points = [(cx, cy - h // 2), (cx + w // 2, cy), (cx, cy + h // 2), (cx - w // 2, cy)]
    draw.polygon([(s(x), s(y)) for x, y in points], outline="#111111", fill="#FFFFFF")
    draw.line([(s(x), s(y)) for x, y in points + [points[0]]], fill="#111111", width=s(3))
    rect = (cx - w // 2, cy - h // 2, cx + w // 2, cy + h // 2)
    draw_centered(draw, rect, lines)
    return rect


def draw_label(draw: ImageDraw.ImageDraw, pos: tuple[int, int], text: str) -> None:
    draw.text((s(pos[0]), s(pos[1])), text, font=font(24), fill="#111111")


def make_png() -> None:
    img = Image.new("RGB", (s(W), s(H)), "white")
    draw = ImageDraw.Draw(img)

    center = 900
    main_w, main_h = 620, 82
    x1, x2 = center - main_w // 2, center + main_w // 2

    start = (650, 60, 1150, 130)
    create = (x1, 205, x2, 287)
    write = (x1, 360, x2, 442)
    rules = (x1, 515, x2, 597)
    review = (x1, 795, x2, 877)
    paid = (x1, 950, x2, 1032)
    dispense = (x1, 1235, x2, 1317)
    stock = (x1, 1390, x2, 1472)
    status_ok = (650, 1518, 1150, 1588)
    end_ok = (650, 1660, 1150, 1730)

    reject = (1260, 645, 1660, 735)
    rejected_status = (1260, 815, 1660, 885)
    unpaid = (1260, 1095, 1660, 1185)
    unpaid_status = (1260, 1260, 1660, 1330)
    returned = (140, 1390, 500, 1472)
    returned_status = (140, 1518, 500, 1588)

    draw_terminator(draw, start, ["开始"])
    draw_process(draw, create, ["医生接诊页创建处方", "PrescriptionService::create"])
    draw_process(draw, write, ["写入 prescriptions", "与 prescription_items"])
    draw_process(draw, rules, ["读取 pass_rules", "进行用药规则提示"])
    decision_review = draw_diamond(draw, center, 700, 520, 155, ["药师是否审核通过？"])
    draw_process(draw, review, ["药师审核通过", "reviewPrescription"])
    decision_paid = draw_diamond(draw, center, 1110, 520, 155, ["账单是否已缴费？", "bills.status = PAID"])
    draw_process(draw, paid, ["收费完成后", "进入发药环节"])
    draw_process(draw, dispense, ["确认发药", "dispensePrescription"])
    draw_process(draw, stock, ["扣减 drugs 库存", "写入 stock_records"])
    draw_terminator(draw, end_ok, ["结束", "状态 DISPENSED"])

    draw_process(draw, reject, ["记录驳回原因", "状态 REJECTED"])
    draw_process(draw, rejected_status, ["处方已驳回"])
    draw_process(draw, unpaid, ["提示先完成收费", "不允许发药"])
    draw_process(draw, unpaid_status, ["等待缴费"])
    draw_process(draw, returned, ["退药 return", "回补库存"])
    draw_process(draw, returned_status, ["状态 RETURNED"])
    draw_process(draw, status_ok, ["状态 DISPENSED"])
    draw_terminator(draw, end_ok, ["结束"])

    # Main vertical route.
    draw_arrow(draw, [(center, 130), (center, 205)])
    draw_arrow(draw, [(center, 287), (center, 360)])
    draw_arrow(draw, [(center, 442), (center, 515)])
    draw_arrow(draw, [(center, 597), (center, 623)])
    draw_arrow(draw, [(center, 777), (center, 795)])
    draw_arrow(draw, [(center, 877), (center, 950)])
    draw_arrow(draw, [(center, 1032), (center, 1033)])
    draw_arrow(draw, [(center, 1187), (center, 1235)])
    draw_arrow(draw, [(center, 1317), (center, 1390)])
    draw_arrow(draw, [(center, 1472), (center, 1518)])
    draw_arrow(draw, [(center, 1588), (center, 1660)])

    # Review branch.
    draw_arrow(draw, [(1160, 700), (1260, 700)])
    draw_label(draw, (1185, 668), "否")
    draw_arrow(draw, [(1460, 735), (1460, 815)])
    draw_arrow(draw, [(1660, 850), (1760, 850), (1760, 1695), (1150, 1695)])
    draw_label(draw, (915, 765), "是")

    # Payment branch.
    draw_arrow(draw, [(1160, 1110), (1260, 1110)])
    draw_label(draw, (1185, 1078), "否")
    draw_arrow(draw, [(1460, 1185), (1460, 1260)])
    draw_arrow(draw, [(1660, 1295), (1725, 1295), (1725, 1660), (1150, 1660)])
    draw_label(draw, (915, 1175), "是")

    # Return branch after dispensing.
    draw_arrow(draw, [(590, 1431), (500, 1431)])
    draw_label(draw, (510, 1395), "退药")
    draw_arrow(draw, [(320, 1472), (320, 1518)])
    draw_arrow(draw, [(500, 1553), (610, 1553), (610, 1695), (650, 1695)])

    img = img.resize((W, H), Image.Resampling.LANCZOS)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    img.save(PNG_PATH)


def svg_text(x: int, y: int, lines: list[str], size: int = 20, weight: str = "normal") -> str:
    line_height = size + 8
    start_y = y - line_height * (len(lines) - 1) / 2
    tspans = []
    for i, line in enumerate(lines):
        tspans.append(
            f'<tspan x="{x}" y="{start_y + i * line_height:.1f}">{html.escape(line)}</tspan>'
        )
    return (
        f'<text text-anchor="middle" dominant-baseline="middle" '
        f'font-family="SimSun, SimHei, Microsoft YaHei" font-size="{size}" '
        f'font-weight="{weight}" fill="#111111">{"".join(tspans)}</text>'
    )


def make_svg() -> None:
    # Lightweight SVG companion for vector insertion if needed.
    PNG_PATH.with_suffix(".svg")
    svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">
  <rect width="100%" height="100%" fill="#ffffff"/>
  <image href="{PNG_PATH.name}" x="0" y="0" width="{W}" height="{H}"/>
</svg>
'''
    SVG_PATH.write_text(svg, encoding="utf-8")


def mx_cell(cell_id: str, value: str, style: str, x: int, y: int, width: int, height: int, shape: str = "rect") -> ET.Element:
    if shape == "terminator":
        style = "rounded=1;arcSize=50;whiteSpace=wrap;html=1;fillColor=#FFFFFF;strokeColor=#111111;strokeWidth=2;fontSize=16;"
    elif shape == "diamond":
        style = "rhombus;whiteSpace=wrap;html=1;fillColor=#FFFFFF;strokeColor=#111111;strokeWidth=2;fontSize=16;"
    else:
        style = "rounded=0;whiteSpace=wrap;html=1;fillColor=#FFFFFF;strokeColor=#111111;strokeWidth=2;fontSize=16;"
    cell = ET.Element("mxCell", id=cell_id, value=value, style=style, vertex="1", parent="1")
    geo = ET.SubElement(cell, "mxGeometry", x=str(x), y=str(y), width=str(width), height=str(height), as_="geometry")
    geo.attrib["as"] = "geometry"
    del geo.attrib["as_"]
    return cell


def edge(cell_id: str, source: str, target: str, label: str = "") -> ET.Element:
    style = "edgeStyle=orthogonalEdgeStyle;rounded=0;html=1;strokeColor=#111111;strokeWidth=2;endArrow=classic;"
    cell = ET.Element("mxCell", id=cell_id, value=label, style=style, edge="1", parent="1", source=source, target=target)
    geo = ET.SubElement(cell, "mxGeometry", relative="1", as_="geometry")
    geo.attrib["as"] = "geometry"
    del geo.attrib["as_"]
    return cell


def make_drawio() -> None:
    mxfile = ET.Element("mxfile", host="app.diagrams.net")
    diagram = ET.SubElement(mxfile, "diagram", name="图4-4 处方审核与发药流程图", id="prescription-flow-redrawn")
    model = ET.SubElement(
        diagram,
        "mxGraphModel",
        dx="1200",
        dy="900",
        grid="1",
        gridSize="10",
        guides="1",
        tooltips="1",
        connect="1",
        arrows="1",
        fold="1",
        page="1",
        pageScale="1",
        pageWidth=str(W),
        pageHeight=str(H),
        background="#FFFFFF",
    )
    root = ET.SubElement(model, "root")
    ET.SubElement(root, "mxCell", id="0")
    ET.SubElement(root, "mxCell", id="1", parent="0")

    nodes = [
        ("n_start", "开始", 650, 60, 500, 70, "terminator"),
        ("n_create", "医生接诊页创建处方<br>PrescriptionService::create", 590, 205, 620, 82, "rect"),
        ("n_write", "写入 prescriptions<br>与 prescription_items", 590, 360, 620, 82, "rect"),
        ("n_rules", "读取 pass_rules<br>进行用药规则提示", 590, 515, 620, 82, "rect"),
        ("n_review_decision", "药师是否审核通过？", 640, 623, 520, 155, "diamond"),
        ("n_reject", "记录驳回原因<br>状态 REJECTED", 1260, 645, 400, 90, "rect"),
        ("n_reject_end", "结束<br>处方已驳回", 1260, 815, 400, 70, "terminator"),
        ("n_review", "药师审核通过<br>reviewPrescription", 590, 795, 620, 82, "rect"),
        ("n_paid", "收费完成后<br>进入发药环节", 590, 950, 620, 82, "rect"),
        ("n_paid_decision", "账单是否已缴费？<br>bills.status = PAID", 640, 1033, 520, 155, "diamond"),
        ("n_unpaid", "提示先完成收费<br>不允许发药", 1260, 1095, 400, 90, "rect"),
        ("n_unpaid_end", "结束<br>等待缴费", 1260, 1260, 400, 70, "terminator"),
        ("n_dispense", "确认发药<br>dispensePrescription", 590, 1235, 620, 82, "rect"),
        ("n_stock", "扣减 drugs 库存<br>写入 stock_records", 590, 1390, 620, 82, "rect"),
        ("n_return", "退药 return<br>回补库存", 140, 1390, 360, 82, "rect"),
        ("n_return_end", "结束<br>状态 RETURNED", 140, 1560, 360, 70, "terminator"),
        ("n_ok_end", "结束<br>状态 DISPENSED", 650, 1560, 500, 70, "terminator"),
    ]
    for node in nodes:
        root.append(mx_cell(*node))

    edges = [
        ("e1", "n_start", "n_create", ""),
        ("e2", "n_create", "n_write", ""),
        ("e3", "n_write", "n_rules", ""),
        ("e4", "n_rules", "n_review_decision", ""),
        ("e5", "n_review_decision", "n_reject", "否"),
        ("e6", "n_reject", "n_reject_end", ""),
        ("e7", "n_review_decision", "n_review", "是"),
        ("e8", "n_review", "n_paid", ""),
        ("e9", "n_paid", "n_paid_decision", ""),
        ("e10", "n_paid_decision", "n_unpaid", "否"),
        ("e11", "n_unpaid", "n_unpaid_end", ""),
        ("e12", "n_paid_decision", "n_dispense", "是"),
        ("e13", "n_dispense", "n_stock", ""),
        ("e14", "n_stock", "n_return", "退药"),
        ("e15", "n_return", "n_return_end", ""),
        ("e16", "n_stock", "n_ok_end", ""),
    ]
    for e in edges:
        root.append(edge(*e))

    tree = ET.ElementTree(mxfile)
    tree.write(DRAWIO_PATH, encoding="utf-8", xml_declaration=True)


if __name__ == "__main__":
    make_png()
    make_svg()
    make_drawio()
    print(PNG_PATH)
    print(DRAWIO_PATH)
