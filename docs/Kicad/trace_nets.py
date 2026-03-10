import re
import sys

pcb_file = 'Kicad/1000W Controller.kicad_pcb'

try:
    with open(pcb_file, 'r', encoding='utf-8') as f:
        content = f.read()
except FileNotFoundError:
    print(f"Error: {pcb_file} not found.")
    sys.exit(1)

footprints = content.split('(footprint ')

components = {}
for fp in footprints[1:]:
    ref_match = re.search(r'\(property "Reference" "(.*?)"', fp)
    val_match = re.search(r'\(property "Value" "(.*?)"', fp)
    if ref_match:
        ref = ref_match.group(1)
        val = val_match.group(1) if val_match else ''
        pads = re.findall(r'\(pad "(.*?)" .*?\(net \d+ "(.*?)"\)', fp, re.DOTALL)
        components[ref] = {
            'value': val,
            'pads': {pad: net for pad, net in pads}
        }

def find_connected(start_net, exclude_refs=None):
    if exclude_refs is None:
        exclude_refs = set()
    connected = []
    for ref, data in components.items():
        if ref in exclude_refs:
            continue
        for pad, net in data['pads'].items():
            if net == start_net:
                connected.append((ref, pad))
    return connected

# Trace A2 (Switch LED)
print("--- Tracing A2 (Switch LED) ---")
a2_net = components['Feather_M4_Express1']['pads'].get('7')
print(f"Feather A2 is on net {a2_net}")
connected_to_a2 = find_connected(a2_net, exclude_refs=['Feather_M4_Express1'])
print(f"Connected to A2: {connected_to_a2}")
for ref, pad in connected_to_a2:
    if ref.startswith('R'):
        # Find other side of resistor
        other_pad = '2' if pad == '1' else '1'
        other_net = components[ref]['pads'].get(other_pad)
        print(f"Other side of {ref} is net {other_net}")
        connected_to_other = find_connected(other_net, exclude_refs=[ref])
        print(f"Connected to other side: {connected_to_other}")

# Trace A4 (PSU Remote On/Off)
print("\n--- Tracing A4 (PSU Remote On/Off) ---")
a4_net = components['Feather_M4_Express1']['pads'].get('9')
print(f"Feather A4 is on net {a4_net}")
connected_to_a4 = find_connected(a4_net, exclude_refs=['Feather_M4_Express1'])
print(f"Connected to A4: {connected_to_a4}")

# Trace U1 (Rad Fans)
print("\n--- Tracing U1 (Rad Fan Isolator) ---")
for pad in ['6', '7']: # Output side of ADuM1201 (pin 6 is VoB, pin 7 is ViA)
    net = components['U1']['pads'].get(pad)
    if net:
        connected = find_connected(net, exclude_refs=['U1'])
        print(f"U1 pad {pad} (net {net}) connects to: {connected}")

# Trace U4 (PSU Fan)
print("\n--- Tracing U4 (PSU Fan Isolator) ---")
for pad in ['6', '7']:
    net = components['U4']['pads'].get(pad)
    if net:
        connected = find_connected(net, exclude_refs=['U4'])
        print(f"U4 pad {pad} (net {net}) connects to: {connected}")

# Trace U5 (Pump)
print("\n--- Tracing U5 (Pump Isolator) ---")
for pad in ['6', '7']:
    net = components['U5']['pads'].get(pad)
    if net:
        connected = find_connected(net, exclude_refs=['U5'])
        print(f"U5 pad {pad} (net {net}) connects to: {connected}")

# Trace U6 (Aux Fan)
print("\n--- Tracing U6 (Aux Fan Isolator) ---")
for pad in ['6', '7']:
    net = components['U6']['pads'].get(pad)
    if net:
        connected = find_connected(net, exclude_refs=['U6'])
        print(f"U6 pad {pad} (net {net}) connects to: {connected}")

