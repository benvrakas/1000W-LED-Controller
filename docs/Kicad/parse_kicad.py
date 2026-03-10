import re

pcb_file = 'Kicad/1000W Controller.kicad_pcb'

with open(pcb_file, 'r', encoding='utf-8') as f:
    content = f.read()

footprints = content.split('(footprint ')

for fp in footprints[1:]:
    ref_match = re.search(r'\(fp_text reference "(.*?)"', fp)
    if ref_match:
        ref = ref_match.group(1)
        if ref in ['R3', 'Therm_sw1', 'Switch1', 'U1', 'U4', 'U5', 'U6']:
            print(f"\n--- {ref} ---")
            pads = re.findall(r'\(pad "(.*?)" .*?\(net \d+ "(.*?)"\)', fp, re.DOTALL)
            for pad, net in sorted(pads):
                print(f"Pad {pad}: {net}")
