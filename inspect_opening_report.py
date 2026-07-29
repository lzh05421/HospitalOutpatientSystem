import sys
import zipfile
from pathlib import Path
from xml.etree import ElementTree as ET

sys.stdout.reconfigure(encoding="utf-8")

DOCX = Path(r"E:\15751\Desktop\刘子航-开题报告\开题报告.docx")
NS = "http://schemas.openxmlformats.org/wordprocessingml/2006/main"


def w(tag):
    return f"{{{NS}}}{tag}"


with zipfile.ZipFile(DOCX) as z:
    root = ET.fromstring(z.read("word/document.xml"))

paras = []
for p in root.findall(".//" + w("p")):
    text = "".join(
        node.text or ""
        for node in p.iter()
        if node.tag in (w("t"), w("delText"))
    )
    if text.strip():
        paras.append(text)

for i, text in enumerate(paras):
    if 20 <= i <= 60:
        print(f"{i}: {text}")
