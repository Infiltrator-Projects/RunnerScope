#!/usr/bin/env python3
from pathlib import Path

path = Path("src/linux_main.c")
source = path.read_text()

old = '''        ".counter-hosted { color:@rm_operation; border-top:2px solid @rm_operation; }\\n"
        ".counter-queued { color:@rm_warning; border-top:2px solid @rm_warning; }\\n",
        (unsigned int)type->ui_bold_weight,
        (unsigned int)metrics->panel_radius,
'''
new = '''        ".counter-hosted { color:@rm_operation; border-top:2px solid @rm_operation; }\\n"
        ".counter-queued { color:@rm_warning; border-top:2px solid @rm_warning; }\\n",
        (unsigned int)metrics->panel_radius,
'''

count = source.count(old)
if count != 1:
    raise SystemExit(f"expected one transformed hero printf argument sequence, found {count}")
source = source.replace(old, new, 1)
path.write_text(source)
