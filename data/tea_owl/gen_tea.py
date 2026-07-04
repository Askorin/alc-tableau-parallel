import sys, os
INIV = 1
def gedec(i, m):
    s = []
    for k in range(i, m + 1):
        s.append(f'<Declaration><Class IRI="#x{k}"/></Declaration>')
        s.append(f'<SubClassOf><Class IRI="#x{k}"/><Class IRI="#x0"/></SubClassOf>')
        s.append(f'<Declaration><ObjectProperty IRI="#r{k}"/></Declaration>')
        s.append(f'<SubObjectPropertyOf><ObjectProperty IRI="#r{k}"/><ObjectProperty IRI="#r0"/></SubObjectPropertyOf>')
    k = m + 1
    s.append(f'<Declaration><Class IRI="#x{k}"/></Declaration>')
    s.append(f'<SubClassOf><Class IRI="#x{k}"/><Class IRI="#x0"/></SubClassOf>')
    return ''.join(s)
def gegcisubi(i):
    return (f'<ObjectAllValuesFrom><ObjectProperty IRI="#r{i}"/>'
            f'<ObjectIntersectionOf><Class IRI="#x{i}"/><Class IRI="#x{i+1}"/>'
            f'</ObjectIntersectionOf></ObjectAllValuesFrom>')
def gegcisupi(i):
    return (f'<ObjectSomeValuesFrom><ObjectProperty IRI="#r{i}"/>'
            f'<ObjectUnionOf><Class IRI="#x{i}"/><Class IRI="#x{i+1}"/>'
            f'</ObjectUnionOf></ObjectSomeValuesFrom>')
def gegci(i, m):
    subs = [gegcisubi(k) for k in range(i, m + 1) if k % 2 == 1]
    sups = [gegcisupi(k) for k in range(i, m + 1) if k % 2 == 0]
    return ('<SubClassOf>'
            '<ObjectUnionOf>' + ''.join(subs) + '</ObjectUnionOf>'
            '<ObjectIntersectionOf>' + ''.join(sups) + '</ObjectIntersectionOf>'
            '</SubClassOf>')
def pin2(i, m):
    return gedec(i, m) + gegci(i, m)
d = sys.argv[1]
tdir = sys.argv[2]
with open(os.path.join(tdir, 'tea_for_testing_trace.template'), newline='') as f:
    template = f.read()
for c in (int(x) for x in sys.argv[3:]):
    out = template.replace('<!--ggccii-->', pin2(INIV, c))
    with open(os.path.join(d, f'tea_for_testing_trace_{c}.owl'), 'w', newline='\r\n') as f:
        f.write(out)
    print(f'wrote scale {c} ({len(out)} chars)')
