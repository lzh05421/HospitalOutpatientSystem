from pathlib import Path
import importlib.util

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parent
BASE_SCRIPT = ROOT / "build_opening_report_7_modules.py"

spec = importlib.util.spec_from_file_location("opening_base", BASE_SCRIPT)
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)

base.OUT_REPORT = Path("E:/15751/Desktop/开题报告-完整模块新版.docx")
base.OUT_REPORT_COPY = ROOT / "开题报告-完整模块新版.docx"
base.OUT_STRUCTURE = ROOT / "系统功能结构图-完整模块新版.png"
base.TITLE = "基于 Qt 和 MySQL 的医院门诊挂号与药品管理系统的设计与实现"


def draw_structure_full(path):
    width, height = 3000, 1840
    image = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(image)
    root_font = base.font(43, True)
    module_font = base.font(25, True)
    leaf_font = base.font(20)
    caption_font = base.font(27)
    line = (48, 66, 94)
    border = (55, 76, 106)
    root_fill = (226, 238, 249)
    module_fill = (236, 245, 242)
    head_fill = (214, 232, 228)
    leaf_fill = (251, 253, 255)

    root = (760, 45, 2240, 126)
    draw.rounded_rectangle(root, radius=10, fill=root_fill, outline=border, width=3)
    base.draw_center(draw, root, "医院门诊挂号与药品管理系统", root_font, (15, 35, 58))

    modules = [
        ("患者入口与账号管理", ["患者注册登录", "就诊人切换", "个人中心"]),
        ("医院人员登录与权限", ["人员登录", "角色菜单", "数据权限"]),
        ("院长驾驶舱", ["今日挂号", "待缴账单", "收入概览"]),
        ("患者管理", ["患者建档", "信息维护", "关联查询"]),
        ("门诊预约挂号", ["智能分诊", "科室医生选择", "医保预校验"]),
        ("挂号管理", ["挂号查询", "急诊标记", "退号管理"]),
        ("候诊队列与叫号", ["候诊筛选", "叫号接诊", "状态流转"]),
        ("患者病历档案", ["历史挂号", "接诊病历", "处方检查"]),
        ("科室与医生管理", ["科室维护", "医生信息", "在职状态"]),
        ("医生排班与号源", ["排班维护", "号源规则", "批量排班"]),
        ("医生接诊", ["主诉病史", "诊断医嘱", "检查候诊"]),
        ("检查检验", ["检查申请", "项目维护", "结果录入"]),
        ("处方管理", ["处方开立", "审核驳回", "发药退药"]),
        ("药品库存", ["药品分类", "条码入库", "库存预警"]),
        ("收费结算与退费", ["账单生成", "自费医保支付", "退款审核"]),
        ("统计日志与系统管理", ["费用统计", "操作日志", "权限配置"]),
    ]

    cols = 4
    card_w, card_h = 660, 270
    gap_x, gap_y = 55, 105
    start_x, start_y = 120, 270
    trunk_y = 185
    row_trunks = [start_y + row * (card_h + gap_y) - 82 for row in range(4)]

    draw.line((width // 2, root[3], width // 2, row_trunks[-1]), fill=line, width=4)
    for row in range(4):
        y = row_trunks[row]
        draw.line((start_x + card_w // 2, y, start_x + 3 * (card_w + gap_x) + card_w // 2, y), fill=line, width=4)

    for idx, (name, leaves) in enumerate(modules):
        row, col = divmod(idx, cols)
        x = start_x + col * (card_w + gap_x)
        y = start_y + row * (card_h + gap_y)
        cx = x + card_w // 2
        branch_y = row_trunks[row]
        draw.line((cx, branch_y, cx, y), fill=line, width=4)
        card = (x, y, x + card_w, y + card_h)
        draw.rounded_rectangle(card, radius=8, fill=module_fill, outline=border, width=3)
        head = (x, y, x + card_w, y + 62)
        draw.rounded_rectangle(head, radius=8, fill=head_fill, outline=head_fill, width=0)
        base.draw_center(draw, head, name, module_font, (18, 64, 66))
        for j, item in enumerate(leaves):
            leaf = (x + 50, y + 92 + j * 48, x + card_w - 50, y + 130 + j * 48)
            draw.rounded_rectangle(leaf, radius=5, fill=leaf_fill, outline=(168, 183, 202), width=1)
            base.draw_center(draw, leaf, item, leaf_font, (31, 43, 60))

    base.draw_center(draw, (0, 1738, width, 1790), "图1 系统功能结构图", caption_font, (20, 32, 46))
    image.save(path)


base.MAIN_CONTENT = [
    ("主要内容：", "heading"),
    ("本系统面向医院门诊挂号与药品管理业务，采用客户端/服务端结构进行设计。客户端负责患者预约入口和医院人员业务界面，服务端负责统一接口、权限校验和业务处理，MySQL 数据库负责保存核心业务数据。按照当前项目实际页面、服务模块和业务流程，系统主要分为十六个功能模块，系统功能结构图如图1所示。", "body"),
    ("__PICTURE__", "picture"),
    ("（一）患者入口与账号管理模块", "subheading"),
    ("该模块面向普通就诊人员，提供患者注册登录、就诊人切换和个人中心等功能。患者登录后可以维护本人或家属就诊人信息，自动带入姓名、手机号和身份证号，减少预约挂号时重复录入。个人中心和历史订单用于查看患者自己的预约记录、待支付订单和已完成挂号记录。", "body"),
    ("（二）医院人员登录与权限管理模块", "subheading"),
    ("该模块面向医院内部工作人员，支持管理员、科主任、挂号员、医生、药房人员和收费员等角色登录。系统根据角色展示不同菜单，并由服务端对模块和动作进行权限校验，避免无关人员误操作患者、药品、账单和权限数据。权限配置用于维护人员账号、角色权限和菜单授权，为系统安全运行提供基础。", "body"),
    ("（三）院长驾驶舱模块", "subheading"),
    ("该模块用于汇总门诊运行情况，包括今日挂号、待缴账单、收入概览、库存预警和业务待办等信息。管理人员通过驾驶舱可以快速掌握当天门诊业务状态，发现收费、候诊和药品库存中的异常情况。", "body"),
    ("（四）患者管理模块", "subheading"),
    ("该模块用于维护患者基础档案，包括患者编号、姓名、性别、出生日期、身份证号、联系电话和家庭地址等信息。工作人员可进行新增、修改、删除、查询和分页展示，患者信息与挂号记录、病历档案、处方明细和收费账单建立关联。", "body"),
    ("（五）门诊预约挂号模块", "subheading"),
    ("该模块主要服务患者端预约流程，包括智能分诊、科室医生选择、号源查询、预约挂号、急诊挂号和医保资格预校验。患者可根据症状描述推荐科室，也可手动选择门诊大类、专科、诊室、医生、日期和时段。医保统筹挂号前进行资格校验，避免后续支付环节出现状态冲突。", "body"),
    ("（六）挂号管理模块", "subheading"),
    ("该模块面向挂号员和管理员，用于查看和维护挂号记录。系统展示挂号编号、患者、医生、科室、就诊日期、时段、费用和状态，支持按条件筛选、分页展示、退号处理和急诊标记。患者端新增预约后，医院端挂号列表可刷新查看。", "body"),
    ("（七）候诊队列与叫号模块", "subheading"),
    ("该模块用于连接挂号和医生接诊流程。系统根据挂号状态形成候诊队列，支持按科室、医生、日期和状态筛选；医生或挂号员可对患者进行叫号，叫号后患者进入接诊流程。急诊患者可通过急诊标识提高处理优先级。", "body"),
    ("（八）患者病历档案模块", "subheading"),
    ("该模块汇总患者历史挂号、接诊记录、检查结果、处方信息和收费记录，便于医生在接诊前了解患者既往就诊情况。病历档案与患者基础资料和挂号记录关联，能够支撑后续论文中的病历数据结构和就诊历史查询说明。", "body"),
    ("（九）科室与医生管理模块", "subheading"),
    ("该模块用于维护医院基础资料，包括科室目录、医生信息和医生状态。科室信息为挂号筛选和统计分析提供依据；医生信息包含所属科室、职称、专长、挂号费和在职状态，直接影响排班和患者端可预约医生列表。", "body"),
    ("（十）医生排班与号源管理模块", "subheading"),
    ("该模块用于维护医生出诊安排，包括医生、日期、时段、总号源、剩余号源和停诊状态。系统支持排班维护、批量排班和号源规则管理，挂号成功后扣减对应号源，保证患者端显示的可预约信息与医院端排班数据一致。", "body"),
    ("（十一）医生接诊模块", "subheading"),
    ("该模块面向医生工作站，医生可查看候诊患者，进行接诊、检查后候诊和完成就诊等操作。接诊过程中可填写主诉、现病史、既往史、体征、诊断、医嘱和外院报告摘要等结构化病历信息，为检查检验和处方开立提供依据。", "body"),
    ("（十二）检查检验模块", "subheading"),
    ("该模块用于管理检查项目和检查流程。医生可为患者开立检查申请，检查人员或医生可维护检查项目、录入检查结果、报告所见和结论。检查结果完成后可回到医生接诊流程，使诊疗过程更加完整。", "body"),
    ("（十三）处方管理模块", "subheading"),
    ("该模块用于处方开立、处方审核、驳回、发药和退药。医生根据诊断结果选择药品并填写数量、用法用量、频次和天数，系统计算处方金额；药房人员审核处方后进行发药，处方状态与药品库存、账单费用和操作日志联动。", "body"),
    ("（十四）药品库存模块", "subheading"),
    ("该模块用于维护药品编码、条形码、名称、分类、规格、单位、进价、售价、库存数量、预警数量和有效期等信息。系统支持药品分类维护、扫码或条码入库、库存记录追踪和库存预警；PASS 用药规则用于对过敏、剂量和联合用药等风险进行提醒或阻断。", "body"),
    ("（十五）收费结算与退费模块", "subheading"),
    ("该模块用于账单生成、自费支付、医保支付、二维码或模拟支付、支付状态查询和退款审核。系统根据挂号费、检查费和药品费生成账单，记录支付方式、金额、收费员和支付时间；退费业务通过申请和审核流程控制，保证收费数据可追溯。", "body"),
    ("（十六）统计日志与系统管理模块", "subheading"),
    ("该模块包括费用统计、操作日志、审计明细、权限配置和系统运行配置。费用统计按日期、科室和费用类型展示挂号收入、药品收入、检查收入和总收入；操作日志记录关键写操作；系统配置负责数据库连接、跨平台构建和运行脚本管理，保证项目可在 Windows、Linux、Qt Creator 和银河麒麟环境中部署运行。", "body"),
]

base.draw_structure = draw_structure_full


if __name__ == "__main__":
    base.build_report()
