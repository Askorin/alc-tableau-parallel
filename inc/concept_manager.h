// ==================================
// Concept.h
// ==================================
#pragma once
#include "concept.h"
#include <memory>
#include <unordered_set>
#include <functional>
#include <string>

struct ConceptHash {
    // Combinador de hashes, personalmente no lo entiendo mucho y lo saqué de internet! TODO: Entender esto
    static inline void hash_combine(std::size_t& seed, std::size_t hash) {
        seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    std::size_t operator()(const std::unique_ptr<Concept>& c) const {
        std::size_t seed = std::hash<int>()(static_cast<int>(c->type));

        switch (c->type) {
            case ConceptType::ATOMIC: {
                const auto* atomic = static_cast<const AtomicConcept*>(c.get());
                hash_combine(seed, std::hash<std::string>()(atomic->name));
                break;
            }
            case ConceptType::NEGATION: {
                const auto* neg = static_cast<const NegationConcept*>(c.get());
                hash_combine(seed, std::hash<const Concept*>()(neg->inner));
                break;
            }
            case ConceptType::CONJUNCTION: {
                const auto* conj = static_cast<const ConjunctionConcept*>(c.get());
                std::size_t hl = std::hash<const Concept*>()(conj->left);
                std::size_t hr = std::hash<const Concept*>()(conj->right);
                // Usar suma permite conmutavidiad (A AND B hashea idéntico a B AND A)
                hash_combine(seed, hl + hr);
                break;
            }
            case ConceptType::DISJUNCTION: {
                const auto* disj = static_cast<const DisjunctionConcept*>(c.get());
                std::size_t hl = std::hash<const Concept*>()(disj->left);
                std::size_t hr = std::hash<const Concept*>()(disj->right);
                hash_combine(seed, hl + hr);
                break;
            }
            case ConceptType::EXISTENTIAL:
            case ConceptType::UNIVERSAL: {
                const auto* restrict = static_cast<const RoleRestrictionConcept*>(c.get());
                hash_combine(seed, std::hash<std::string>()(restrict->role));
                hash_combine(seed, std::hash<const Concept*>()(restrict->inner));
                break;
            }
        }
        return seed;
    }
};

struct ConceptEqual {
    bool operator()(const std::unique_ptr<Concept>& a, const std::unique_ptr<Concept>& b) const {
        return a->isEqual(b.get());
    }
};

class ConceptManager {
private:
    std::unordered_set<std::unique_ptr<Concept>, ConceptHash, ConceptEqual> internPool;

    const Concept* intern(std::unique_ptr<Concept> c) {
        auto it = internPool.find(c);
        if (it != internPool.end()) {
            return it->get();
        }
        const Concept* ptr = c.get();
        internPool.insert(std::move(c));
        return ptr;
    }

public:
    const Concept* getAtomic(const std::string&name) {
        return intern(std::make_unique<AtomicConcept>(name));
    };

    const Concept* getNegation(const Concept* inner) {
        return intern(std::make_unique<NegationConcept>(inner));
    };

    const Concept* getConjunction(const Concept* left, const Concept* right) {
        return intern(std::make_unique<ConjunctionConcept>(left, right));
    };

    const Concept* getDisjunction(const Concept* left, const Concept* right) {
        return intern(std::make_unique<DisjunctionConcept>(left, right));
    };

    const Concept* getExistential(const std::string& role, const Concept* inner) {
        return intern(std::make_unique<RoleRestrictionConcept>(ConceptType::EXISTENTIAL, role, inner));
    };

    const Concept* getUniversal(const std::string& role, const Concept* inner) {
        return intern(std::make_unique<RoleRestrictionConcept>(ConceptType::UNIVERSAL, role, inner));
    };

    std::vector<const Concept*> getAllAtomicConcepts() const {
        std::vector<const Concept*> atomics;
        for (const auto& conceptPtr : internPool) {
            if (conceptPtr->type == ConceptType::ATOMIC) {
                atomics.push_back(conceptPtr.get());
            }
        }
        return atomics;
    }
};


