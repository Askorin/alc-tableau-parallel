// ==================================
// serial_reasoner.h
// ==================================

#pragma once
#include "arena_pool.h"
#include "concept.h"
#include "concept_manager.h"
#include "reasoner.h"
#include "tableau_node.h"
#include "thread_local_arena.h"
#include <algorithm>
#include <iostream>
#include <vector>

class SerialReasoner : public Reasoner {
  private:
    ArenaPool pool;
    const Concept* tbox_;

    bool checkForClash(size_t node_idx, const ThreadLocalArena& arena) {
        for (const Concept* c : arena.getLabels(node_idx)) {
            if (c->type == ConceptType::NEGATION) {
                const auto* neg = static_cast<const NegationConcept*>(c);
                if (arena.hasLabel(node_idx, neg->inner)) {
                    //const_cast<ThreadLocalArena&>(arena)
                    //    .getNode(node_idx)
                    //    .isClashed = true;
                    return true;
                }
            }
        }
        return false;
    }

    bool isSubset(size_t sub_idx, size_t super_idx,
                  const ThreadLocalArena& arena) {
        for (const Concept* c : arena.getLabels(sub_idx)) {
            if (!arena.hasLabel(super_idx, c)) {
                return false;
            }
        }
        return true;
    }

    bool isBlocked(size_t node_idx, const std::vector<size_t>& ancestors,
                   const ThreadLocalArena& arena) {
        for (size_t ancestor_idx : ancestors) {
            if (isSubset(node_idx, ancestor_idx, arena)) {
                return true;
            }
        }
        return false;
    }

    size_t makeSuccesor(size_t parent_idx, ThreadLocalArena& arena) {
        size_t idx = arena.allocateNode(parent_idx);
        if (tbox_)
            arena.addLabelToNode(idx, tbox_);
        return idx;
    }

    bool saturatePropositional(size_t node_idx, ThreadLocalArena& arena) {
        bool changed = true;
        while (changed) {
            changed = false;
            auto current_labels = arena.getLabels(node_idx);
            std::vector<const Concept*> snapshot(current_labels.begin(),
                                                 current_labels.end());

            for (const Concept* c : snapshot) {
                if (c->type == ConceptType::CONJUNCTION) {
                    const auto* conj =
                        static_cast<const ConjunctionConcept*>(c);
                    if (!arena.hasLabel(node_idx, conj->left)) {
                        arena.addLabelToNode(node_idx, conj->left);
                        changed = true;
                    }
                    if (!arena.hasLabel(node_idx, conj->right)) {
                        arena.addLabelToNode(node_idx, conj->right);
                        changed = true;
                    }
                }
            }
            if (changed && checkForClash(node_idx, arena))
                return false;
        }
        return true;
    }

    std::vector<size_t> generateFrontier(size_t node_idx,
                                         ThreadLocalArena& arena) {
        auto current_labels = arena.getLabels(node_idx);
        std::vector<const Concept*> snapshot(current_labels.begin(),
                                             current_labels.end());

        for (const Concept* c : snapshot) {
            if (c->type == ConceptType::EXISTENTIAL) {
                const auto* ex = static_cast<const RoleRestrictionConcept*>(c);
                bool satisfied = false;
                for (const Edge& edge : arena.getEdges(node_idx)) {
                    if (edge.role == ex->role &&
                        arena.hasLabel(edge.child_idx, ex->inner)) {
                        satisfied = true;
                        break;
                    }
                }
                if (!satisfied) {
                    size_t new_idx = makeSuccesor(node_idx, arena);
                    arena.addLabelToNode(new_idx, ex->inner);
                    arena.addEdgeToNode(node_idx, ex->role, new_idx);
                }
            }
        }

        for (const Concept* c : snapshot) {
            if (c->type == ConceptType::UNIVERSAL) {
                const auto* univ =
                    static_cast<const RoleRestrictionConcept*>(c);

                auto current_edges = arena.getEdges(node_idx);
                std::vector<Edge> edge_snapshot(current_edges.begin(),
                                                current_edges.end());

                for (const Edge& edge : edge_snapshot) {
                    if (edge.role == univ->role &&
                        !arena.hasLabel(edge.child_idx, univ->inner)) {
                        arena.addLabelToNode(edge.child_idx, univ->inner);
                    }
                }
            }
        }

        std::vector<size_t> frontier;
        for (const Edge& edge : arena.getEdges(node_idx)) {
            frontier.push_back(edge.child_idx);
        }
        return frontier;
    }

    bool hasPendingDisjunctions(size_t node_idx,
                                const ThreadLocalArena& arena) {
        for (const Concept* c : arena.getLabels(node_idx)) {
            if (c->type == ConceptType::DISJUNCTION) {
                const auto* disj = static_cast<const DisjunctionConcept*>(c);
                if (!arena.hasLabel(node_idx, disj->left) &&
                    !arena.hasLabel(node_idx, disj->right)) {
                    return true;
                }
            }
        }
        return false;
    }

    bool resolveDisjunctions(size_t node_idx, ThreadLocalArena& arena,
                             std::vector<size_t>& ancestors) {
        auto current_labels = arena.getLabels(node_idx);
        std::vector<const Concept*> labels_snapshot(current_labels.begin(),
                                                    current_labels.end());

        for (const Concept* c : labels_snapshot) {
            if (c->type == ConceptType::DISJUNCTION) {
                const auto* disj = static_cast<const DisjunctionConcept*>(c);

                if (!arena.hasLabel(node_idx, disj->left) &&
                    !arena.hasLabel(node_idx, disj->right)) {

                    ScopedArena left_scoped(pool);
                    ThreadLocalArena& copyLeft = left_scoped.get();
                    copyLeft.bindPrefix(arena, arena.saveState(), node_idx);
                    copyLeft.addLabelToNode(node_idx, disj->left);

                    if (processNode(node_idx, copyLeft, ancestors)) {
                        return true;
                    }

                    ScopedArena right_scoped(pool);
                    ThreadLocalArena& copyRight = right_scoped.get();
                    copyRight.bindPrefix(arena, arena.saveState(), node_idx);
                    copyRight.addLabelToNode(node_idx, disj->right);

                    return processNode(node_idx, copyRight, ancestors);
                }
            }
        }
        return false;
    }

    bool processNode(size_t node_idx, ThreadLocalArena& arena,
                     std::vector<size_t>& ancestors) {
        ++stats.nodes;
        if (checkForClash(node_idx, arena))
            return false;
        if (isBlocked(node_idx, ancestors, arena))
            return true; // Blocked = SAT Valid Cycle

        if (!saturatePropositional(node_idx, arena))
            return false;

        if (hasPendingDisjunctions(node_idx, arena)) {
            return resolveDisjunctions(node_idx, arena, ancestors);
        }

        std::vector<size_t> frontier = generateFrontier(node_idx, arena);
        // mu: primer frontier no vacio del test = ramas conjuntivas
        // disponibles en el primer punto paralelizable 
        if (stats.mu == 0 && !frontier.empty())
            stats.mu = frontier.size();
        if (frontier.empty())
            return true; 

        ancestors.push_back(node_idx);

        for (size_t child_idx : frontier) {
            if (!processNode(child_idx, arena, ancestors)) {
                ancestors.pop_back();
                return false;
            }
        }

        ancestors.pop_back();
        return true;
    }

  public:
    // El pool member se default-construye (lazy, sin arenas pre-alocadas)
    explicit SerialReasoner() = default;

    bool isSatisfiable(const Concept* query,
                       const Concept* tbox = nullptr) override {
        tbox_ = tbox;
        stats = {};
        ScopedArena master_scoped(pool);
        ThreadLocalArena& arena = master_scoped.get();
        size_t root_idx = arena.allocateNode();
        arena.addLabelToNode(root_idx, query);
        if (tbox_)
            arena.addLabelToNode(root_idx, tbox_);

        std::vector<size_t> root_ancestors;
        return processNode(root_idx, arena, root_ancestors);
    }
};
