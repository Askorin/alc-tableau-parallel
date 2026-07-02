#pragma once
#include "concept.h"
#include "concept_manager.h"
#include <unordered_map>
#include <utility>

struct NNFHash {
    std::size_t operator()(const std::pair<const Concept*, bool>& p) const {
        return std::hash<const Concept*>()(p.first) ^ (std::hash<bool>()(p.second) << 1);
    }
};

class NNFTransformer {
private:
    ConceptManager& targetManager;
    std::unordered_map<std::pair<const Concept*, bool>, const Concept*, NNFHash> cache;
    const Concept* transform(const Concept* node, bool isNegated) {
        auto key = std::make_pair(node, isNegated);
        if (cache.find(key) != cache.end()) return cache[key];

        const Concept* result = nullptr;

        if (node->type == ConceptType::ATOMIC) {
            auto* atomic = static_cast<const AtomicConcept*>(node);
            if (isNegated) {
                result = targetManager.getNegation(targetManager.getAtomic(atomic->name));
            } else {
                result = targetManager.getAtomic(atomic->name);
            }
        }
        else if (node->type == ConceptType::NEGATION) {
            auto* neg = static_cast<const NegationConcept*>(node);
            result = transform(neg->inner, !isNegated);
        }
        else if (node->type == ConceptType::CONJUNCTION) {
            auto* conj = static_cast<const ConjunctionConcept*>(node);
            if (isNegated) {
                result = targetManager.getDisjunction(transform(conj->left, true), transform(conj->right, true));
            } else {
                result = targetManager.getConjunction(transform(conj->left, false), transform(conj->right, false));
            }
        } else if (node->type == ConceptType::DISJUNCTION) {
            auto* disj = static_cast<const DisjunctionConcept*>(node);
            if (isNegated) {
                result = targetManager.getConjunction(transform(disj->left, true), transform(disj->right, true));
            } else {
                result = targetManager.getDisjunction(transform(disj->left, false), transform(disj->right, false));
            }
        } else if (node->type == ConceptType::EXISTENTIAL) {
            auto* ex = static_cast<const RoleRestrictionConcept*>(node);
            if (isNegated) {
                result = targetManager.getUniversal(ex->role, transform(ex->inner, true));
            } else {
                result = targetManager.getExistential(ex->role, transform(ex->inner, false));
            }
        } else if (node->type == ConceptType::UNIVERSAL) {
            auto* univ = static_cast<const RoleRestrictionConcept*>(node);
            if (isNegated) {
                result = targetManager.getExistential(univ->role, transform(univ->inner, true));
            } else {
                result = targetManager.getUniversal(univ->role, transform(univ->inner, false));
            }
        }

        cache[key] = result;
        return result;
    }

public:
    explicit NNFTransformer(ConceptManager& target) : targetManager(target) {}
    
    const Concept* convertToNNF(const Concept* sourceRoot) {
        return transform(sourceRoot, false);
    }
};


