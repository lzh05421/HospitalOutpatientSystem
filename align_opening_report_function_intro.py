import copy
import shutil
import sys
import zipfile
from pathlib import Path
from xml.etree import ElementTree as ET

sys.stdout.reconfigure(encoding="utf-8")

DOCX = Path(r"E:\15751\Desktop\刘子航-开题报告\开题报告.docx")
BACKUP = DOCX.with_name("开题报告-修改前备份.docx")

NS_W = "http://schemas.openxmlformats.org/wordprocessingml/2006/main"
ET.register_namespace("w", NS_W)

for prefix, uri in [
    ("r", "http://schemas.openxmlformats.org/officeDocument/2006/relationships"),
    ("wp", "http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing"),
    ("a", "http://schemas.openxmlformats.org/drawingml/2006/main"),
    ("pic", "http://schemas.openxmlformats.org/drawingml/2006/picture"),
    ("wps", "http://schemas.microsoft.com/office/word/2010/wordprocessingShape"),
    ("w14", "http://schemas.microsoft.com/office/word/2010/wordml"),
    ("w15", "http://schemas.microsoft.com/office/word/2012/wordml"),
    ("w16cid", "http://schemas.microsoft.com/office/word/2016/wordml/cid"),
    ("mc", "http://schemas.openxmlformats.org/markup-compatibility/2006"),
]:
    ET.register_namespace(prefix, uri)


def w(tag):
    return f"{{{NS_W}}}{tag}"


def p_text(p):
    return "".join(
        node.text or ""
        for node in p.iter()
        if node.tag in (w("t"), w("delText"))
    )


def set_p_text(p, text):
    p_pr = p.find(w("pPr"))
    r_pr = None
    first_r = p.find(w("r"))
    if first_r is not None:
        first_r_pr = first_r.find(w("rPr"))
        if first_r_pr is not None:
            r_pr = copy.deepcopy(first_r_pr)

    for child in list(p):
        if child is not p_pr:
            p.remove(child)

    run = ET.Element(w("r"))
    if r_pr is not None:
        run.append(r_pr)
    t = ET.Element(w("t"))
    t.text = text
    run.append(t)
    p.append(run)


def clone_with_text(template, text):
    p = copy.deepcopy(template)
    set_p_text(p, text)
    return p


replacements = [
    (
        "（一）登录权限模块",
        "该模块与图1中的“登录权限”对应，包含医院人员登录、角色权限控制和操作日志管理三个子功能。医院工作人员通过账号密码登录系统，系统根据管理员、科主任、挂号员、医生、药房人员和收费员等角色控制可访问菜单与业务操作；同时对登录、查询、新增、修改、审核、发药和收费等关键动作进行日志记录，便于后续权限追溯和安全审计。",
    ),
    (
        "（二）患者管理模块",
        "该模块与图1中的“患者管理”对应，包含患者注册、患者维护和病历档案三个子功能。系统支持患者基础信息登记，维护姓名、性别、出生日期、身份证号、联系电话和家庭地址等资料；患者档案与挂号记录、接诊病历、检查结果、处方明细和收费账单建立关联，为后续就诊查询和病历追踪提供基础数据。",
    ),
    (
        "（三）挂号管理模块",
        "该模块与图1中的“挂号管理”对应，包含科室医生、日期时段和号源控制三个子功能。患者或挂号人员可按照科室、医生、日期和时段选择可用号源，系统根据医生排班中的剩余号源进行校验；挂号成功后同步扣减号源数量，避免同一时段重复占号或超额挂号。",
    ),
    (
        "（四）候诊叫号模块",
        "该模块与图1中的“候诊叫号”对应，包含候诊队列、科室候诊和医生叫号三个子功能。系统根据已挂号记录形成候诊队列，并可按科室、医生和日期展示待诊患者；医生或挂号人员完成叫号后，患者状态进入接诊流程，使挂号、候诊和医生接诊之间保持连续。",
    ),
    (
        "（五）医生管理模块",
        "该模块与图1中的“医生管理”对应，包含医师注册、专科信息和状态审核三个子功能。系统维护医生账号、所属科室、职称、专长、联系方式和在职状态等资料；通过状态审核控制医生是否参与排班、挂号和接诊，保证患者端显示的医生信息准确有效。",
    ),
    (
        "（六）医生排班模块",
        "该模块与图1中的“医生排班”对应，包含排班安排、号源同步和停诊管理三个子功能。管理人员为医生设置出诊日期、时间段和可挂号数量，排班数据同步形成患者端可预约号源；当医生停诊或调整出诊时，系统及时更新排班状态，保证实际出诊安排与挂号号源一致。",
    ),
    (
        "（七）医生接诊模块",
        "该模块与图1中的“医生接诊”对应，包含待诊查询、病历填写和检查医嘱三个子功能。医生可查询本人待诊患者并进入接诊页面，填写主诉、现病史、既往史、体征、诊断和医嘱等病历内容；需要进一步检查时，可在接诊过程中开立检查医嘱，为检查检验和处方处理提供依据。",
    ),
    (
        "（八）检查检验模块",
        "该模块与图1中的“检查检验”对应，包含申请医嘱、结果录入和状态查询三个子功能。医生可根据诊疗需要提交检查检验申请，检查完成后录入结果、报告所见和结论；系统支持检查状态查询，使检查申请、执行、结果回写和医生复诊判断形成完整闭环。",
    ),
    (
        "（九）处方管理模块",
        "该模块与图1中的“处方管理”对应，包含开立处方、处方审核和确认发药三个子功能。医生根据诊断结果选择药品并填写数量、用法用量、频次和天数，生成处方主表与明细；药房人员审核处方后确认发药，系统联动药品库存、处方状态和收费记录，保证处方处理过程可追踪。",
    ),
    (
        "（十）药品库存模块",
        "该模块与图1中的“药品库存”对应，包含药品维护、扫码入库和库存预警三个子功能。系统维护药品编码、条形码、名称、分类、规格、单位、价格、库存数量和有效期等信息；药品入库时可通过扫码或条码录入提高效率，当库存低于预警数量时系统进行提示，便于药房及时补充药品。",
    ),
    (
        "（十一）收费结算模块",
        "该模块与图1中的“收费结算”对应，包含账单查询、缴费结算和支付记录三个子功能。收费人员可查询挂号费、检查检验费和药品费等账单明细，完成自费或模拟医保缴费结算；系统记录支付方式、支付金额、经办人员和支付时间，使收费过程能够查询和追溯。",
    ),
    (
        "（十二）费用统计模块",
        "该模块与图1中的“费用统计”对应，包含日期统计、科室统计和图表展示三个子功能。系统按日期、科室和费用类型统计挂号收入、检查检验收入、药品收入及总收入，并通过图表展示门诊费用变化情况，为管理人员掌握业务运行和收入构成提供参考。",
    ),
]


if not BACKUP.exists():
    shutil.copy2(DOCX, BACKUP)

with zipfile.ZipFile(DOCX, "r") as zin:
    files = {name: zin.read(name) for name in zin.namelist()}

root = ET.fromstring(files["word/document.xml"])
paragraphs = root.findall(".//" + w("p"))
nonempty = [(p, p_text(p)) for p in paragraphs if p_text(p).strip()]

set_p_text(
    nonempty[23][0],
    "本系统面向医院门诊挂号与药品管理业务，采用客户端/服务端结构进行设计。客户端负责医院工作人员业务界面，服务端负责统一接口、权限校验和业务处理，MySQL数据库负责保存核心业务数据。按照图1所示功能结构，系统主要分为十二个模块、三十六个子功能，系统功能结构图如图1所示。",
)

start_p = nonempty[25][0]
end_p = nonempty[56][0]
parent = None
for candidate in root.iter():
    children = list(candidate)
    if start_p in children:
        parent = candidate
        break
if parent is None:
    raise RuntimeError("未找到图1后的功能介绍段落")

children = list(parent)
start_idx = children.index(start_p)
end_idx = children.index(end_p)
heading_template = nonempty[25][0]
body_template = nonempty[26][0]

new_paras = []
for heading, body in replacements:
    new_paras.append(clone_with_text(heading_template, heading))
    new_paras.append(clone_with_text(body_template, body))

for old in children[start_idx : end_idx + 1]:
    parent.remove(old)
for offset, paragraph in enumerate(new_paras):
    parent.insert(start_idx + offset, paragraph)

files["word/document.xml"] = ET.tostring(root, encoding="utf-8", xml_declaration=True)

with zipfile.ZipFile(DOCX, "w", zipfile.ZIP_DEFLATED) as zout:
    for name, data in files.items():
        zout.writestr(name, data)

print(f"updated={DOCX}")
print(f"backup={BACKUP}")
print(f"modules={len(replacements)} subfunctions={len(replacements) * 3}")
