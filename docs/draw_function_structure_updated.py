from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


OUT = Path(__file__).resolve().parent / "系统功能结构图-新版.png"
W, H = 2600, 980


def get_font(size, bold=False):
    preferred = Path(r"C:\Windows\Fonts\simhei.ttf" if bold else r"C:\Windows\Fonts\simsun.ttc")
    candidates = [
        preferred,
        Path(r"C:\Windows\Fonts\msyh.ttc"),
        Path(r"C:\Windows\Fonts\simhei.ttf"),
    ]
    for item in candidates:
        if item.exists():
            return ImageFont.truetype(str(item), size)
    return ImageFont.load_default()


def center_text(draw, box, text, font):
    x1, y1, x2, y2 = box
    bbox = draw.textbbox((0, 0), text, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    draw.text((x1 + (x2 - x1 - tw) / 2, y1 + (y2 - y1 - th) / 2 - 2), text, font=font, fill=(0, 0, 0))


def vertical_text(draw, box, text, font):
    x1, y1, x2, y2 = box
    chars = list(text)
    line_h = 28
    total_h = line_h * len(chars)
    y = y1 + max(8, (y2 - y1 - total_h) / 2)
    for ch in chars:
        bbox = draw.textbbox((0, 0), ch, font=font)
        tw = bbox[2] - bbox[0]
        draw.text((x1 + (x2 - x1 - tw) / 2, y), ch, font=font, fill=(0, 0, 0))
        y += line_h


def rect(draw, box, width=2):
    draw.rectangle(box, outline=(0, 0, 0), width=width)


def main():
    image = Image.new("RGB", (W, H), "white")
    draw = ImageDraw.Draw(image)
    f_title = get_font(38)
    f_module = get_font(25)
    f_leaf = get_font(24)
    f_caption = get_font(28)

    modules = [
        ("登录权限", ["人员登录", "角色权限", "操作日志"]),
        ("患者管理", ["患者建档", "患者查询", "病历档案"]),
        ("挂号管理", ["科室医生", "日期时段", "号源控制"]),
        ("候诊叫号", ["候诊队列", "科室筛选", "医生叫号"]),
        ("医生管理", ["医生信息", "科室维护", "状态管理"]),
        ("医生排班", ["排班维护", "号源调整", "停诊管理"]),
        ("医生接诊", ["待诊查看", "病历填写", "检查候诊"]),
        ("检查检验", ["开检查单", "结果录入", "状态查询"]),
        ("处方管理", ["开立处方", "处方审核", "确认发药"]),
        ("药品库存", ["药品维护", "扫码入库", "库存预警"]),
        ("收费结算", ["账单查询", "缴费退费", "支付记录"]),
        ("费用统计", ["日期统计", "科室统计", "图表展示"]),
    ]

    root = (965, 40, 1635, 102)
    rect(draw, root)
    center_text(draw, root, "医院门诊挂号与药品管理系统", f_title)

    root_cx = (root[0] + root[2]) // 2
    trunk_y = 158
    module_y1, module_y2 = 190, 252
    leaf_y1, leaf_y2 = 322, 830
    leaf_w = 50
    leaf_gap = 13
    module_gap = 28

    blocks = []
    x = 45
    for name, leaves in modules:
        block_w = len(leaves) * leaf_w + (len(leaves) - 1) * leaf_gap
        module_w = max(138, block_w + 18)
        blocks.append((x, x + module_w, block_w, name, leaves))
        x += module_w + module_gap

    used_w = blocks[-1][1] - blocks[0][0]
    shift = (W - used_w) / 2 - blocks[0][0]
    blocks = [(int(a + shift), int(b + shift), bw, name, leaves) for a, b, bw, name, leaves in blocks]

    first_c = (blocks[0][0] + blocks[0][1]) // 2
    last_c = (blocks[-1][0] + blocks[-1][1]) // 2
    draw.line((root_cx, root[3], root_cx, trunk_y), fill=(0, 0, 0), width=2)
    draw.line((first_c, trunk_y, last_c, trunk_y), fill=(0, 0, 0), width=2)

    for x1, x2, block_w, name, leaves in blocks:
        cx = (x1 + x2) // 2
        draw.line((cx, trunk_y, cx, module_y1), fill=(0, 0, 0), width=2)
        mod_box = (cx - 68, module_y1, cx + 68, module_y2)
        rect(draw, mod_box)
        center_text(draw, mod_box, name, f_module)

        leaf_start = cx - block_w // 2
        leaf_centers = []
        for i in range(len(leaves)):
            lx1 = leaf_start + i * (leaf_w + leaf_gap)
            lx2 = lx1 + leaf_w
            leaf_centers.append((lx1 + lx2) // 2)

        branch_y = 294
        draw.line((cx, module_y2, cx, branch_y), fill=(0, 0, 0), width=2)
        draw.line((leaf_centers[0], branch_y, leaf_centers[-1], branch_y), fill=(0, 0, 0), width=2)
        for i, leaf in enumerate(leaves):
            lx1 = leaf_start + i * (leaf_w + leaf_gap)
            lx2 = lx1 + leaf_w
            lc = (lx1 + lx2) // 2
            draw.line((lc, branch_y, lc, leaf_y1), fill=(0, 0, 0), width=2)
            leaf_box = (lx1, leaf_y1, lx2, leaf_y2)
            rect(draw, leaf_box)
            vertical_text(draw, leaf_box, leaf, f_leaf)

    caption = "图1  系统功能结构图"
    bbox = draw.textbbox((0, 0), caption, font=f_caption)
    draw.text(((W - (bbox[2] - bbox[0])) / 2, 875), caption, font=f_caption, fill=(0, 0, 0))

    image.save(OUT)
    print(OUT)


if __name__ == "__main__":
    main()
