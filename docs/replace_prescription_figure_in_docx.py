from __future__ import annotations

import shutil
import tempfile
import zipfile
from pathlib import Path
from xml.etree import ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
PNG = ROOT / "output" / "redrawn_course_figures" / "figure_4_4_prescription_flow.png"
CAPTION = "图4-4 处方审核与发药流程图"

NS_W = "http://schemas.openxmlformats.org/wordprocessingml/2006/main"
NS_A = "http://schemas.openxmlformats.org/drawingml/2006/main"
NS_R = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"
NS_REL = "http://schemas.openxmlformats.org/package/2006/relationships"


def q(ns: str, tag: str) -> str:
    return f"{{{ns}}}{tag}"


def paragraph_text(p: ET.Element) -> str:
    return "".join(
        node.text or ""
        for node in p.iter()
        if node.tag in (q(NS_W, "t"), q(NS_W, "delText"))
    )


def find_caption_image_target(docx_path: Path) -> str | None:
    with zipfile.ZipFile(docx_path, "r") as z:
        document = ET.fromstring(z.read("word/document.xml"))
        rels = ET.fromstring(z.read("word/_rels/document.xml.rels"))

    paragraphs = document.findall(".//" + q(NS_W, "p"))
    caption_index = None
    for i, p in enumerate(paragraphs):
        if paragraph_text(p).strip() == CAPTION:
            caption_index = i
            break
    if caption_index is None:
        return None

    embed_id = None
    for p in reversed(paragraphs[:caption_index]):
        for blip in p.findall(".//" + q(NS_A, "blip")):
            embed_id = blip.attrib.get(q(NS_R, "embed"))
            if embed_id:
                break
        if embed_id:
            break
    if not embed_id:
        return None

    for rel in rels.findall(q(NS_REL, "Relationship")):
        if rel.attrib.get("Id") == embed_id:
            target = rel.attrib.get("Target", "")
            return "word/" + target.lstrip("/")
    return None


def replace_media(docx_path: Path) -> bool:
    target = find_caption_image_target(docx_path)
    if not target:
        return False

    png_bytes = PNG.read_bytes()
    with tempfile.NamedTemporaryFile(delete=False, suffix=".docx") as tmp:
        tmp_path = Path(tmp.name)

    with zipfile.ZipFile(docx_path, "r") as zin, zipfile.ZipFile(tmp_path, "w", zipfile.ZIP_DEFLATED) as zout:
        for item in zin.infolist():
            data = png_bytes if item.filename == target else zin.read(item.filename)
            zout.writestr(item, data)

    shutil.move(str(tmp_path), str(docx_path))
    return True


if __name__ == "__main__":
    targets = [
        ROOT / "output" / "course_design_summary_generated.docx",
        ROOT / "output" / "course_summary_current.docx",
        ROOT / "output" / "course_summary_figures_tables_fixed.docx",
        ROOT / "output" / "desktop_final_verify.docx",
        ROOT / "output" / "desktop_fixed_verify.docx",
    ]
    for target in targets:
        if target.exists():
            print(target, "updated" if replace_media(target) else "not-found")
