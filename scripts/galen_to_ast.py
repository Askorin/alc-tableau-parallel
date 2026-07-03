"""
Convierte full-galen.owl (RDF/XML) a un modulo ALC en formato JSON AST.
"""

import argparse
import json
import sys
import xml.etree.ElementTree as ET
from collections import deque

RDF = "{http://www.w3.org/1999/02/22-rdf-syntax-ns#}"
RDFS = "{http://www.w3.org/2000/01/rdf-schema#}"
OWL = "{http://www.w3.org/2002/07/owl#}"


def local(uri):
    return uri.split("#")[-1]


def parse_rdf(path):
    """Devuelve nodes: id -> lista de (prop, valor) donde valor es
    ('ref', id) o ('lit', texto). id = nombre local (named) o nodeID (blank)."""
    nodes = {}
    for _, elem in ET.iterparse(path, events=("end",)):
        if elem.tag != RDF + "Description":
            continue
        nid = elem.attrib.get(RDF + "about")
        if nid is not None:
            nid = local(nid)
        else:
            nid = elem.attrib.get(RDF + "nodeID")
        if nid is None:
            elem.clear()
            continue
        props = nodes.setdefault(nid, [])
        for child in elem:
            ref = child.attrib.get(RDF + "resource")
            blank = child.attrib.get(RDF + "nodeID")
            if ref is not None:
                props.append((child.tag, ("ref", local(ref))))
            elif blank is not None:
                props.append((child.tag, ("ref", blank)))
            elif child.text and child.text.strip():
                props.append((child.tag, ("lit", child.text.strip())))
        elem.clear()
    return nodes


def get(props, tag):
    for t, v in props:
        if t == tag:
            return v
    return None


def get_all(props, tag):
    return [v for t, v in props if t == tag]


class ExprBuilder:
    """Construye expresiones AST (dict) desde nodos RDF, con memoizacion."""

    def __init__(self, nodes):
        self.nodes = nodes
        self.memo = {}
        self.dropped = 0  # expresiones con constructos no soportados

    def rdf_list(self, nid):
        items = []
        while nid is not None and nid != "nil":
            props = self.nodes.get(nid, [])
            first = get(props, RDF + "first")
            rest = get(props, RDF + "rest")
            if first and first[0] == "ref":
                items.append(first[1])
            nid = rest[1] if rest else None
        return items

    def build(self, nid):
        """AST dict o None si el constructo no es soportado."""
        if nid in self.memo:
            return self.memo[nid]
        props = self.nodes.get(nid)
        if props is None:
            # referencia a algo sin descripcion: tratarlo como atomico nombrado
            ast = {"type": "ATOMIC", "name": nid}
            self.memo[nid] = ast
            return ast

        types = {v[1] for t, v in props if t == RDF + "type" and v[0] == "ref"}

        result = None
        if "Restriction" in types:
            role = get(props, OWL + "onProperty")
            some = get(props, OWL + "someValuesFrom")
            allv = get(props, OWL + "allValuesFrom")
            if role and some:
                inner = self.build(some[1])
                if inner:
                    result = {"type": "EXISTENTIAL", "role": role[1], "inner": inner}
            elif role and allv:
                inner = self.build(allv[1])
                if inner:
                    result = {"type": "UNIVERSAL", "role": role[1], "inner": inner}
            # otras restricciones (cardinalidad, hasValue): no soportadas
        else:
            inter = get(props, OWL + "intersectionOf")
            union = get(props, OWL + "unionOf")
            compl = get(props, OWL + "complementOf")
            if inter:
                parts = [self.build(x) for x in self.rdf_list(inter[1])]
                if parts and all(parts):
                    result = balanced_fold(parts, "CONJUNCTION")
            elif union:
                parts = [self.build(x) for x in self.rdf_list(union[1])]
                if parts and all(parts):
                    result = balanced_fold(parts, "DISJUNCTION")
            elif compl:
                inner = self.build(compl[1])
                if inner:
                    result = {"type": "NEGATION", "inner": inner}
            elif not nid.startswith("A") or "Class" in types:
                # nodo nombrado sin estructura -> atomico
                # (los blank nodes de jena se llaman A0, A1, ...)
                result = {"type": "ATOMIC", "name": nid}

        if result is None:
            self.dropped += 1
        self.memo[nid] = result
        return result


def balanced_fold(parts, op):
    """Conjuncion/disyuncion balanceada: profundidad log n, no n."""
    if len(parts) == 1:
        return parts[0]
    mid = len(parts) // 2
    return {
        "type": op,
        "left": balanced_fold(parts[:mid], op),
        "right": balanced_fold(parts[mid:], op),
    }


def signature(ast, out):
    t = ast["type"]
    if t == "ATOMIC":
        out.add(ast["name"])
    elif t == "NEGATION":
        signature(ast["inner"], out)
    elif t in ("EXISTENTIAL", "UNIVERSAL"):
        signature(ast["inner"], out)
    else:
        signature(ast["left"], out)
        signature(ast["right"], out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("output")
    ap.add_argument(
        "--size",
        type=int,
        default=100,
        help="numero max de conceptos nombrados del modulo",
    )
    ap.add_argument(
        "--seed", default="Heart", help="concepto semilla del BFS (default: Heart)"
    )
    ap.add_argument(
        "--primitive-only",
        action="store_true",
        help="tratar A=C como A<=C (descarta direccion inversa)",
    )
    args = ap.parse_args()

    print(f"Parseando {args.input} ...", file=sys.stderr)
    nodes = parse_rdf(args.input)
    builder = ExprBuilder(nodes)

    # ---- Recoleccion de axiomas de clase: lhs_name -> [(kind, rhs_ast)] ----
    axioms_of = {}
    n_sub, n_equiv, n_role_axioms_dropped, n_unsupported = 0, 0, 0, 0

    role_props = {OWL + "inverseOf"}
    role_types = {
        "TransitiveProperty",
        "FunctionalProperty",
        "ObjectProperty",
        "SymmetricProperty",
        "InverseFunctionalProperty",
    }

    for nid, props in nodes.items():
        types = {v[1] for t, v in props if t == RDF + "type" and v[0] == "ref"}
        if types & role_types:
            # axiomas de roles: fuera de ALC tal como los tratamos
            n_role_axioms_dropped += sum(
                1 for t, v in props if t in role_props or t == RDFS + "subPropertyOf"
            )
            continue
        if nid.startswith("A") and nid[1:].isdigit():
            continue  # blank node: es parte de una expresion, no un axioma

        for t, v in props:
            if t == RDFS + "subClassOf" and v[0] == "ref":
                rhs = builder.build(v[1])
                if rhs:
                    axioms_of.setdefault(nid, []).append(("sub", rhs))
                    n_sub += 1
                else:
                    n_unsupported += 1
            elif t == OWL + "equivalentClass" and v[0] == "ref":
                rhs = builder.build(v[1])
                if rhs:
                    axioms_of.setdefault(nid, []).append(("equiv", rhs))
                    n_equiv += 1
                else:
                    n_unsupported += 1

    all_named = sorted(axioms_of.keys())
    print(
        f"Clases con axiomas: {len(all_named)}  subClassOf: {n_sub}  "
        f"equivalentClass: {n_equiv}",
        file=sys.stderr,
    )
    print(
        f"DESCARTADO (debilitacion semantica): {n_role_axioms_dropped} axiomas "
        f"de roles, {n_unsupported} axiomas con constructos no soportados, "
        f"{builder.dropped} expresiones no soportadas",
        file=sys.stderr,
    )

    # ---- Extraccion de modulo: BFS determinista desde la semilla ----
    seed = args.seed if args.seed in axioms_of else all_named[0]
    if seed != args.seed:
        print(
            f"AVISO: semilla '{args.seed}' no encontrada, usando '{seed}'",
            file=sys.stderr,
        )

    selected = []  # orden de insercion, determinista
    selected_set = set()
    queue = deque([seed])
    while queue and len(selected) < args.size:
        name = queue.popleft()
        if name in selected_set:
            continue
        selected.append(name)
        selected_set.add(name)
        for kind, rhs in axioms_of.get(name, []):
            sig = set()
            signature(rhs, sig)
            for s in sorted(sig):
                if s not in selected_set:
                    queue.append(s)

    # ---- Filtrado: solo axiomas con firma contenida en el modulo ----
    conjuncts = []
    kept_sub, kept_equiv = 0, 0
    for name in selected:
        for kind, rhs in axioms_of.get(name, []):
            sig = set()
            signature(rhs, sig)
            if not sig <= selected_set:
                continue
            lhs = {"type": "ATOMIC", "name": name}
            # LHS <= RHS  ==>  NOT LHS OR RHS
            conjuncts.append(
                {
                    "type": "DISJUNCTION",
                    "left": {"type": "NEGATION", "inner": lhs},
                    "right": rhs,
                }
            )
            kept_sub += 1
            if kind == "equiv" and not args.primitive_only:
                # RHS <= LHS  ==>  NOT RHS OR LHS  (no absorbible!)
                conjuncts.append(
                    {
                        "type": "DISJUNCTION",
                        "left": {"type": "NEGATION", "inner": rhs},
                        "right": lhs,
                    }
                )
                kept_equiv += 1

    if not conjuncts:
        print("ERROR: modulo vacio", file=sys.stderr)
        sys.exit(1)

    root = balanced_fold(conjuncts, "CONJUNCTION")
    with open(args.output, "w") as f:
        json.dump(root, f)

    print(
        f"Modulo: {len(selected)} conceptos (semilla '{seed}'), "
        f"{kept_sub} axiomas <= (absorbibles), "
        f"{kept_equiv} direcciones inversas de = (residuales)"
        + (" [primitive-only]" if args.primitive_only else ""),
        file=sys.stderr,
    )
    print(f"Escrito: {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
