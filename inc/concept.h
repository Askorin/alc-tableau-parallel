#pragma once
#include <string>
#include <vector>

// ALC solo considera estos tipos.
enum class ConceptType {
    ATOMIC,
    NEGATION,
    CONJUNCTION,
    DISJUNCTION,
    EXISTENTIAL, // \exists R.C
    UNIVERSAL    // \forall R.C
};

/* Clase base para el AST */
struct Concept {
    ConceptType type;
    explicit Concept(ConceptType t) : type(t) {}
    virtual ~Concept() = default;

    virtual bool isEqual(const Concept* other) const = 0;
};

struct AtomicConcept : public Concept {
    std::string name;
    explicit AtomicConcept(const std::string& n) : Concept(ConceptType::ATOMIC), name(n) {}
    bool isEqual(const Concept* other) const override {
        if (other->type != ConceptType::ATOMIC) return false;
        return name == static_cast<const AtomicConcept*>(other)->name;
    }
};

struct NegationConcept : public Concept {
    const Concept* inner;
    explicit NegationConcept(const Concept* i) : Concept(ConceptType::NEGATION), inner(i) {}
    bool isEqual(const Concept* other) const override {
        if (other->type != ConceptType::NEGATION) return false;
        return inner == static_cast<const NegationConcept*>(other)->inner;
    }
};


struct ConjunctionConcept : public Concept {
    const Concept* left;
    const Concept* right;
    ConjunctionConcept(const Concept* l, const Concept* r) : Concept(ConceptType::CONJUNCTION), left(l), right(r) {}
    bool isEqual(const Concept* other) const override {
        if (other->type != ConceptType::CONJUNCTION) return false;
        auto* o = static_cast<const ConjunctionConcept*>(other);
        return (left == o->left & right == o->right) || (left == o->right && right == o->left); // Conmutatividad
    }
};


struct DisjunctionConcept : public Concept {
    const Concept* left;
    const Concept* right;
    DisjunctionConcept(const Concept* l, const Concept* r) : Concept(ConceptType::DISJUNCTION), left(l), right(r) {}
    bool isEqual(const Concept* other) const override {
        if (other->type != ConceptType::DISJUNCTION) return false;
        auto* o = static_cast<const ConjunctionConcept*>(other);
        return (left == o->left & right == o->right) || (left == o->right && right == o->left); // Conmutatividad
    }
};

struct RoleRestrictionConcept : public Concept {
    std::string role;
    const Concept* inner;
    RoleRestrictionConcept(ConceptType t, const std::string& r, const Concept* i) : Concept(t), role(r), inner(i) {}
    bool isEqual(const Concept* other) const override {
        if (other->type != type) return false;
        auto* o = static_cast<const RoleRestrictionConcept*>(other);
        return role == o->role && inner == o->inner;
    }
};


