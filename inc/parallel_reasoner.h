// ==================================
// parallel_reasoner.h
// ==================================

#pragma once
#include "arena_pool.h"
#include "concept.h"
#include "concept_manager.h"
#include "reasoner.h"
#include "tableau_node.h"
#include "thread_local_arena.h"
#include <algorithm>
#include <atomic>
#include <iostream>
#include <omp.h>
#include <vector>

class ParallelReasoner : public Reasoner {
  private:
    static constexpr int MAX_OMP_DEPTH = 1;
    static constexpr int CHUNK_SIZE = 1;
    ArenaPool pool;
    const Concept* tbox_;

    bool checkForClash(size_t node_idx, const ThreadLocalArena& arena) {
        for (const Concept* c : arena.getLabels(node_idx)) {
            if (c->type == ConceptType::NEGATION) {
                const auto* neg = static_cast<const NegationConcept*>(c);
                if (arena.hasLabel(node_idx, neg->inner)) {
                    // const_cast<ThreadLocalArena&>(arena)
                    //     .getNode(node_idx)
                    //     .isClashed = true;
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

    // FASE A: Solo resuelve ANDs, no se generan hijos aquí.
    bool
    saturatePropositional(size_t node_idx, ThreadLocalArena& arena,
                          const std::unordered_map<Concept*, Concept*>& defs) {
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
                } else if (c->type == ConceptType::ATOMIC) {
                    if (auto it = defs.find(c);
                        it != defs.end() &&
                        !arena.hasLabel(node_idx, it->second())) {

                        arena.addLabelToNode(node_idx, it->second());
                        changed = true;
                    }
                }
            }
            if (changed && checkForClash(node_idx, arena))
                return false;
        }
        return true;
    }

    // FASE B: Generamos el arbol estrictamente hacia abajo
    std::vector<size_t> generateFrontier(size_t node_idx,
                                         ThreadLocalArena& arena) {
        auto current_labels = arena.getLabels(node_idx);
        std::vector<const Concept*> snapshot(current_labels.begin(),
                                             current_labels.end());

        // Fase \exists
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

        // Fase \forall (Pushea a hijos recientemente creados)
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
                        // Seguro: Hijo está garantizado a haber sido creado
                        // localmente en fase previa in Phase 1
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
                // A disjunction is pending if NEITHER side is currently in the
                // node's labels
                if (!arena.hasLabel(node_idx, disj->left) &&
                    !arena.hasLabel(node_idx, disj->right)) {
                    return true;
                }
            }
        }
        return false;
    }

    bool resolveDisjunctions(size_t node_idx, ThreadLocalArena& arena,
                             int depth, std::vector<size_t>& ancestors,
                             const std::atomic<bool>* cancel) {
        // Snapshot de labels para evitar invalidación de iterador
        auto current_labels = arena.getLabels(node_idx);
        std::vector<const Concept*> labels_snapshot(current_labels.begin(),
                                                    current_labels.end());

        for (const Concept* c : labels_snapshot) {
            if (c->type == ConceptType::DISJUNCTION) {
                const auto* disj = static_cast<const DisjunctionConcept*>(c);

                if (!arena.hasLabel(node_idx, disj->left) &&
                    !arena.hasLabel(node_idx, disj->right)) {

                    // Adivinar izquierda
                    ScopedArena left_scoped(pool);
                    ThreadLocalArena& copyLeft = left_scoped.get();
                    copyLeft.bindPrefix(arena, arena.saveState(), node_idx);
                    copyLeft.addLabelToNode(node_idx, disj->left);

                    // Saturamos la izquierda
                    if (processNode(node_idx, copyLeft, depth, ancestors,
                                    cancel)) {
                        return true;
                    }

                    // Adivinar derecha (backtrack)
                    ScopedArena right_scoped(pool);
                    ThreadLocalArena& copyRight = right_scoped.get();
                    copyRight.bindPrefix(arena, arena.saveState(), node_idx);
                    copyRight.addLabelToNode(node_idx, disj->right);

                    // Saturamos la derecha
                    return processNode(node_idx, copyRight, depth, ancestors,
                                       cancel);
                }
            }
        }
        return false; // No debería alcanzarse si hasPendingDisjunctions era
                      // true
    }

    bool processNode(size_t node_idx, ThreadLocalArena& arena, int depth,
                     std::vector<size_t>& ancestors,
                     const std::unordered_map<Concept*, Concept*>& defs,
                     const std::atomic<bool>* cancel = nullptr) {

        // Un hermano en un frontier paralelo superior ya clasheó: nuestro
        // resultado es irrelevante, abortamos barato. (Sin esto, un task
        // profundo nunca ve el flag y explora subárboles que el orden serial
        // jamás tocaría)
        if (cancel && cancel->load(std::memory_order_relaxed))
            return false;

        // Atrapamos clashes pasados desde reglas universales/existenciales de
        // padres o clashes creados por disyunciones previas
        if (checkForClash(node_idx, arena))
            return false;

        if (isBlocked(node_idx, ancestors, arena))
            return true; // Blocked = SAT Valid Cycle

        // Saturación proposicional (ANDs)
        if (!saturatePropositional(node_idx, arena, defs))
            return false;

        // Disyunciones (ORs)
        if (hasPendingDisjunctions(node_idx, arena)) {
            return resolveDisjunctions(node_idx, arena, depth, ancestors,
                                       cancel);
        }

        // Generación estructural solo si es que el nodo está al 100%
        // proposiconalmente
        std::vector<size_t> frontier = generateFrontier(node_idx, arena);
        if (frontier.empty())
            return true; // Leaf node

        // Comenzamos paralelismo de ramas conjuntivas
        std::atomic<bool> global_clash{false};
        ancestors.push_back(node_idx);

        if (depth < MAX_OMP_DEPTH && frontier.size() >= 2) {
#pragma omp parallel for schedule(dynamic, CHUNK_SIZE) shared(global_clash)
            for (size_t i = 0; i < frontier.size(); ++i) {
                if (global_clash.load(std::memory_order_relaxed))
                    continue;

                ScopedArena task_scoped(pool);
                ThreadLocalArena& task_arena = task_scoped.get();
                task_arena.bindPrefix(arena, arena.saveState(), frontier[i]);

                std::vector<size_t> local_ancestors = ancestors;
                if (!processNode(frontier[i], task_arena, depth + 1,
                                 local_ancestors, defs, &global_clash)) {
                    global_clash.store(true, std::memory_order_relaxed);
                }
            }
        } else {
            for (size_t child_idx : frontier) {
                if (global_clash.load(std::memory_order_relaxed))
                    break;
                if (!processNode(child_idx, arena, depth + 1, ancestors, defs,
                                 cancel)) {
                    global_clash.store(true, std::memory_order_relaxed);
                }
            }
        }

        ancestors.pop_back();
        return !global_clash.load(std::memory_order_relaxed);
    }

  public:
    // El pool member se default-construye (lazy, sin arenas pre-alocadas)
    explicit ParallelReasoner() = default;

    bool isSatisfiable(const Concept* query,
                       const std::unordered_map<Concept*, Concept*>& defs,
                       const Concept* tbox = nullptr) override {
        tbox_ = tbox;
        ScopedArena master_scoped(pool);
        ThreadLocalArena& arena = master_scoped.get();

        size_t root_idx = arena.allocateNode();
        arena.addLabelToNode(root_idx, query);
        if (tbox_)
            arena.addLabelToNode(root_idx, tbox_);

        // El nodo raíz no tiene ancestros, lógicamente
        // processNode manejará pushing y popping a medida que el arbol crece
        std::vector<size_t> root_ancestors;

        // Invocamos el dispatcher maestro
        return processNode(root_idx, arena, 0, root_ancestors, defs);
    }
};
