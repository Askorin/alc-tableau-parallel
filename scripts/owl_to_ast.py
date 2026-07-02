import xml.etree.ElementTree as ET
import json
import sys

NS = {'owl': 'http://www.w3.org/2002/07/owl#'}

def parse_concept(element):
    """Recursively parses OWL XML elements into AST dictionaries."""
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
        # Robust namespace-agnostic child iteration
        role_iri = ""
        inner = None
        for child in element:
            child_tag = child.tag.split('}')[-1]
            if child_tag == 'ObjectProperty':
                role_iri = child.attrib.get('IRI', '').replace('#', '')
            else:
                # The filler concept (Class, ObjectUnionOf, etc.)
                inner = parse_concept(child)
        
        node_type = "EXISTENTIAL" if tag == 'ObjectSomeValuesFrom' else "UNIVERSAL"
        return {"type": node_type, "role": role_iri, "inner": inner}
        
    raise ValueError(f"Unsupported tag: {tag}")

def internalize_tbox(xml_file):
    """Parses TBox axioms and reduces them to a single global concept."""
    tree = ET.parse(xml_file)
    root = tree.getroot()
    
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

if __name__ == "__main__":
    source_prefix = "../data/tea_owl/"
    for i in ["3", "5", "8", "13", "21", "34", "55", "89"]:
        source_filename = f"tea_for_testing_trace_{i}"
        ast = internalize_tbox(source_prefix + source_filename + ".owl")

        target_prefix = "../data/tea_ast/"
        with open(target_prefix + source_filename + ".json", "w", encoding="utf-8") as f:
            json.dump(ast, f, indent=4)
