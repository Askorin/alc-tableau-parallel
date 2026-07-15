import xml.etree.ElementTree as ET
import json
import os
import sys

NS = {'owl': 'http://www.w3.org/2002/07/owl#'}

def parse_concept(element):
    tag = element.tag.split('}')[-1] 
    
    if tag == 'Class':
        return {"type": "ATOMIC", "name": element.attrib.get('IRI', '').replace('#', '')}
        
    elif tag == 'ObjectComplementOf':
        return {"type": "NEGATION", "inner": parse_concept(element[0])}
        
    elif tag == 'ObjectIntersectionOf':
        left = parse_concept(element[0])
        for child in element[1:]:
            left = {"type": "CONJUNCTION", "left": left, "right": parse_concept(child)}
        return left
        
    elif tag == 'ObjectUnionOf':
        left = parse_concept(element[0])
        for child in element[1:]:
            left = {"type": "DISJUNCTION", "left": left, "right": parse_concept(child)}
        return left
        
    elif tag in ('ObjectSomeValuesFrom', 'ObjectAllValuesFrom'):
        role_iri = ""
        inner = None
        for child in element:
            child_tag = child.tag.split('}')[-1]
            if child_tag == 'ObjectProperty':
                role_iri = child.attrib.get('IRI', '').replace('#', '')
            else:
                inner = parse_concept(child)
        
        node_type = "EXISTENTIAL" if tag == 'ObjectSomeValuesFrom' else "UNIVERSAL"
        return {"type": node_type, "role": role_iri, "inner": inner}
        
    raise ValueError(f"Unsupported tag: {tag}")

def internalize_tbox(root):
    axioms = []
    
    for sub in root.findall('.//owl:SubClassOf', NS):
        if len(sub) == 2:
            C = parse_concept(sub[0])
            D = parse_concept(sub[1])
            axioms.append({
                "type": "DISJUNCTION",
                "left": {"type": "NEGATION", "inner": C},
                "right": D
            })
            
    for eq in root.findall('.//owl:EquivalentClasses', NS):
        if len(eq) == 2:
            C = parse_concept(eq[0])
            D = parse_concept(eq[1])
            
            sub1 = {"type": "DISJUNCTION", "left": {"type": "NEGATION", "inner": C}, "right": D}
            sub2 = {"type": "DISJUNCTION", "left": {"type": "NEGATION", "inner": D}, "right": C}
            
            axioms.append(sub1)
            axioms.append(sub2)

    if not axioms:
        return {}
        
    global_tbox = axioms[0]
    for axiom in axioms[1:]:
        global_tbox = {"type": "CONJUNCTION", "left": global_tbox, "right": axiom}
        
    return global_tbox

def count_existential_branches(node, polarity=1):
    if not node: return 0
    
    t = node.get("type")
    if t == "ATOMIC":
        return 0
    elif t == "NEGATION":
        return count_existential_branches(node["inner"], -polarity)
    elif t in ("CONJUNCTION", "DISJUNCTION"):
        return count_existential_branches(node["left"], polarity) + count_existential_branches(node["right"], polarity)
    elif t == "EXISTENTIAL":
        branches = 1 if polarity == 1 else 0
        return branches + count_existential_branches(node["inner"], polarity)
    elif t == "UNIVERSAL":
        branches = 1 if polarity == -1 else 0
        return branches + count_existential_branches(node["inner"], polarity)
    
    return 0

def extract_metadata(xml_file):
    try:
        tree = ET.parse(xml_file)
        root = tree.getroot()
    except ET.ParseError as e:
        print(f"Error parsing {xml_file}: {e}")
        return None

    # 1. Contar Conceptos Unicos (Atomic Classes)
    concepts = set()
    for cls in root.findall('.//owl:Class', NS):
        iri = cls.attrib.get('IRI')
        if iri:
            concepts.add(iri.replace('#', ''))
            
    # 2. Contar Axiomas
    subclass_axioms = len(root.findall('.//owl:SubClassOf', NS))
    equiv_axioms = len(root.findall('.//owl:EquivalentClasses', NS))
    total_axioms = subclass_axioms + equiv_axioms
    
    # 3. Contar Ramas Conjuntivas Esperadas
    tbox_ast = internalize_tbox(root)
    conjunctive_branches = count_existential_branches(tbox_ast, polarity=1) if tbox_ast else 0
    
    return {
        "file": os.path.basename(xml_file),
        "concepts": len(concepts),
        "subclass_axioms": subclass_axioms,
        "equivalent_axioms": equiv_axioms,
        "total_axioms": total_axioms,
        "expected_conjunctive_branches": conjunctive_branches
    }

if __name__ == "__main__":
    source_prefix = "../data/tea_owl/"
    scales = ["3", "5", "8", "13", "21", "34", "55", "89"]
    
    print(f"{'Scale':<7} | {'Concepts':<10} | {'SubClass':<10} | {'Equivalent':<12} | {'Total Axioms':<14} | {'Expected AND Branches':<22}")
    print("-" * 85)
    
    for i in scales:
        source_filename = f"tea_for_testing_trace_{i}.owl"
        filepath = os.path.join(source_prefix, source_filename)
        
        if os.path.exists(filepath):
            meta = extract_metadata(filepath)
            if meta:
                print(f"{i:<7} | {meta['concepts']:<10} | {meta['subclass_axioms']:<10} | {meta['equivalent_axioms']:<12} | {meta['total_axioms']:<14} | {meta['expected_conjunctive_branches']:<22}")
        else:
            print(f"{i:<7} | {'FILE NOT FOUND':<60}")
