from __future__ import annotations

import io
import math
import shutil
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

from lxml import etree
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "output"
ASSET_DIR = OUT / "redrawn_course_figures"
SOURCE_DOCX = OUT / "course_summary_current.docx"
FIXED_DOCX = OUT / "course_summary_figures_tables_fixed.docx"

W_NS = "http://schemas.openxmlformats.org/wordprocessingml/2006/main"
NS = {
    "w": W_NS,
    "a": "http://schemas.openxmlformats.org/drawingml/2006/main",
    "r": "http://schemas.openxmlformats.org/officeDocument/2006/relationships",
}


def font_path(*names: str) -> str:
    for name in names:
        p = Path("C:/Windows/Fonts") / name
        if p.exists():
            return str(p)
    return str(Path("C:/Windows/Fonts/simsun.ttc"))


FONT_CN = font_path("simsun.ttc", "simfang.ttf", "msyh.ttc")
FONT_HEI = font_path("simhei.ttf", "msyhbd.ttc", "msyh.ttc")


def font(size: int, bold: bool = False):
    return ImageFont.truetype(FONT_HEI if bold else FONT_CN, size)


def text_bbox(draw: ImageDraw.ImageDraw, text: str, fnt) -> tuple[int, int]:
    bbox = draw.multiline_textbbox((0, 0), text, font=fnt, spacing=4, align="center")
    return bbox[2] - bbox[0], bbox[3] - bbox[1]


def center_text(draw: ImageDraw.ImageDraw, box, text: str, size=28, bold=False, fill="black"):
    fnt = font(size, bold)
    w, h = text_bbox(draw, text, fnt)
    x = (box[0] + box[2] - w) / 2
    y = (box[1] + box[3] - h) / 2
    draw.multiline_text((x, y), text, font=fnt, fill=fill, spacing=4, align="center")


def center_text_fit(draw: ImageDraw.ImageDraw, box, text: str, size=28, bold=False, fill="black", min_size=14):
    """Center text without splitting real class/table names across letters."""
    current = size
    while current >= min_size:
        fnt = font(current, bold)
        w, h = text_bbox(draw, text, fnt)
        if w <= (box[2] - box[0] - 14) and h <= (box[3] - box[1] - 8):
            x = (box[0] + box[2] - w) / 2
            y = (box[1] + box[3] - h) / 2
            draw.multiline_text((x, y), text, font=fnt, fill=fill, spacing=4, align="center")
            return
        current -= 1
    center_text(draw, box, text, min_size, bold, fill)


def draw_wrapped_text(draw, box, text: str, size=26, bold=False):
    # Keep English identifiers intact; most diagram labels are manually line-broken.
    if "\n" in text or any(ch.isascii() and ch.isalpha() for ch in text):
        center_text_fit(draw, box, text, size, bold)
        return
    max_chars = max(4, int((box[2] - box[0]) / (size * 0.95)))
    if len(text) > max_chars:
        mid = len(text) // 2
        text = text[:mid] + "\n" + text[mid:]
    center_text_fit(draw, box, text, size, bold)


def rect(draw, box, text="", size=26, bold=False, width=2):
    draw.rectangle(box, outline="black", width=width)
    if text:
        draw_wrapped_text(draw, box, text, size, bold)


def rounded(draw, box, text="", size=26, bold=False, width=2):
    draw.rounded_rectangle(box, radius=28, outline="black", width=width, fill="white")
    if text:
        draw_wrapped_text(draw, box, text, size, bold)


def ellipse(draw, box, text="", size=26, bold=False, width=2):
    draw.ellipse(box, outline="black", width=width, fill="white")
    if text:
        draw_wrapped_text(draw, box, text, size, bold)


def diamond(draw, cx, cy, w, h, text="", size=24, width=2):
    pts = [(cx, cy - h // 2), (cx + w // 2, cy), (cx, cy + h // 2), (cx - w // 2, cy)]
    draw.polygon(pts, outline="black", fill="white")
    draw.line(pts + [pts[0]], fill="black", width=width)
    if text:
        center_text(draw, (cx - w // 2, cy - h // 2, cx + w // 2, cy + h // 2), text, size)


def line(draw, a, b, width=2, dash=False):
    if not dash:
        draw.line([a, b], fill="black", width=width)
        return
    x1, y1 = a
    x2, y2 = b
    length = math.hypot(x2 - x1, y2 - y1)
    if length == 0:
        return
    step = 16
    gap = 10
    dx = (x2 - x1) / length
    dy = (y2 - y1) / length
    pos = 0
    while pos < length:
        end = min(pos + step, length)
        draw.line([(x1 + dx * pos, y1 + dy * pos), (x1 + dx * end, y1 + dy * end)], fill="black", width=width)
        pos += step + gap


def arrow(draw, a, b, width=2, dash=False):
    line(draw, a, b, width, dash)
    ang = math.atan2(b[1] - a[1], b[0] - a[0])
    size = 14
    pts = [
        b,
        (b[0] - size * math.cos(ang - math.pi / 6), b[1] - size * math.sin(ang - math.pi / 6)),
        (b[0] - size * math.cos(ang + math.pi / 6), b[1] - size * math.sin(ang + math.pi / 6)),
    ]
    draw.polygon(pts, fill="black")


def label(draw, xy, text, size=22):
    draw.text(xy, text, font=font(size), fill="black")


def canvas(w=1600, h=1000, gray=False):
    img = Image.new("RGB", (w, h), "#eeeeee" if gray else "white")
    return img, ImageDraw.Draw(img)


def save(img: Image.Image, path: Path):
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path)


def strip_top_title(path: Path, height: int = 96):
    img = Image.open(path).convert("RGB")
    draw = ImageDraw.Draw(img)
    draw.rectangle((0, 0, img.width, height), fill="white")
    img.save(path)


def fig_arch(path: Path):
    img, d = canvas(1800, 1000)
    rect(d, (70, 80, 1730, 920), width=2)
    rect(d, (655, 125, 1145, 185), "医院门诊挂号与药品管理系统", 25)
    boxes = {
        "医院工作台\nEntryDialog / LoginDialog\nMainWindow": (120, 250, 510, 380),
        "患者预约入口\nPatientLoginDialog\nPatientAppointmentWindow": (120, 560, 510, 690),
        "客户端通信\nApiClient\nQTcpSocket + JSON Lines": (655, 405, 1075, 535),
        "服务端连接\nHospitalServer\nClientConnection": (1240, 180, 1635, 285),
        "统一路由与鉴权\nRequestRouter\nSessionManager\nAuthorizationService": (1240, 330, 1635, 465),
        "业务服务层\nAuth / Patient / Registration / Department\nSchedule / Doctor / Consultation / Examination\nPrescription / Inventory / Billing / Statistics\nDashboard / PatientRecord / OperationLog / PermissionAdmin": (1135, 525, 1695, 710),
        "数据访问层\nDatabaseManager\nMySQL：users、patients、departments、doctors\nregistrations、medical_records、examinations、prescriptions\ndrugs、bills、payments、operation_logs": (1135, 780, 1695, 880),
    }
    for text, b in boxes.items():
        rect(d, b, text, 21 if "业务服务层" in text or "数据访问层" in text else 23)
    arrow(d, (510, 315), (655, 450))
    arrow(d, (510, 625), (655, 495))
    arrow(d, (1075, 470), (1240, 232))
    arrow(d, (1075, 470), (1240, 398))
    arrow(d, (1438, 465), (1438, 525))
    arrow(d, (1438, 710), (1438, 780))
    arrow(d, (1438, 780), (1438, 710))
    label(d, (530, 360), "Request / Response", 22)
    label(d, (1095, 348), "module / action", 21)
    save(img, path)


def vertical_label(text: str) -> str:
    return "\n".join(text)


def fig_function_tree(path: Path):
    img, d = canvas(1900, 980)
    root = (690, 60, 1210, 115)
    rect(d, root, "医院门诊挂号与药品管理系统", 25)
    groups = [
        ("基础管理", ["院长驾驶舱", "患者管理", "科室管理", "医生管理"]),
        ("挂号排班", ["医生排班", "挂号管理", "候诊队列"]),
        ("诊疗病历", ["医生接诊", "患者病历档案", "检查检验"]),
        ("药房收费", ["处方管理", "药品库存", "收费结算"]),
        ("统计审计权限", ["费用统计", "操作日志", "权限配置"]),
    ]
    start_x = 90
    gap = 365
    y_group = 240
    y_leaf = 355
    bus_y = 175
    line(d, ((root[0] + root[2]) // 2, root[3]), ((root[0] + root[2]) // 2, bus_y))
    line(d, (start_x + 125, bus_y), (start_x + gap * 4 + 125, bus_y))
    for i, (name, items) in enumerate(groups):
        x = start_x + i * gap
        rect(d, (x, y_group, x + 250, y_group + 55), name, 24)
        line(d, (x + 125, bus_y), (x + 125, y_group))
        line(d, (x + 125, y_group + 55), (x + 125, y_leaf - 30))
        if len(items) == 4:
            leaf_xs = [x - 18, x + 62, x + 142, x + 222]
        else:
            leaf_xs = [x + 10, x + 95, x + 180]
        line(d, (leaf_xs[0] + 30, y_leaf - 30), (leaf_xs[-1] + 30, y_leaf - 30))
        for lx, item in zip(leaf_xs, items):
            line(d, (lx + 30, y_leaf - 30), (lx + 30, y_leaf))
            rect(d, (lx, y_leaf, lx + 60, y_leaf + 300), vertical_label(item), 23)
    save(img, path)


def fig_tech(path: Path):
    img, d = canvas()
    layers = [
        ("表现层", "EntryDialog、MainWindow、ModulePage、PatientAppointmentWindow"),
        ("通信层", "ApiClient、QTcpSocket、JSON Lines、common::Request/Response"),
        ("路由层", "HospitalServer、ClientConnection、RequestRouter、SessionManager"),
        ("业务层", "16个ModuleService：挂号、排班、接诊、处方、库存、收费等"),
        ("数据层", "DatabaseManager、MySQL、事务、外键、审计日志、Outbox"),
        ("工程层", "CMakePresets、Windows/Linux/银河麒麟脚本、Qt Test/CTest"),
    ]
    y = 95
    for name, desc in layers:
        rect(d, (180, y, 1420, y + 92), width=2)
        rect(d, (180, y, 380, y + 92), name, 27)
        center_text(d, (410, y, 1420, y + 92), desc, 25)
        if y < 700:
            arrow(d, (800, y + 92), (800, y + 128))
        y += 135
    save(img, path)


def uml_class(draw, box, name, attrs, methods, size=20):
    rect(draw, box, width=2)
    x1, y1, x2, y2 = box
    draw.line((x1, y1 + 46, x2, y1 + 46), fill="black", width=2)
    draw.line((x1, y1 + 46 + 28 * max(1, len(attrs)), x2, y1 + 46 + 28 * max(1, len(attrs))), fill="black", width=2)
    center_text(draw, (x1, y1, x2, y1 + 46), name, size + 2, True)
    y = y1 + 55
    for a in attrs:
        draw.text((x1 + 12, y), a, font=font(size), fill="black")
        y += 28
    y = y1 + 58 + 28 * max(1, len(attrs))
    for m in methods:
        draw.text((x1 + 12, y), m, font=font(size), fill="black")
        y += 28


def fig_class_initial(path: Path):
    img, d = canvas(1700, 980)
    uml_class(d, (80, 100, 410, 335), "MainWindow", ["- m_apiClient", "- m_navigation", "- m_navSearch", "- m_pages"], ["+ addModulePage()", "+ canAccess()", "+ filterNavigation()"], 18)
    uml_class(d, (80, 500, 410, 750), "ModulePage", ["- m_apiClient", "- m_module"], ["+ refresh()", "+ sendRequest()", "+ renderRows()"])
    uml_class(d, (520, 300, 830, 535), "ApiClient", ["- m_socket", "- m_token"], ["+ connectToServer()", "+ send()", "+ onReadyRead()"])
    uml_class(d, (950, 100, 1280, 315), "HospitalServer", ["- m_router"], ["+ start()", "+ onNewConnection()"])
    uml_class(d, (950, 500, 1280, 760), "RequestRouter", ["- m_services", "- m_sessions"], ["+ registerService()", "+ route()", "+ authorize()"])
    uml_class(d, (1370, 300, 1660, 535), "DatabaseManager", ["- m_database"], ["+ ensureOpen()", "+ database()"])
    arrow(d, (410, 215), (520, 365))
    arrow(d, (410, 620), (520, 470))
    arrow(d, (830, 418), (950, 215))
    arrow(d, (1115, 315), (1115, 500))
    arrow(d, (1280, 630), (1370, 425))
    label(d, (425, 320), "调用", 22)
    label(d, (850, 275), "TCP请求", 22)
    save(img, path)


def fig_sequence(path: Path):
    img, d = canvas(1700, 1000, gray=True)
    names = ["挂号员", "RegistrationPage", "ApiClient", "RequestRouter", "RegistrationService", "MySQL"]
    xs = [170, 430, 690, 960, 1230, 1500]
    for x, name in zip(xs, names):
        if name == "挂号员":
            ellipse(d, (x - 22, 95, x + 22, 139), "")
            line(d, (x, 139), (x, 195))
            line(d, (x - 35, 160), (x + 35, 160))
            line(d, (x, 195), (x - 32, 238))
            line(d, (x, 195), (x + 32, 238))
            center_text(d, (x - 70, 245, x + 70, 285), name, 24)
        else:
            rect(d, (x - 115, 115, x + 115, 175), ":" + name, 21)
        line(d, (x, 175), (x, 895), dash=True)
        rect(d, (x - 9, 265, x + 9, 800), "")
    messages = [
        (0, 1, "1. 选择患者、医生和号源"),
        (1, 2, "2. send(registration/create)"),
        (2, 3, "3. JSON Lines请求"),
        (3, 4, "4. authorize后转发"),
        (4, 5, "5. 校验号源、医保token"),
        (5, 4, "6. 写registrations和bills"),
        (4, 3, "7. 返回registrationNo/billNo"),
        (3, 2, "8. publicResponseData"),
        (2, 1, "9. onResponseReceived"),
        (1, 0, "10. 刷新挂号列表"),
    ]
    y = 285
    for a, b, txt in messages:
        dash = a > b
        arrow(d, (xs[a] + (12 if a < b else -12), y), (xs[b] - (12 if a < b else -12), y), dash=dash)
        label(d, ((xs[a] + xs[b]) // 2 - 80, y - 28), txt, 19)
        y += 58
    save(img, path)


def fig_class_detail(path: Path):
    img, d = canvas(1900, 1150)
    uml_class(d, (700, 70, 1100, 250), "ModuleService", [], ["+ handle(request)"])
    services = [
        "AuthService", "PatientService", "RegistrationService", "DepartmentService",
        "ScheduleService", "DoctorService", "ConsultationService", "ExaminationService",
        "PrescriptionService", "InventoryService", "BillingService", "StatisticsService",
        "DashboardService", "PatientRecordService", "OperationLogService", "PermissionAdminService",
    ]
    top_join_y = 300
    bottom_join_y = 965
    line(d, (220, top_join_y), (1680, top_join_y))
    line(d, (900, 250), (900, top_join_y))
    for i, name in enumerate(services):
        col = i % 4
        row = i // 4
        x = 80 + col * 450
        y = 360 + row * 185
        uml_class(d, (x, y, x + 350, y + 125), name, ["- m_database"], ["+ handle(request)"], 16)
        line(d, (x + 175, y), (x + 175, top_join_y))
        line(d, (x + 175, y + 125), (x + 175, bottom_join_y))
    uml_class(d, (700, 1000, 1100, 1138), "DatabaseManager", ["- QSqlDatabase"], ["+ ensureOpen()", "+ database()"], 18)
    line(d, (220, bottom_join_y), (1680, bottom_join_y))
    arrow(d, (900, bottom_join_y), (900, 1000))
    save(img, path)


def fig_er_total(path: Path):
    img, d = canvas(1900, 1120, gray=True)
    boxes = {
        "科室\ndepartments": (120, 110, 320, 185),
        "医生\ndoctors": (495, 110, 675, 185),
        "排班号源\ndoctor_schedules": (850, 110, 1090, 185),
        "患者\npatients": (120, 390, 320, 465),
        "挂号\nregistrations": (495, 390, 695, 465),
        "病历\nmedical_records": (850, 340, 1070, 415),
        "检查\nexaminations": (850, 485, 1070, 560),
        "处方\nprescriptions": (850, 650, 1070, 725),
        "药品\ndrugs": (1245, 650, 1425, 725),
        "账单\nbills": (495, 835, 695, 910),
        "支付\npayments": (850, 835, 1070, 910),
        "操作日志\noperation_logs": (1245, 835, 1465, 910),
    }
    for name, box in boxes.items():
        rect(d, box, name, 21)

    def anchor(name: str, side: str):
        x1, y1, x2, y2 = boxes[name]
        if side == "L":
            return x1, (y1 + y2) // 2
        if side == "R":
            return x2, (y1 + y2) // 2
        if side == "T":
            return (x1 + x2) // 2, y1
        if side == "B":
            return (x1 + x2) // 2, y2
        raise ValueError(side)

    def relation(name, cx, cy):
        diamond(d, cx, cy, 114, 68, name, 21)
        return {
            "L": (cx - 57, cy),
            "R": (cx + 57, cy),
            "T": (cx, cy - 34),
            "B": (cx, cy + 34),
        }

    def connect_entity_rel(ent_name, ent_side, rel_pt, card, card_xy):
        line(d, anchor(ent_name, ent_side), rel_pt)
        label(d, card_xy, card, 22)

    def connect_rel_entity(rel_pt, ent_name, ent_side, card, card_xy):
        line(d, rel_pt, anchor(ent_name, ent_side))
        label(d, card_xy, card, 22)

    r = relation("隶属", 408, 148)
    connect_entity_rel("科室\ndepartments", "R", r["L"], "1", (330, 115))
    connect_rel_entity(r["R"], "医生\ndoctors", "L", "n", (465, 115))

    r = relation("排班", 765, 148)
    connect_entity_rel("医生\ndoctors", "R", r["L"], "1", (685, 115))
    connect_rel_entity(r["R"], "排班号源\ndoctor_schedules", "L", "n", (820, 115))

    r = relation("办理", 408, 428)
    connect_entity_rel("患者\npatients", "R", r["L"], "1", (330, 395))
    connect_rel_entity(r["R"], "挂号\nregistrations", "L", "n", (465, 395))

    r = relation("选择", 595, 286)
    connect_entity_rel("医生\ndoctors", "B", r["T"], "1", (610, 205))
    connect_rel_entity(r["B"], "挂号\nregistrations", "T", "n", (617, 350))

    r = relation("占用", 775, 286)
    connect_entity_rel("排班号源\ndoctor_schedules", "B", r["T"], "1", (910, 205))
    connect_rel_entity(r["B"], "挂号\nregistrations", "T", "n", (708, 350))

    r = relation("生成", 785, 378)
    connect_entity_rel("挂号\nregistrations", "R", r["L"], "1", (705, 370))
    connect_rel_entity(r["R"], "病历\nmedical_records", "L", "1", (817, 318))

    r = relation("申请", 785, 523)
    connect_entity_rel("挂号\nregistrations", "R", r["L"], "1", (705, 455))
    connect_rel_entity(r["R"], "检查\nexaminations", "L", "n", (817, 462))

    r = relation("开立", 785, 688)
    connect_entity_rel("挂号\nregistrations", "B", r["L"], "1", (705, 548))
    connect_rel_entity(r["R"], "处方\nprescriptions", "L", "n", (817, 627))

    r = relation("包含", 1160, 688)
    connect_entity_rel("处方\nprescriptions", "R", r["L"], "n", (1080, 655))
    connect_rel_entity(r["R"], "药品\ndrugs", "L", "m", (1220, 655))

    r = relation("形成", 595, 718)
    connect_entity_rel("挂号\nregistrations", "B", r["T"], "1", (617, 472))
    connect_rel_entity(r["B"], "账单\nbills", "T", "1", (617, 785))

    r = relation("支付", 785, 872)
    connect_entity_rel("账单\nbills", "R", r["L"], "1", (705, 837))
    connect_rel_entity(r["R"], "支付\npayments", "L", "1", (817, 837))

    r = relation("审计", 1160, 872)
    connect_entity_rel("账单\nbills", "R", r["L"], "1", (705, 895))
    connect_rel_entity(r["R"], "操作日志\noperation_logs", "L", "n", (1215, 837))
    save(img, path)


def fig_er_core(path: Path):
    img, d = canvas(1700, 820)
    rect(d, (690, 320, 990, 410), "挂号记录\nregistrations", 24)
    attrs = [
        ("挂号单号\nregistration_no", 625, 70),
        ("状态\nstatus", 1040, 112),
        ("费用\nfee", 1060, 325),
        ("挂号时间\nregister_time", 1000, 555),
        ("医保令牌\ninsurance_token_no", 600, 590),
        ("患者ID\npatient_id", 390, 440),
        ("医生ID\ndoctor_id", 375, 220),
        ("号源ID\nschedule_id", 610, 145),
        ("支付身份\npayment_identity", 710, 690),
    ]
    for name, x, y in attrs:
        ellipse(d, (x, y, x + 245, y + 72), name, 21)
        line(d, (840, 365), (x + 122, y + 36))
    rect(d, (150, 120, 330, 180), "患者\npatients", 22)
    rect(d, (1340, 120, 1520, 180), "医生\ndoctors", 22)
    rect(d, (1320, 570, 1545, 640), "排班号源\ndoctor_schedules", 20)
    diamond(d, 520, 150, 120, 70, "预约", 22)
    diamond(d, 1190, 150, 120, 70, "接诊", 22)
    diamond(d, 1190, 600, 120, 70, "占用", 22)
    line(d, (330, 150), (460, 150)); line(d, (580, 150), (700, 350))
    line(d, (980, 350), (1130, 150)); line(d, (1250, 150), (1340, 150))
    line(d, (980, 385), (1130, 600)); line(d, (1250, 600), (1340, 600))
    for xy, t in [((375, 125), "1"), ((635, 225), "n"), ((1040, 230), "n"), ((1290, 125), "1"), ((1050, 500), "n"), ((1290, 575), "1")]:
        label(d, xy, t, 23)
    save(img, path)


def fig_directory(path: Path):
    img, d = canvas(1500, 900)
    rect(d, (540, 70, 960, 125), "HospitalOutpatientSystem", 25)
    items = [
        ("client", ["src/pages", "include/client", "web-dashboard"], 60),
        ("server", ["src/modules", "src/booking", "src/outbox"], 330),
        ("common", ["include/common", "src/Protocol.cpp"], 600),
        ("database", ["schema.sql", "migrations"], 860),
        ("tests", ["auth_router", "schedule", "payment"], 1130),
    ]
    bus_y = 210
    line(d, (750, 125), (750, bus_y))
    line(d, (230, bus_y), (1270, bus_y))
    for name, subs, x in items:
        line(d, (x + 80, bus_y), (x + 80, 285))
        rect(d, (x, 285, x + 160, 340), name, 25)
        y = 410
        line(d, (x + 80, 340), (x + 80, y - 30))
        for s in subs:
            line(d, (x + 80, y - 30), (x + 80, y))
            rect(d, (x + 5, y, x + 155, y + 50), s, 22)
            y += 78
    save(img, path)


def fig_platform_wire(path: Path):
    img, d = canvas(1600, 950)
    rect(d, (90, 80, 1510, 870), width=2)
    rect(d, (90, 80, 360, 870), width=2)
    center_text(d, (115, 105, 335, 155), "MainWindow", 25)
    nav_items = ["院长驾驶舱", "患者管理", "挂号管理", "候诊队列", "患者病历档案", "科室管理", "医生排班", "医生管理",
                 "医生接诊", "检查检验", "处方管理", "药品库存", "收费结算", "费用统计", "操作日志", "权限配置"]
    for i, item in enumerate(nav_items):
        y = 168 + i * 41
        rect(d, (120, y, 330, y + 30), item, 17)
    rect(d, (400, 120, 1460, 190), "RegistrationPage 挂号管理", 26)
    for i, f in enumerate(["患者", "科室", "医生", "就诊日期", "支付身份"]):
        rect(d, (420 + i * 200, 240, 580 + i * 200, 285), f, 20)
    rect(d, (400, 330, 1460, 805), width=2)
    headers = ["挂号单号", "患者", "科室", "医生", "时段", "状态", "费用"]
    col_x = [420, 585, 730, 875, 1020, 1180, 1320, 1440]
    for i, h in enumerate(headers):
        center_text_fit(d, (col_x[i] + 5, 350, col_x[i + 1] - 5, 390), h, 20)
    for y in [410, 475, 540, 605, 670, 735]:
        line(d, (420, y), (1440, y))
    for x in col_x[1:-1]:
        line(d, (x, 330), (x, 805))
    save(img, path)


def fig_login_flow(path: Path):
    img, d = canvas(1200, 1100)
    x = 600
    rounded(d, (470, 70, 730, 125), "开始", 24)
    rect(d, (440, 170, 760, 230), "LoginDialog输入账号密码", 22)
    rect(d, (440, 285, 760, 345), "ApiClient发送 auth/login", 22)
    diamond(d, x, 440, 310, 110, "AuthService\n校验是否通过", 22)
    rect(d, (440, 560, 760, 620), "SessionManager生成Token", 22)
    rect(d, (440, 675, 760, 735), "AuthorizationService加载权限", 21)
    rect(d, (425, 790, 775, 850), "MainWindow按角色加载16个页面", 21)
    rounded(d, (470, 930, 730, 985), "结束", 24)
    for a, b in [((600,125),(600,170)),((600,230),(600,285)),((600,345),(600,385)),((600,495),(600,560)),((600,620),(600,675)),((600,735),(600,790)),((600,850),(600,930))]:
        arrow(d, a, b)
    rect(d, (850, 410, 1070, 470), "返回登录失败", 24)
    arrow(d, (755, 440), (850, 440)); label(d, (780, 405), "否", 23)
    arrow(d, (960, 470), (960, 315)); arrow(d, (960,315),(760,315))
    label(d, (620, 505), "是", 23)
    save(img, path)


def simple_flow(path: Path, title_items: list[str], decision: str | None = None):
    img, d = canvas(1200, 1280)
    x = 600
    rounded(d, (470, 65, 730, 120), "开始", 24)
    y = 170
    last = (600, 120)
    for idx, item in enumerate(title_items):
        if decision and idx == 2:
            diamond(d, x, y + 55, 330, 115, decision, 23)
            arrow(d, last, (600, y))
            last = (600, y + 112)
            y += 165
        rect(d, (350, y, 850, y + 64), item, 23)
        arrow(d, last, (600, y))
        last = (600, y + 64)
        y += 125
    rounded(d, (470, y, 730, y + 55), "结束", 24)
    arrow(d, last, (600, y))
    save(img, path)


def fig_prescription_flow(path: Path):
    img, d = canvas(1250, 1320)
    x = 625
    rounded(d, (495, 55, 755, 110), "开始", 24)
    steps = [
        ("PrescriptionPage.createPrescription", 160),
        ("PrescriptionService写prescriptions", 270),
        ("写prescription_items明细", 380),
        ("药师reviewPrescription", 500),
        ("BillingService确认bills已缴", 790),
        ("药师dispensePrescription发药", 900),
        ("扣减drugs.stock_quantity", 1010),
        ("写stock_records库存流水", 1120),
    ]
    last = (x, 110)
    for text, y in steps[:4]:
        rect(d, (350, y, 900, y + 64), text, 23)
        arrow(d, last, (x, y))
        last = (x, y + 64)
    diamond(d, x, 650, 330, 115, "审核结果\n是否通过", 23)
    arrow(d, last, (x, 592))
    rect(d, (915, 620, 1145, 680), "驳回并记录原因", 22)
    arrow(d, (790, 650), (915, 650))
    label(d, (805, 615), "否", 22)
    arrow(d, (1030, 680), (1030, 1235))
    label(d, (645, 715), "是", 22)
    last = (x, 708)
    for text, y in steps[4:]:
        rect(d, (350, y, 900, y + 64), text, 23)
        arrow(d, last, (x, y))
        last = (x, y + 64)
    rounded(d, (495, 1235, 755, 1290), "结束", 24)
    arrow(d, last, (x, 1235))
    arrow(d, (1030, 1235), (755, 1262))
    save(img, path)


def fig_billing_flow(path: Path):
    simple_flow(path, ["BillingPage查询bills待缴费", "选择SELF_PAY/INSURANCE/QR", "BillingService执行支付", "写入payments", "更新bills.status", "退费申请requestRefund", "退费审核reviewRefund", "汇总fee_statistics_daily"], "支付是否\n成功")


def fig_patient_wire(path: Path):
    img, d = canvas(1500, 900)
    rect(d, (130, 80, 1370, 800), width=2)
    rect(d, (130, 80, 1370, 155), "PatientAppointmentWindow 患者预约挂号", 26)
    labels = ["就诊人", "科室", "医生", "日期", "就诊时段", "支付身份/医保方式"]
    for i, lab in enumerate(labels):
        x = 220 + (i % 3) * 380
        y = 230 + (i // 3) * 140
        label(d, (x, y - 34), lab, 23)
        rect(d, (x, y, x + 270, y + 58), "请选择", 23)
    rect(d, (220, 555, 1280, 650), "可用号源：上午 12 个    下午 9 个        挂号费用：15.00 元", 23)
    rect(d, (980, 700, 1230, 760), "提交预约", 25)
    save(img, path)


def multiline_left(draw: ImageDraw.ImageDraw, xy, lines: list[str], size=20, gap=8):
    x, y = xy
    fnt = font(size)
    for item in lines:
        draw.text((x, y), item, font=fnt, fill="black")
        y += size + gap


def block(draw: ImageDraw.ImageDraw, box, title: str, lines: list[str] | None = None, title_size=24, line_size=19):
    rect(draw, box, width=2)
    x1, y1, x2, y2 = box
    header_h = 46 if lines else y2 - y1
    center_text_fit(draw, (x1 + 8, y1, x2 - 8, y1 + header_h), title, title_size, True)
    if lines:
        draw.line((x1, y1 + header_h, x2, y1 + header_h), fill="black", width=2)
        multiline_left(draw, (x1 + 16, y1 + header_h + 14), lines, line_size, 7)


def anchor(box, side: str):
    x1, y1, x2, y2 = box
    if side == "L":
        return x1, (y1 + y2) // 2
    if side == "R":
        return x2, (y1 + y2) // 2
    if side == "T":
        return (x1 + x2) // 2, y1
    if side == "B":
        return (x1 + x2) // 2, y2
    if side == "C":
        return (x1 + x2) // 2, (y1 + y2) // 2
    raise ValueError(side)


def polyline(draw: ImageDraw.ImageDraw, points: list[tuple[int, int]], width=2, arrow_end=False, dash=False):
    for i in range(len(points) - 1):
        line(draw, points[i], points[i + 1], width, dash)
    if arrow_end and len(points) >= 2:
        a, b = points[-2], points[-1]
        ang = math.atan2(b[1] - a[1], b[0] - a[0])
        size = 14
        pts = [
            b,
            (b[0] - size * math.cos(ang - math.pi / 6), b[1] - size * math.sin(ang - math.pi / 6)),
            (b[0] - size * math.cos(ang + math.pi / 6), b[1] - size * math.sin(ang + math.pi / 6)),
        ]
        draw.polygon(pts, fill="black")


def connect(draw: ImageDraw.ImageDraw, a_box, a_side: str, b_box, b_side: str, label_text: str = "", offset=(0, 0)):
    a = anchor(a_box, a_side)
    b = anchor(b_box, b_side)
    if a_side in ("L", "R") and b_side in ("L", "R"):
        mid_x = (a[0] + b[0]) // 2
        pts = [a, (mid_x, a[1]), (mid_x, b[1]), b]
    elif a_side in ("T", "B") and b_side in ("T", "B"):
        mid_y = (a[1] + b[1]) // 2
        pts = [a, (a[0], mid_y), (b[0], mid_y), b]
    else:
        pts = [a, b]
    polyline(draw, pts, arrow_end=True)
    if label_text:
        lx = (a[0] + b[0]) // 2 + offset[0]
        ly = (a[1] + b[1]) // 2 + offset[1]
        label(draw, (lx, ly), label_text, 19)


def fig_arch(path: Path):
    img, d = canvas(2200, 1280)
    center_text(d, (0, 25, 2200, 90), "系统总体架构图", 34, True)
    columns = [
        (80, 145, 475, 1175, "用户入口层"),
        (570, 145, 965, 1175, "Qt客户端层"),
        (1060, 145, 1545, 1175, "Qt服务端层"),
        (1650, 145, 2120, 1175, "数据与支撑层"),
    ]
    for x1, y1, x2, y2, title in columns:
        rect(d, (x1, y1, x2, y2), width=2)
        center_text(d, (x1, y1 + 10, x2, y1 + 56), title, 25, True)

    staff = (125, 250, 430, 420)
    patient = (125, 620, 430, 790)
    entry = (615, 220, 920, 380)
    api = (615, 500, 920, 670)
    protocol = (615, 790, 920, 930)
    server_conn = (1105, 210, 1500, 355)
    router = (1105, 455, 1500, 625)
    services = (1105, 735, 1500, 970)
    dbmgr = (1695, 230, 2075, 390)
    mysql = (1695, 505, 2075, 735)
    redis = (1695, 850, 2075, 1000)

    block(d, staff, "医院工作站", ["EntryDialog", "LoginDialog", "MainWindow + 16个ModulePage"])
    block(d, patient, "患者预约入口", ["PatientLoginDialog", "PatientAppointmentWindow", "个人资料 + 预约挂号"])
    block(d, entry, "客户端界面组件", ["Qt Widgets", "页面：挂号/排班/接诊/处方/收费等", "按角色权限显示导航"])
    block(d, api, "ApiClient", ["QTcpSocket", "send(common::Request)", "onReadyRead解析响应"])
    block(d, protocol, "共享协议 common", ["Protocol.cpp", "JSON Lines", "module / action / payload"])
    block(d, server_conn, "连接接入", ["HospitalServer监听端口", "ClientConnection按行拆包", "支持模拟扫码支付HTTP入口"])
    block(d, router, "统一路由与鉴权", ["RequestRouter", "SessionManager", "AuthorizationService"])
    block(d, services, "业务服务层", ["Auth / Patient / Registration / Schedule", "Doctor / Consultation / Examination", "Prescription / Inventory / Billing", "Statistics / Dashboard / OperationLog / PermissionAdmin"])
    block(d, dbmgr, "DatabaseManager", ["连接MySQL", "事务与重连", "演示数据初始化"])
    block(d, mysql, "MySQL业务库", ["patients / departments / doctors", "doctor_schedules / registrations", "medical_records / examinations", "prescriptions / drugs / bills / payments", "operation_logs / audit_log_details"])
    block(d, redis, "可选支撑", ["RedisManager", "stock_deduct.lua", "outbox_events"])

    connect(d, staff, "R", entry, "L", "打开医院端")
    connect(d, patient, "R", entry, "L", "打开患者端", (0, 18))
    connect(d, entry, "B", api, "T", "页面请求")
    connect(d, api, "B", protocol, "T", "封装")
    connect(d, protocol, "R", server_conn, "L", "TCP JSON Lines")
    connect(d, server_conn, "B", router, "T", "Request")
    connect(d, router, "B", services, "T", "module/action分发")
    connect(d, services, "R", dbmgr, "L", "SQL访问")
    connect(d, dbmgr, "B", mysql, "T", "QODBC/QMYSQL")
    connect(d, services, "R", redis, "L", "库存/异步扩展", (0, 26))
    save(img, path)


def fig_function_tree(path: Path):
    img, d = canvas(2500, 1120)
    center_text(d, (0, 28, 2500, 90), "系统功能结构图", 34, True)
    root = (900, 105, 1600, 170)
    rect(d, root, "医院门诊挂号与药品管理系统", 27, True)
    groups = [
        ("门户与基础资料", ["院长驾驶舱", "患者管理", "科室管理", "医生管理"]),
        ("挂号排班业务", ["医生排班", "挂号管理", "候诊队列", "患者端预约"]),
        ("诊疗与药房业务", ["医生接诊", "患者病历档案", "检查检验", "处方管理"]),
        ("收费与系统治理", ["药品库存", "收费结算", "费用统计", "操作日志", "权限配置"]),
    ]
    start_x, gap = 165, 575
    group_y, leaf_y, bus_y = 285, 430, 220
    root_c = anchor(root, "B")
    polyline(d, [root_c, (root_c[0], bus_y), (start_x + 220, bus_y), (start_x + gap * 3 + 220, bus_y)])
    for i, (name, items) in enumerate(groups):
        x = start_x + i * gap
        group = (x, group_y, x + 440, group_y + 64)
        rect(d, group, name, 25, True)
        line(d, (x + 220, bus_y), (x + 220, group_y))
        leaf_centers = []
        leaf_w = 72
        total = len(items) * leaf_w + (len(items) - 1) * 24
        leaf_start = x + 220 - total // 2
        for j, item in enumerate(items):
            lx = leaf_start + j * (leaf_w + 24)
            leaf_centers.append(lx + leaf_w // 2)
            rect(d, (lx, leaf_y, lx + leaf_w, leaf_y + 470), vertical_label(item), 24)
        branch_y = 390
        line(d, (x + 220, group_y + 64), (x + 220, branch_y))
        line(d, (leaf_centers[0], branch_y), (leaf_centers[-1], branch_y))
        for lc in leaf_centers:
            line(d, (lc, branch_y), (lc, leaf_y))
    save(img, path)


def fig_tech(path: Path):
    img, d = canvas(1850, 1080)
    center_text(d, (0, 24, 1850, 85), "技术架构图", 34, True)
    layers = [
        ("表现层", ["Qt Widgets", "EntryDialog / LoginDialog / MainWindow", "ModulePage与各业务页面"]),
        ("客户端通信层", ["ApiClient", "QTcpSocket", "异步响应回调与Token透传"]),
        ("公共协议层", ["common::Request / common::Response", "JSON Lines", "Protocol.cpp序列化与反序列化"]),
        ("服务接入与安全层", ["HospitalServer / ClientConnection", "RequestRouter", "SessionManager + AuthorizationService"]),
        ("业务服务层", ["16个ModuleService", "挂号排班、候诊接诊、病历检查、处方库存、收费统计、权限审计"]),
        ("数据持久层", ["DatabaseManager", "MySQL：外键、事务、索引、初始化脚本", "RedisManager和Outbox作为扩展支撑"]),
        ("工程验证层", ["CMake / CMakePresets", "Windows、Linux、银河麒麟运行脚本", "tests目录源码测试和路由测试"]),
    ]
    x1, x2 = 160, 1690
    y = 110
    boxes = []
    for name, lines in layers:
        b = (x1, y, x2, y + 105)
        boxes.append(b)
        rect(d, b, width=2)
        rect(d, (x1, y, x1 + 230, y + 105), name, 25, True)
        multiline_left(d, (x1 + 270, y + 22), lines, 21, 6)
        y += 135
    for a, b in zip(boxes, boxes[1:]):
        connect(d, a, "B", b, "T")
    save(img, path)


def fig_class_initial(path: Path):
    img, d = canvas(2100, 1180)
    center_text(d, (0, 24, 2100, 85), "初步类图设计", 34, True)
    boxes = {
        "EntryDialog": (80, 130, 380, 300),
        "LoginDialog": (80, 390, 380, 600),
        "PatientLoginDialog": (80, 705, 380, 915),
        "MainWindow": (510, 130, 850, 395),
        "ModulePage": (510, 535, 850, 820),
        "PatientAppointmentWindow": (510, 900, 850, 1110),
        "ApiClient": (990, 430, 1330, 710),
        "HospitalServer": (1470, 130, 1810, 330),
        "ClientConnection": (1470, 420, 1810, 620),
        "RequestRouter": (1470, 715, 1810, 970),
    }
    uml_class(d, boxes["EntryDialog"], "EntryDialog", ["- m_choice"], ["+ exec()", "+ selectedChoice()"], 17)
    uml_class(d, boxes["LoginDialog"], "LoginDialog", ["- m_apiClient", "- m_statusLabel"], ["+ loginRequested()", "+ onLoginResponse()"], 17)
    uml_class(d, boxes["PatientLoginDialog"], "PatientLoginDialog", ["- m_apiClient"], ["+ patientLogin()", "+ patientRegister()"], 17)
    uml_class(d, boxes["MainWindow"], "MainWindow", ["- m_apiClient", "- m_navigation", "- m_pages"], ["+ addModulePage()", "+ canAccess()", "+ filterNavigation()"], 17)
    uml_class(d, boxes["ModulePage"], "ModulePage", ["- m_apiClient", "- m_module"], ["+ refresh()", "+ sendRequest()", "+ renderRows()"], 17)
    uml_class(d, boxes["PatientAppointmentWindow"], "PatientAppointmentWindow", ["- m_apiClient", "- m_patientProfile"], ["+ loadDoctors()", "+ submitAppointment()"], 16)
    uml_class(d, boxes["ApiClient"], "ApiClient", ["- m_socket", "- m_token", "- m_patientToken"], ["+ connectToServer()", "+ send()", "+ onReadyRead()"], 17)
    uml_class(d, boxes["HospitalServer"], "HospitalServer", ["- m_server", "- m_router"], ["+ start()", "+ onNewConnection()"], 17)
    uml_class(d, boxes["ClientConnection"], "ClientConnection", ["- m_socket", "- m_buffer"], ["+ onReadyRead()", "+ writeResponse()"], 17)
    uml_class(d, boxes["RequestRouter"], "RequestRouter", ["- m_services", "- m_sessions"], ["+ registerService()", "+ route()", "+ authorize()"], 17)
    db = (1880, 720, 2070, 970)
    uml_class(d, db, "DatabaseManager", ["- m_database"], ["+ ensureOpen()", "+ database()"], 15)
    for src, side in [("LoginDialog", "R"), ("MainWindow", "R"), ("ModulePage", "R"), ("PatientLoginDialog", "R"), ("PatientAppointmentWindow", "R")]:
        connect(d, boxes[src], side, boxes["ApiClient"], "L")
    connect(d, boxes["ApiClient"], "R", boxes["ClientConnection"], "L", "TCP")
    connect(d, boxes["HospitalServer"], "B", boxes["ClientConnection"], "T")
    connect(d, boxes["ClientConnection"], "B", boxes["RequestRouter"], "T")
    connect(d, boxes["RequestRouter"], "R", db, "L")
    save(img, path)


def fig_sequence(path: Path):
    img, d = canvas(2050, 1200)
    center_text(d, (0, 24, 2050, 85), "挂号业务时序图", 34, True)
    actors = [
        ("挂号员/患者", 150),
        ("RegistrationPage\nPatientAppointmentWindow", 500),
        ("ApiClient", 815),
        ("RequestRouter", 1130),
        ("RegistrationService", 1460),
        ("MySQL", 1830),
    ]
    for name, x in actors:
        rect(d, (x - 130, 120, x + 130, 185), name, 21)
        line(d, (x, 185), (x, 1080), dash=True)
    xs = [x for _, x in actors]
    messages = [
        (0, 1, "1. 选择患者、科室、医生、日期和号源"),
        (1, 2, "2. send registration/insurancePrecheck"),
        (2, 3, "3. JSON Lines请求"),
        (3, 4, "4. 会话鉴权并转发"),
        (4, 5, "5. 读取patients/doctors/schedules/insurance"),
        (5, 4, "6. 返回校验结果"),
        (1, 2, "7. send registration/create"),
        (2, 3, "8. 携带Token和payload"),
        (3, 4, "9. route到RegistrationService"),
        (4, 5, "10. 事务：扣减remain_quota"),
        (4, 5, "11. 写registrations和bills"),
        (5, 4, "12. commit并返回registration_no/bill_no"),
        (4, 3, "13. 统一Response"),
        (3, 2, "14. onResponseReceived"),
        (2, 1, "15. 刷新列表和候诊状态"),
    ]
    y = 245
    for a, b, text in messages:
        start = (xs[a] + (16 if a < b else -16), y)
        end = (xs[b] - (16 if a < b else -16), y)
        arrow(d, start, end, dash=a > b)
        center_text_fit(d, (min(start[0], end[0]) + 10, y - 35, max(start[0], end[0]) - 10, y - 8), text, 17)
        y += 55
    save(img, path)


def fig_class_detail(path: Path):
    img, d = canvas(2300, 1350)
    center_text(d, (0, 24, 2300, 85), "详细类图设计", 34, True)
    router = (910, 110, 1390, 280)
    uml_class(d, router, "RequestRouter", ["- m_services", "- m_sessionManager", "- m_authorization"], ["+ registerService(module, service)", "+ route(request)", "+ publicResponseData()"], 18)
    base = (940, 390, 1360, 520)
    uml_class(d, base, "ModuleService", [], ["+ handle(request)"], 18)
    services = [
        "AuthService", "PatientService", "RegistrationService", "DepartmentService",
        "ScheduleService", "DoctorService", "ConsultationService", "ExaminationService",
        "PrescriptionService", "InventoryService", "BillingService", "StatisticsService",
        "DashboardService", "PatientRecordService", "OperationLogService", "PermissionAdminService",
    ]
    service_boxes = []
    for i, name in enumerate(services):
        col, row = i % 4, i // 4
        x = 100 + col * 545
        y = 635 + row * 150
        b = (x, y, x + 420, y + 105)
        service_boxes.append(b)
        uml_class(d, b, name, ["- DatabaseManager* m_database"], ["+ handle(request)"], 14)
    db = (860, 1250, 1440, 1338)
    rect(d, db, "DatabaseManager + MySQL业务库", 23, True)
    connect(d, router, "B", base, "T", "注册并转发")
    bus_y = 580
    line(d, (310, bus_y), (1990, bus_y))
    line(d, (anchor(base, "B")[0], anchor(base, "B")[1]), (anchor(base, "B")[0], bus_y))
    for b in service_boxes:
        line(d, (anchor(b, "T")[0], bus_y), anchor(b, "T"))
    db_bus_y = 1190
    line(d, (310, db_bus_y), (1990, db_bus_y))
    for b in service_boxes:
        line(d, anchor(b, "B"), (anchor(b, "B")[0], db_bus_y))
    arrow(d, (1150, db_bus_y), anchor(db, "T"))
    save(img, path)


def er_table(draw, box, title: str, fields: list[str]):
    rect(draw, box, width=2)
    x1, y1, x2, y2 = box
    draw.line((x1, y1 + 38, x2, y1 + 38), fill="black", width=2)
    center_text_fit(draw, (x1 + 4, y1, x2 - 4, y1 + 38), title, 20, True)
    multiline_left(draw, (x1 + 10, y1 + 50), fields, 16, 4)


def relation_line(draw, a_box, a_side, b_box, b_side, left_card="1", right_card="n"):
    a = anchor(a_box, a_side)
    b = anchor(b_box, b_side)
    if a_side in ("L", "R") and b_side in ("L", "R"):
        mid_x = (a[0] + b[0]) // 2
        pts = [a, (mid_x, a[1]), (mid_x, b[1]), b]
    elif a_side in ("T", "B") and b_side in ("T", "B"):
        mid_y = (a[1] + b[1]) // 2
        pts = [a, (a[0], mid_y), (b[0], mid_y), b]
    else:
        pts = [a, b]
    polyline(draw, pts)
    label(draw, (a[0] + 8, a[1] - 26), left_card, 18)
    label(draw, (b[0] - 22, b[1] - 26), right_card, 18)


def fig_er_total(path: Path):
    img, d = canvas(2600, 1500)
    center_text(d, (0, 22, 2600, 82), "数据库实体关系图", 34, True)
    boxes = {
        "roles": (80, 120, 310, 245),
        "users": (420, 120, 650, 245),
        "departments": (760, 120, 990, 245),
        "doctors": (1100, 120, 1330, 245),
        "doctor_schedules": (1440, 120, 1710, 245),
        "patient_users": (80, 420, 330, 545),
        "patients": (420, 420, 650, 545),
        "registrations": (845, 420, 1135, 565),
        "medical_records": (1280, 360, 1540, 485),
        "examinations": (1280, 560, 1540, 685),
        "prescriptions": (1280, 770, 1540, 895),
        "prescription_items": (1700, 770, 1990, 895),
        "drug_categories": (1700, 1020, 1970, 1145),
        "drugs": (2100, 770, 2360, 895),
        "stock_records": (2100, 1015, 2360, 1140),
        "bills": (845, 1010, 1135, 1145),
        "payments": (1280, 1010, 1540, 1135),
        "insurance_transactions": (1280, 1220, 1590, 1345),
        "operation_logs": (2100, 1220, 2360, 1345),
        "audit_log_details": (1700, 1220, 1990, 1345),
    }
    fields = {
        "roles": ["id PK", "role_code", "role_name"],
        "users": ["id PK", "role_id FK", "username", "status"],
        "departments": ["id PK", "dept_code", "dept_name"],
        "doctors": ["id PK", "user_id FK", "department_id FK", "registration_fee"],
        "doctor_schedules": ["id PK", "doctor_id FK", "work_date", "remain_quota"],
        "patient_users": ["id PK", "username", "password_hash"],
        "patients": ["id PK", "user_id FK", "patient_no", "id_card"],
        "registrations": ["id PK", "patient_id FK", "doctor_id FK", "schedule_id FK", "status / fee"],
        "medical_records": ["id PK", "registration_id FK", "diagnosis", "doctor_id FK"],
        "examinations": ["id PK", "registration_id FK", "item_name", "status"],
        "prescriptions": ["id PK", "registration_id FK", "doctor_id FK", "status"],
        "prescription_items": ["id PK", "prescription_id FK", "drug_id FK", "quantity"],
        "drug_categories": ["id PK", "category_name"],
        "drugs": ["id PK", "category_id FK", "drug_code", "stock_quantity"],
        "stock_records": ["id PK", "drug_id FK", "operator_id FK", "change_type"],
        "bills": ["id PK", "registration_id FK", "patient_id FK", "status"],
        "payments": ["id PK", "bill_id FK", "cashier_id FK", "pay_method"],
        "insurance_transactions": ["id PK", "bill_id FK", "patient_id FK", "status"],
        "operation_logs": ["id PK", "user_id FK", "module", "action"],
        "audit_log_details": ["id PK", "operation_log_id FK", "field_name"],
    }
    for name, b in boxes.items():
        er_table(d, b, name, fields[name])
    relation_line(d, boxes["roles"], "R", boxes["users"], "L")
    relation_line(d, boxes["users"], "R", boxes["doctors"], "L")
    relation_line(d, boxes["departments"], "R", boxes["doctors"], "L")
    relation_line(d, boxes["doctors"], "R", boxes["doctor_schedules"], "L")
    relation_line(d, boxes["patient_users"], "R", boxes["patients"], "L")
    relation_line(d, boxes["patients"], "R", boxes["registrations"], "L")
    relation_line(d, boxes["doctors"], "B", boxes["registrations"], "T")
    relation_line(d, boxes["doctor_schedules"], "B", boxes["registrations"], "T")
    relation_line(d, boxes["registrations"], "R", boxes["medical_records"], "L", "1", "1")
    relation_line(d, boxes["registrations"], "R", boxes["examinations"], "L")
    relation_line(d, boxes["registrations"], "R", boxes["prescriptions"], "L")
    relation_line(d, boxes["prescriptions"], "R", boxes["prescription_items"], "L")
    relation_line(d, boxes["drugs"], "L", boxes["prescription_items"], "R")
    relation_line(d, boxes["drug_categories"], "R", boxes["drugs"], "L")
    relation_line(d, boxes["drugs"], "B", boxes["stock_records"], "T")
    relation_line(d, boxes["registrations"], "B", boxes["bills"], "T", "1", "1")
    relation_line(d, boxes["patients"], "B", boxes["bills"], "L")
    relation_line(d, boxes["bills"], "R", boxes["payments"], "L", "1", "0..1")
    relation_line(d, boxes["bills"], "R", boxes["insurance_transactions"], "L")
    relation_line(d, boxes["users"], "B", boxes["operation_logs"], "T")
    relation_line(d, boxes["operation_logs"], "L", boxes["audit_log_details"], "R", "1", "n")
    save(img, path)


def fig_er_core(path: Path):
    img, d = canvas(2150, 1180)
    center_text(d, (0, 22, 2150, 82), "核心业务 E-R 图", 34, True)
    boxes = {
        "patients": (90, 460, 330, 585),
        "departments": (500, 130, 760, 255),
        "doctors": (920, 130, 1160, 255),
        "doctor_schedules": (1320, 130, 1620, 255),
        "registrations": (860, 460, 1220, 620),
        "medical_records": (1460, 390, 1740, 515),
        "examinations": (1460, 585, 1740, 710),
        "prescriptions": (860, 820, 1160, 945),
        "prescription_items": (1280, 820, 1580, 945),
        "drugs": (1700, 820, 1960, 945),
        "bills": (390, 820, 670, 945),
        "payments": (390, 1010, 670, 1135),
    }
    er_table(d, boxes["patients"], "patients 患者", ["patient_no", "name", "id_card", "phone"])
    er_table(d, boxes["departments"], "departments 科室", ["dept_code", "dept_name", "location"])
    er_table(d, boxes["doctors"], "doctors 医生", ["user_id", "department_id", "title", "fee"])
    er_table(d, boxes["doctor_schedules"], "doctor_schedules 号源", ["doctor_id", "work_date", "period", "remain_quota"])
    er_table(d, boxes["registrations"], "registrations 挂号", ["registration_no", "patient_id / doctor_id / schedule_id", "status", "fee"])
    er_table(d, boxes["medical_records"], "medical_records 病历", ["registration_id", "diagnosis", "advice"])
    er_table(d, boxes["examinations"], "examinations 检查", ["registration_id", "item_name", "status"])
    er_table(d, boxes["prescriptions"], "prescriptions 处方", ["registration_id", "doctor_id", "status"])
    er_table(d, boxes["prescription_items"], "prescription_items 明细", ["prescription_id", "drug_id", "quantity"])
    er_table(d, boxes["drugs"], "drugs 药品", ["drug_code", "sale_price", "stock_quantity"])
    er_table(d, boxes["bills"], "bills 账单", ["registration_id", "patient_id", "total_amount", "status"])
    er_table(d, boxes["payments"], "payments 支付", ["bill_id", "amount", "pay_method"])
    relation_line(d, boxes["patients"], "R", boxes["registrations"], "L")
    relation_line(d, boxes["departments"], "R", boxes["doctors"], "L")
    relation_line(d, boxes["doctors"], "R", boxes["doctor_schedules"], "L")
    relation_line(d, boxes["doctors"], "B", boxes["registrations"], "T")
    relation_line(d, boxes["doctor_schedules"], "B", boxes["registrations"], "T")
    relation_line(d, boxes["registrations"], "R", boxes["medical_records"], "L", "1", "1")
    relation_line(d, boxes["registrations"], "R", boxes["examinations"], "L")
    relation_line(d, boxes["registrations"], "B", boxes["prescriptions"], "T")
    relation_line(d, boxes["prescriptions"], "R", boxes["prescription_items"], "L")
    relation_line(d, boxes["drugs"], "L", boxes["prescription_items"], "R")
    relation_line(d, boxes["registrations"], "B", boxes["bills"], "T", "1", "1")
    relation_line(d, boxes["bills"], "B", boxes["payments"], "T", "1", "0..1")
    save(img, path)


def fig_directory(path: Path):
    img, d = canvas(1900, 1120)
    center_text(d, (0, 22, 1900, 82), "程序目录结构图", 34, True)
    root = (690, 105, 1210, 165)
    rect(d, root, "HospitalOutpatientSystem", 27, True)
    groups = [
        ("client", ["src：入口、ApiClient、窗口", "src/pages：16个业务页面", "include/client：头文件", "web-dashboard：Vue驾驶舱"]),
        ("server", ["main.cpp：注册16个服务", "src/modules：业务服务", "src/booking：预约扩展", "src/outbox：异步事件"]),
        ("common", ["include/common/Protocol.h", "src/Protocol.cpp", "Request / Response协议"]),
        ("database", ["schema.sql", "migrations", "MySQL业务表与演示数据"]),
        ("config/scripts", ["server.example.ini", "windows/linux/kylin脚本", "构建与运行入口"]),
        ("tests/docs", ["tests：路由、排班、支付等验证", "docs：论文与图表生成脚本", "output：生成文档与图片"]),
    ]
    start_x, gap = 80, 300
    bus_y = 245
    line(d, anchor(root, "B"), (anchor(root, "B")[0], bus_y))
    line(d, (start_x + 110, bus_y), (start_x + gap * 5 + 110, bus_y))
    for i, (name, lines_) in enumerate(groups):
        x = start_x + i * gap
        b = (x, 330, x + 220, 405)
        rect(d, b, name, 24, True)
        line(d, (x + 110, bus_y), (x + 110, 330))
        y = 480
        for item in lines_:
            rect(d, (x - 25, y, x + 245, y + 72), item, 19)
            line(d, (x + 110, y - 35), (x + 110, y))
            y += 105
    save(img, path)


def fig_platform_wire(path: Path):
    img, d = canvas(1900, 1080)
    center_text(d, (0, 22, 1900, 82), "平台端主界面布局图", 34, True)
    outer = (120, 120, 1780, 980)
    rect(d, outer, width=2)
    top = (120, 120, 1780, 205)
    side = (120, 205, 420, 980)
    rect(d, top, width=2)
    rect(d, side, width=2)
    center_text(d, (150, 138, 600, 185), "MainWindow 医院工作站", 25, True)
    rect(d, (1390, 145, 1585, 180), "当前用户/角色", 18)
    rect(d, (1610, 145, 1740, 180), "退出登录", 18)
    nav = ["院长驾驶舱", "患者管理", "挂号管理", "候诊队列", "患者病历档案", "科室管理", "医生排班", "医生管理",
           "医生接诊", "检查检验", "处方管理", "药品库存", "收费结算", "费用统计", "操作日志", "权限配置"]
    for i, item in enumerate(nav):
        y = 225 + i * 43
        rect(d, (150, y, 390, y + 31), item, 17)
    content = (465, 245, 1735, 930)
    rect(d, content, width=2)
    rect(d, (500, 280, 1685, 350), "RegistrationPage / ModulePage 业务页面区域", 24, True)
    filters = ["患者", "科室", "医生", "日期", "支付身份", "状态"]
    for i, item in enumerate(filters):
        x = 510 + i * 185
        rect(d, (x, 390, x + 150, 435), item, 18)
    for i, item in enumerate(["刷新", "新增", "修改", "退号", "导出"]):
        x = 1390 + i * 66
        rect(d, (x, 390, x + 54, 435), item, 17)
    table = (510, 485, 1685, 875)
    rect(d, table, width=2)
    headers = ["挂号单号", "患者", "科室", "医生", "时段", "状态", "费用", "操作"]
    col_w = (table[2] - table[0]) // len(headers)
    for i, h in enumerate(headers):
        x1 = table[0] + i * col_w
        x2 = table[0] + (i + 1) * col_w if i < len(headers) - 1 else table[2]
        line(d, (x1, table[1]), (x1, table[3]))
        center_text_fit(d, (x1 + 5, table[1] + 8, x2 - 5, table[1] + 48), h, 18, True)
    line(d, (table[2], table[1]), (table[2], table[3]))
    for y in [540, 600, 660, 720, 780, 840]:
        line(d, (table[0], y), (table[2], y))
    save(img, path)


def fig_login_flow(path: Path):
    img, d = canvas(1450, 1300)
    center_text(d, (0, 22, 1450, 82), "登录权限控制流程图", 34, True)
    start = (590, 110, 860, 165)
    choice = (535, 245, 915, 335)
    staff = (195, 430, 525, 500)
    patient = (925, 430, 1255, 500)
    auth = (175, 590, 545, 680)
    p_auth = (905, 590, 1275, 680)
    token = (195, 790, 525, 860)
    p_token = (925, 790, 1255, 860)
    main = (175, 970, 545, 1045)
    p_win = (905, 970, 1275, 1045)
    end = (590, 1160, 860, 1215)
    rounded(d, start, "开始", 24)
    diamond(d, 725, 290, 380, 120, "EntryDialog\n选择入口", 23)
    rect(d, staff, "LoginDialog\n院内账号登录", 22)
    rect(d, patient, "PatientLoginDialog\n患者登录/注册", 22)
    diamond(d, 360, 635, 370, 120, "AuthService\n账号和角色是否有效", 21)
    diamond(d, 1090, 635, 370, 120, "PatientService\n患者账号是否有效", 21)
    rect(d, token, "SessionManager\n生成员工Token", 22)
    rect(d, p_token, "保存患者Token\n绑定patient_id", 22)
    rect(d, main, "MainWindow\n按权限加载模块", 22)
    rect(d, p_win, "PatientAppointmentWindow\n加载个人预约功能", 21)
    rounded(d, end, "结束", 24)
    arrow(d, anchor(start, "B"), (725, 230))
    arrow(d, (560, 335), anchor(staff, "T"))
    arrow(d, (890, 335), anchor(patient, "T"))
    arrow(d, anchor(staff, "B"), (360, 575))
    arrow(d, anchor(patient, "B"), (1090, 575))
    arrow(d, (360, 695), anchor(token, "T")); label(d, (380, 700), "是", 20)
    arrow(d, (1090, 695), anchor(p_token, "T")); label(d, (1110, 700), "是", 20)
    arrow(d, anchor(token, "B"), anchor(main, "T"))
    arrow(d, anchor(p_token, "B"), anchor(p_win, "T"))
    connect(d, main, "B", end, "T")
    connect(d, p_win, "B", end, "T")
    fail1 = (60, 610, 140, 660)
    fail2 = (1310, 610, 1390, 660)
    rect(d, fail1, "失败", 18)
    rect(d, fail2, "失败", 18)
    arrow(d, (175, 635), anchor(fail1, "R")); label(d, (145, 595), "否", 20)
    arrow(d, (1275, 635), anchor(fail2, "L")); label(d, (1280, 595), "否", 20)
    save(img, path)


def fig_prescription_flow(path: Path):
    img, d = canvas(1500, 1500)
    center_text(d, (0, 22, 1500, 82), "处方审核与发药流程图", 34, True)
    x = 750
    steps = [
        ((555, 110, 945, 165), "开始"),
        ((500, 235, 1000, 300), "医生接诊页创建处方\nPrescriptionService::create"),
        ((500, 370, 1000, 435), "写prescriptions与prescription_items"),
        ((500, 505, 1000, 570), "读取pass_rules进行用药规则提示"),
        ((500, 805, 1000, 870), "药师审核\nreview / reject"),
        ((500, 1045, 1000, 1110), "收费完成后检查bills.status=PAID"),
        ((500, 1180, 1000, 1245), "发药dispense：校验库存并扣减drugs"),
        ((500, 1315, 1000, 1380), "写stock_records并更新处方状态DISPENSED"),
        ((555, 1440, 945, 1490), "结束"),
    ]
    rounded(d, steps[0][0], steps[0][1], 23)
    for b, text in steps[1:-1]:
        rect(d, b, text, 22)
    rounded(d, steps[-1][0], steps[-1][1], 23)
    for (a, _), (b, _) in zip(steps[:4], steps[1:5]):
        arrow(d, anchor(a, "B"), anchor(b, "T"))
    diamond(d, x, 685, 360, 120, "规则是否阻断\n或药师是否驳回", 21)
    arrow(d, anchor(steps[4][0], "B"), (x, 625))
    arrow(d, (x, 745), anchor(steps[5][0], "T")); label(d, (770, 750), "否", 20)
    reject = (1075, 655, 1370, 725)
    rect(d, reject, "返回驳回原因\n状态REJECTED", 21)
    arrow(d, (930, 685), anchor(reject, "L")); label(d, (965, 650), "是", 20)
    connect(d, reject, "B", steps[-1][0], "R")
    for (a, _), (b, _) in zip(steps[5:-1], steps[6:]):
        arrow(d, anchor(a, "B"), anchor(b, "T"))
    ret = (120, 1180, 405, 1245)
    rect(d, ret, "退药return\n回补库存", 21)
    connect(d, steps[7][0], "L", ret, "R", "退药")
    save(img, path)


def fig_billing_flow(path: Path):
    img, d = canvas(1500, 1420)
    center_text(d, (0, 22, 1500, 82), "收费结算与退费流程图", 34, True)
    start = (555, 100, 945, 155)
    query = (470, 225, 1030, 290)
    choose = (555, 380, 945, 490)
    self_pay = (150, 600, 470, 670)
    insurance = (590, 600, 910, 670)
    scan = (1030, 600, 1350, 670)
    payment = (470, 790, 1030, 855)
    bill = (470, 945, 1030, 1010)
    refund = (470, 1100, 1030, 1165)
    review = (470, 1250, 1030, 1315)
    end = (555, 1360, 945, 1410)
    rounded(d, start, "开始", 23)
    rect(d, query, "BillingPage查询待缴账单\n来源：挂号费、检查费、药品费", 22)
    diamond(d, 750, 435, 390, 120, "选择支付方式", 22)
    rect(d, self_pay, "自费支付\nprocessSelfPay", 21)
    rect(d, insurance, "医保模拟\nprocessMedicalInsurancePay", 21)
    rect(d, scan, "扫码支付\ncreatePaymentQr/checkPayStatus", 20)
    rect(d, payment, "写payments支付记录\ncashier_id、amount、pay_method", 22)
    rect(d, bill, "更新bills.status与pay_time\nPAID / UNPAID / CANCELLED", 22)
    rect(d, refund, "退费申请requestRefund\n生成退款待审状态", 22)
    rect(d, review, "退费审核reviewRefund\n通过后标记REFUNDED并写日志", 21)
    rounded(d, end, "结束", 23)
    arrow(d, anchor(start, "B"), anchor(query, "T"))
    arrow(d, anchor(query, "B"), (750, 375))
    arrow(d, (620, 490), anchor(self_pay, "T"))
    arrow(d, (750, 490), anchor(insurance, "T"))
    arrow(d, (880, 490), anchor(scan, "T"))
    connect(d, self_pay, "B", payment, "T")
    connect(d, insurance, "B", payment, "T")
    connect(d, scan, "B", payment, "T")
    arrow(d, anchor(payment, "B"), anchor(bill, "T"))
    arrow(d, anchor(bill, "B"), anchor(refund, "T"))
    arrow(d, anchor(refund, "B"), anchor(review, "T"))
    arrow(d, anchor(review, "B"), anchor(end, "T"))
    save(img, path)


def fig_patient_wire(path: Path):
    img, d = canvas(1800, 1040)
    center_text(d, (0, 22, 1800, 82), "患者端预约界面布局图", 34, True)
    outer = (130, 120, 1670, 940)
    rect(d, outer, width=2)
    rect(d, (130, 120, 1670, 205), "PatientAppointmentWindow 患者预约入口", 26, True)
    rect(d, (1450, 148, 1615, 180), "个人中心", 18)
    left = (170, 245, 560, 870)
    right = (610, 245, 1630, 870)
    rect(d, left, width=2)
    rect(d, right, width=2)
    rect(d, (200, 285, 530, 350), "就诊人信息", 22, True)
    for i, item in enumerate(["姓名", "身份证号", "手机号", "关系"]):
        y = 395 + i * 85
        label(d, (210, y - 28), item, 20)
        rect(d, (300, y - 36, 520, y + 8), "已绑定/可维护", 17)
    rect(d, (200, 760, 530, 820), "新增家庭成员", 21)
    rect(d, (650, 285, 1590, 350), "预约挂号表单", 22, True)
    fields = ["科室", "医生", "就诊日期", "号源时段", "支付身份", "医保预校验"]
    for i, item in enumerate(fields):
        x = 680 + (i % 3) * 300
        y = 410 + (i // 3) * 130
        label(d, (x, y - 30), item, 20)
        rect(d, (x, y, x + 235, y + 50), "请选择", 19)
    rect(d, (680, 685, 1250, 745), "可用号源：remain_quota  剩余号源", 20)
    rect(d, (1300, 685, 1545, 745), "提交预约", 23)
    rect(d, (680, 790, 1545, 840), "我的预约记录：registration_no / doctor / time_slot / status / bill_no", 18)
    save(img, path)


def fig_function_tree(path: Path):
    img, d = canvas(3600, 1320)
    root = (1320, 70, 2280, 140)
    rect(d, root, "医院门诊挂号与药品管理系统", 30, True)
    modules = [
        ("院长驾驶舱", ["今日挂号", "收入汇总", "库存预警"]),
        ("患者管理", ["患者建档", "资料查询", "家庭成员"]),
        ("挂号管理", ["预约挂号", "医保校验", "退号处理"]),
        ("候诊队列", ["候诊统计", "叫号提醒", "急诊优先"]),
        ("患者病历档案", ["病历查询", "病历维护", "外院报告"]),
        ("科室管理", ["科室新增", "科室修改", "科室停用"]),
        ("医生排班", ["智能排班", "号源维护", "停诊规则"]),
        ("医生管理", ["医生资料", "科室归属", "出诊费用"]),
        ("医生接诊", ["叫号接诊", "病历填写", "开立医嘱"]),
        ("检查检验", ["检查项目", "检查申请", "结果录入"]),
        ("处方管理", ["处方开立", "处方审核", "发药退药"]),
        ("药品库存", ["药品维护", "入库出库", "库存预警"]),
        ("收费结算", ["账单查询", "自费医保", "退费审核"]),
        ("费用统计", ["日收入", "科室收入", "导出报表"]),
        ("操作日志", ["操作记录", "审计明细", "变更追踪"]),
        ("权限配置", ["用户管理", "角色授权", "菜单权限"]),
    ]
    start_x, gap = 70, 215
    bus_y, module_y, leaf_y = 220, 305, 455
    root_c = anchor(root, "B")
    line(d, root_c, (root_c[0], bus_y))
    line(d, (start_x + 80, bus_y), (start_x + gap * (len(modules) - 1) + 80, bus_y))
    for i, (name, items) in enumerate(modules):
        x = start_x + i * gap
        module = (x, module_y, x + 160, module_y + 66)
        rect(d, module, name, 22, True)
        line(d, (x + 80, bus_y), (x + 80, module_y))
        leaf_w, leaf_gap = 44, 12
        total = len(items) * leaf_w + (len(items) - 1) * leaf_gap
        leaf_start = x + 80 - total // 2
        centers = []
        for j, item in enumerate(items):
            lx = leaf_start + j * (leaf_w + leaf_gap)
            b = (lx, leaf_y, lx + leaf_w, leaf_y + 620)
            centers.append(lx + leaf_w // 2)
            rect(d, b, vertical_label(item), 21)
        branch_y = 420
        line(d, (x + 80, module_y + 66), (x + 80, branch_y))
        line(d, (centers[0], branch_y), (centers[-1], branch_y))
        for cx in centers:
            line(d, (cx, branch_y), (cx, leaf_y))
    save(img, path)


def fig_class_detail(path: Path):
    img, d = canvas(2600, 1680)
    router = (990, 65, 1610, 250)
    uml_class(d, router, "RequestRouter", ["- m_services", "- m_database", "- m_sessions"], ["+ registerService(module, service)", "+ route(request)", "+ authorize(request)", "+ writeOperationLog()"], 18)
    base = (1065, 360, 1535, 500)
    uml_class(d, base, "ModuleService", [], ["+ handle(request)"], 18)
    services = [
        ("AuthService", "登录/患者账号"),
        ("PatientService", "患者档案"),
        ("RegistrationService", "挂号/退号/候诊"),
        ("DepartmentService", "科室维护"),
        ("ScheduleService", "排班/号源"),
        ("DoctorService", "医生维护"),
        ("ConsultationService", "接诊/病历入口"),
        ("ExaminationService", "检查项目/结果"),
        ("PrescriptionService", "处方/审核/发药"),
        ("InventoryService", "药品库存"),
        ("BillingService", "收费/退费/医保"),
        ("StatisticsService", "费用统计"),
        ("DashboardService", "院长驾驶舱"),
        ("PatientRecordService", "病历档案"),
        ("OperationLogService", "日志审计"),
        ("PermissionAdminService", "用户角色权限"),
    ]
    service_boxes = []
    for i, (name, desc) in enumerate(services):
        col, row = i % 4, i // 4
        x = 115 + col * 605
        y = 640 + row * 195
        b = (x, y, x + 490, y + 145)
        service_boxes.append(b)
        uml_class(d, b, name, ["- DatabaseManager* m_database", f"- {desc}"], ["+ handle(request)"], 15)

    db = (905, 1545, 1695, 1640)
    rect(d, db, "DatabaseManager + MySQL（业务表、权限表、审计表、Outbox）", 22, True)

    connect(d, router, "B", base, "T", "转发到服务基类")
    group_box = (70, 590, 2530, 1425)
    rect(d, group_box, width=2)
    label(d, (95, 602), "业务服务实现类（继承 ModuleService，均重写 handle(request)）", 22)
    arrow(d, anchor(base, "B"), (1300, group_box[1]))
    arrow(d, (1300, group_box[3]), anchor(db, "T"))
    label(d, (1320, 1460), "统一数据访问", 20)
    save(img, path)


def relation_line(draw, a_box, a_side, b_box, b_side, left_card="1", right_card="n", via=None):
    a = anchor(a_box, a_side)
    b = anchor(b_box, b_side)
    if via:
        pts = [a] + via + [b]
    elif a_side in ("L", "R") and b_side in ("L", "R"):
        mid_x = (a[0] + b[0]) // 2
        pts = [a, (mid_x, a[1]), (mid_x, b[1]), b]
    elif a_side in ("T", "B") and b_side in ("T", "B"):
        mid_y = (a[1] + b[1]) // 2
        pts = [a, (a[0], mid_y), (b[0], mid_y), b]
    else:
        pts = [a, b]
    polyline(draw, pts)
    label(draw, (a[0] + 8, a[1] - 28), left_card, 18)
    label(draw, (b[0] - 24, b[1] - 28), right_card, 18)


def fig_er_total(path: Path):
    img, d = canvas(3200, 1850)
    boxes = {
        "roles": (90, 120, 350, 255),
        "users": (470, 120, 730, 255),
        "departments": (850, 120, 1110, 255),
        "doctors": (1230, 120, 1490, 255),
        "doctor_schedules": (1610, 120, 1910, 255),
        "patient_users": (90, 480, 370, 615),
        "patients": (470, 480, 730, 615),
        "registrations": (1120, 480, 1480, 635),
        "medical_records": (1740, 420, 2050, 555),
        "examinations": (1740, 660, 2050, 795),
        "examination_items": (2220, 660, 2520, 795),
        "prescriptions": (1120, 900, 1480, 1035),
        "prescription_items": (1740, 900, 2050, 1035),
        "drug_categories": (2220, 1090, 2520, 1225),
        "drugs": (2220, 900, 2520, 1035),
        "pass_rules": (2700, 900, 3010, 1035),
        "stock_records": (2220, 1300, 2520, 1435),
        "bills": (1120, 1300, 1480, 1455),
        "payments": (1740, 1300, 2050, 1435),
        "insurance_transactions": (1740, 1535, 2080, 1670),
        "fee_statistics_daily": (760, 1535, 1080, 1670),
        "operation_logs": (2700, 1300, 3010, 1435),
        "audit_log_details": (2700, 1535, 3010, 1670),
    }
    fields = {
        "roles": ["id PK", "role_code", "role_name"],
        "users": ["id PK", "role_id FK", "username", "status"],
        "departments": ["id PK", "dept_code", "dept_name"],
        "doctors": ["id PK", "user_id FK", "department_id FK", "registration_fee"],
        "doctor_schedules": ["id PK", "doctor_id FK", "work_date", "remain_quota"],
        "patient_users": ["id PK", "username", "password_hash"],
        "patients": ["id PK", "user_id FK", "patient_no", "id_card"],
        "registrations": ["id PK", "patient_id FK", "doctor_id FK", "schedule_id FK", "status / fee"],
        "medical_records": ["id PK", "registration_id FK", "diagnosis", "doctor_id FK"],
        "examinations": ["id PK", "registration_id FK", "item_id FK", "status"],
        "examination_items": ["id PK", "item_code", "item_name", "unit_price"],
        "prescriptions": ["id PK", "registration_id FK", "doctor_id FK", "status"],
        "prescription_items": ["id PK", "prescription_id FK", "drug_id FK", "quantity"],
        "drug_categories": ["id PK", "category_name"],
        "drugs": ["id PK", "category_id FK", "drug_code", "stock_quantity"],
        "pass_rules": ["id PK", "rule_type", "drug_name", "warning_level"],
        "stock_records": ["id PK", "drug_id FK", "operator_id FK", "change_type"],
        "bills": ["id PK", "registration_id FK", "patient_id FK", "total_amount / status"],
        "payments": ["id PK", "bill_id FK", "cashier_id FK", "pay_method"],
        "insurance_transactions": ["id PK", "bill_id FK", "patient_id FK", "status"],
        "fee_statistics_daily": ["id PK", "stat_date", "department_id FK", "total_income"],
        "operation_logs": ["id PK", "user_id FK", "module", "action"],
        "audit_log_details": ["id PK", "operation_log_id FK", "field_name"],
    }
    for name, b in boxes.items():
        er_table(d, b, name, fields[name])
    relation_line(d, boxes["roles"], "R", boxes["users"], "L")
    relation_line(d, boxes["users"], "R", boxes["doctors"], "L")
    relation_line(d, boxes["departments"], "R", boxes["doctors"], "L")
    relation_line(d, boxes["doctors"], "R", boxes["doctor_schedules"], "L")
    relation_line(d, boxes["patient_users"], "R", boxes["patients"], "L")
    relation_line(d, boxes["patients"], "R", boxes["registrations"], "L")
    relation_line(d, boxes["doctors"], "B", boxes["registrations"], "T", via=[(1360, 345), (1300, 345)])
    relation_line(d, boxes["doctor_schedules"], "B", boxes["registrations"], "T", via=[(1760, 360), (1300, 360)])
    relation_line(d, boxes["registrations"], "R", boxes["medical_records"], "L", "1", "1")
    relation_line(d, boxes["registrations"], "R", boxes["examinations"], "L")
    relation_line(d, boxes["examination_items"], "L", boxes["examinations"], "R", "1", "n")
    relation_line(d, boxes["registrations"], "B", boxes["prescriptions"], "T")
    relation_line(d, boxes["prescriptions"], "R", boxes["prescription_items"], "L")
    relation_line(d, boxes["drugs"], "L", boxes["prescription_items"], "R")
    relation_line(d, boxes["drug_categories"], "T", boxes["drugs"], "B", "1", "n", via=[(2370, 1060)])
    relation_line(d, boxes["drugs"], "R", boxes["pass_rules"], "L", "1", "n")
    relation_line(d, boxes["drugs"], "B", boxes["stock_records"], "T")
    relation_line(d, boxes["registrations"], "B", boxes["bills"], "T", "1", "1", via=[(1300, 760), (1300, 760)])
    relation_line(d, boxes["patients"], "B", boxes["bills"], "L", via=[(600, 760), (1010, 760), (1010, 1378)])
    relation_line(d, boxes["bills"], "R", boxes["payments"], "L", "1", "0..1")
    relation_line(d, boxes["bills"], "R", boxes["insurance_transactions"], "L", "1", "n", via=[(1600, 1380), (1600, 1602)])
    relation_line(d, boxes["departments"], "B", boxes["fee_statistics_daily"], "T", "1", "n", via=[(980, 330), (920, 330), (920, 1535)])
    relation_line(d, boxes["operation_logs"], "B", boxes["audit_log_details"], "T", "1", "n")
    save(img, path)


def fig_er_core(path: Path):
    img, d = canvas(2600, 1450)
    boxes = {
        "patients": (90, 500, 390, 640),
        "departments": (560, 130, 860, 270),
        "doctors": (1040, 130, 1340, 270),
        "doctor_schedules": (1520, 130, 1870, 270),
        "registrations": (1010, 520, 1370, 690),
        "medical_records": (1640, 450, 1960, 590),
        "examinations": (1640, 680, 1960, 820),
        "prescriptions": (1010, 900, 1370, 1040),
        "prescription_items": (1550, 900, 1900, 1040),
        "drugs": (2090, 900, 2390, 1040),
        "bills": (560, 900, 860, 1040),
        "payments": (560, 1180, 860, 1320),
    }
    er_table(d, boxes["patients"], "patients 患者", ["patient_no", "name", "id_card", "phone"])
    er_table(d, boxes["departments"], "departments 科室", ["dept_code", "dept_name", "location"])
    er_table(d, boxes["doctors"], "doctors 医生", ["user_id FK", "department_id FK", "title", "registration_fee"])
    er_table(d, boxes["doctor_schedules"], "doctor_schedules 号源", ["doctor_id FK", "work_date", "period", "remain_quota"])
    er_table(d, boxes["registrations"], "registrations 挂号", ["registration_no", "patient_id / doctor_id / schedule_id", "status", "payment_identity"])
    er_table(d, boxes["medical_records"], "medical_records 病历", ["registration_id FK", "diagnosis", "advice", "doctor_id FK"])
    er_table(d, boxes["examinations"], "examinations 检查", ["registration_id FK", "item_name", "unit_price", "status"])
    er_table(d, boxes["prescriptions"], "prescriptions 处方", ["registration_id FK", "doctor_id FK", "status", "total_amount"])
    er_table(d, boxes["prescription_items"], "prescription_items 明细", ["prescription_id FK", "drug_id FK", "quantity", "amount"])
    er_table(d, boxes["drugs"], "drugs 药品", ["drug_code", "sale_price", "stock_quantity"])
    er_table(d, boxes["bills"], "bills 账单", ["registration_id FK", "patient_id FK", "total_amount", "status"])
    er_table(d, boxes["payments"], "payments 支付", ["bill_id FK", "amount", "pay_method"])
    relation_line(d, boxes["patients"], "R", boxes["registrations"], "L", via=[(500, 570), (500, 605)])
    relation_line(d, boxes["departments"], "R", boxes["doctors"], "L")
    relation_line(d, boxes["doctors"], "R", boxes["doctor_schedules"], "L")
    relation_line(d, boxes["doctors"], "B", boxes["registrations"], "T", via=[(1190, 360), (1110, 360)])
    relation_line(d, boxes["doctor_schedules"], "B", boxes["registrations"], "T", via=[(1695, 360), (1290, 360)])
    relation_line(d, boxes["registrations"], "R", boxes["medical_records"], "L", "1", "1")
    relation_line(d, boxes["registrations"], "R", boxes["examinations"], "L")
    relation_line(d, boxes["registrations"], "B", boxes["prescriptions"], "T")
    relation_line(d, boxes["prescriptions"], "R", boxes["prescription_items"], "L")
    relation_line(d, boxes["drugs"], "L", boxes["prescription_items"], "R")
    relation_line(d, boxes["registrations"], "B", boxes["bills"], "T", "1", "1", via=[(1190, 790), (710, 790)])
    relation_line(d, boxes["bills"], "B", boxes["payments"], "T", "1", "0..1")
    save(img, path)


def generate_figures() -> list[Path]:
    ASSET_DIR.mkdir(parents=True, exist_ok=True)
    specs = [
        ("figure_3_1_architecture.png", fig_arch),
        ("figure_3_2_function_tree.png", fig_function_tree),
        ("figure_3_3_tech_layers.png", fig_tech),
        ("figure_3_4_class_initial.png", fig_class_initial),
        ("figure_3_5_sequence_registration.png", fig_sequence),
        ("figure_3_6_class_detail.png", fig_class_detail),
        ("figure_3_7_er_total.png", fig_er_total),
        ("figure_3_8_er_core.png", fig_er_core),
        ("figure_4_1_directory.png", fig_directory),
        ("figure_4_2_platform_wire.png", fig_platform_wire),
        ("figure_4_3_login_flow.png", fig_login_flow),
        ("figure_4_4_prescription_flow.png", fig_prescription_flow),
        ("figure_4_5_billing_flow.png", fig_billing_flow),
        ("figure_4_6_patient_wire.png", fig_patient_wire),
    ]
    paths = []
    for name, fn in specs:
        path = ASSET_DIR / name
        fn(path)
        if name not in {
            "figure_3_2_function_tree.png",
            "figure_3_6_class_detail.png",
            "figure_3_7_er_total.png",
            "figure_3_8_er_core.png",
        }:
            strip_top_title(path)
        paths.append(path)
    return paths


def qn(tag: str) -> str:
    return f"{{{W_NS}}}{tag}"


def set_border(el, val: str, size: str = "8"):
    el.set(qn("val"), val)
    el.set(qn("sz"), size)
    el.set(qn("space"), "0")
    el.set(qn("color"), "000000")


def get_or_add(parent, tag: str):
    found = parent.find(qn(tag))
    if found is None:
        found = etree.SubElement(parent, qn(tag))
    return found


def clear_all_table_borders(tbl):
    tbl_pr = get_or_add(tbl, "tblPr")
    borders = get_or_add(tbl_pr, "tblBorders")
    for edge in ("top", "left", "bottom", "right", "insideH", "insideV"):
        set_border(get_or_add(borders, edge), "nil")
    for tc in tbl.xpath(".//w:tc", namespaces=NS):
        tc_pr = get_or_add(tc, "tcPr")
        tc_borders = get_or_add(tc_pr, "tcBorders")
        for edge in ("top", "left", "bottom", "right", "insideH", "insideV"):
            set_border(get_or_add(tc_borders, edge), "nil")


def set_cell_edge(tc, edge: str, size: str):
    tc_pr = get_or_add(tc, "tcPr")
    tc_borders = get_or_add(tc_pr, "tcBorders")
    set_border(get_or_add(tc_borders, edge), "single", size)


def apply_three_line_tables(document_xml: bytes) -> bytes:
    doc = etree.fromstring(document_xml)
    tables = doc.xpath(".//w:tbl", namespaces=NS)
    # Table 0 is the cover form. Keep it as a form; convert only numbered正文 tables.
    for tbl in tables[1:]:
        clear_all_table_borders(tbl)
        rows = tbl.xpath("./w:tr", namespaces=NS)
        if not rows:
            continue
        for tc in rows[0].xpath("./w:tc", namespaces=NS):
            set_cell_edge(tc, "top", "12")
            set_cell_edge(tc, "bottom", "8")
        for tc in rows[-1].xpath("./w:tc", namespaces=NS):
            set_cell_edge(tc, "bottom", "12")
        # Make table width full and remove cell shading inherited from old styles.
        tbl_pr = get_or_add(tbl, "tblPr")
        tbl_w = get_or_add(tbl_pr, "tblW")
        tbl_w.set(qn("w"), "5000")
        tbl_w.set(qn("type"), "pct")
        for shd in tbl.xpath(".//w:shd", namespaces=NS):
            parent = shd.getparent()
            if parent is not None:
                parent.remove(shd)
    return etree.tostring(doc, xml_declaration=True, encoding="UTF-8", standalone="yes")


def paragraph_text(paragraph) -> str:
    return "".join(paragraph.xpath(".//w:t/text()", namespaces=NS))


def make_reference_paragraph(text: str):
    p = etree.Element(qn("p"))
    p_pr = etree.SubElement(p, qn("pPr"))
    ind = etree.SubElement(p_pr, qn("ind"))
    ind.set(qn("firstLineChars"), "200")
    jc = etree.SubElement(p_pr, qn("jc"))
    jc.set(qn("val"), "both")
    r = etree.SubElement(p, qn("r"))
    t = etree.SubElement(r, qn("t"))
    t.text = text
    return p


def caption_to_reference(caption: str) -> str:
    mapping = {
        "图3-1": "系统采用客户端、服务端和数据库分层协同的总体结构，系统总体架构如图3-1所示。",
        "图3-2": "根据项目实际运行界面中的功能菜单，系统功能结构如图3-2所示。",
        "图3-3": "系统技术实现由Qt界面、TCP通信、服务路由和MySQL持久化等部分组成，技术架构如图3-3所示。",
        "图3-4": "围绕客户端窗口、通信组件、服务端路由和数据访问对象，系统初步类关系如图3-4所示。",
        "图3-5": "以挂号业务为例，客户端、路由器、业务服务和数据库之间的交互过程如图3-5所示。",
        "图3-6": "服务端各业务服务均通过统一路由注册并访问数据库，详细类结构如图3-6所示。",
        "图3-7": "系统主要数据表及其外键关联关系如图3-7所示。",
        "图3-8": "围绕患者挂号、接诊、处方、账单和支付形成的核心业务数据关系如图3-8所示。",
        "图4-1": "项目源码目录按照客户端、服务端、公共协议、数据库脚本和测试文档组织，目录结构如图4-1所示。",
        "图4-2": "医院端工作人员登录后进入平台端主界面，界面布局如图4-2所示。",
        "图4-3": "系统登录后通过会话Token和角色权限控制模块访问，登录权限控制流程如图4-3所示。",
        "图4-4": "处方从医生开立到药师审核、收费后发药的处理过程如图4-4所示。",
        "图4-5": "收费结算支持自费、医保模拟支付和退费审核，收费结算流程如图4-5所示。",
        "图4-6": "患者端提供就诊人维护、号源选择和预约提交等功能，预约界面布局如图4-6所示。",
    }
    for key, sentence in mapping.items():
        if caption.startswith(key):
            return sentence
    return ""


def ensure_figure_references(document_xml: bytes) -> bytes:
    doc = etree.fromstring(document_xml)
    body = doc.find(".//w:body", namespaces=NS)
    if body is None:
        return document_xml
    children = list(body)
    i = 0
    while i < len(children):
        child = children[i]
        if child.tag != qn("p") or not child.xpath(".//w:drawing", namespaces=NS):
            i += 1
            continue
        caption = ""
        if i + 1 < len(children):
            caption = paragraph_text(children[i + 1]).strip()
        reference = caption_to_reference(caption)
        if not reference:
            i += 1
            continue
        prev_text = paragraph_text(children[i - 1]).strip() if i > 0 and children[i - 1].tag == qn("p") else ""
        fig_no = caption.split()[0] if caption else ""
        if fig_no and fig_no in prev_text and "所示" in prev_text:
            i += 1
            continue
        body.insert(body.index(child), make_reference_paragraph(reference))
        children = list(body)
        i += 2
    return etree.tostring(doc, xml_declaration=True, encoding="UTF-8", standalone="yes")


def replace_media_and_tables(figures: list[Path]):
    media_targets = [f"word/media/image{i}.png" for i in range(2, 16)]
    media_data = {target: fig.read_bytes() for target, fig in zip(media_targets, figures)}
    tmp = FIXED_DOCX.with_suffix(".tmp.docx")
    with ZipFile(SOURCE_DOCX, "r") as zin, ZipFile(tmp, "w", ZIP_DEFLATED) as zout:
        for item in zin.infolist():
            data = zin.read(item.filename)
            if item.filename in media_data:
                data = media_data[item.filename]
            elif item.filename == "word/document.xml":
                data = apply_three_line_tables(data)
                data = ensure_figure_references(data)
            zout.writestr(item, data)
    shutil.move(str(tmp), str(FIXED_DOCX))


def main():
    if not SOURCE_DOCX.exists():
        raise FileNotFoundError(SOURCE_DOCX)
    figures = generate_figures()
    replace_media_and_tables(figures)
    print(FIXED_DOCX)
    for p in figures:
        print(p)


if __name__ == "__main__":
    main()
