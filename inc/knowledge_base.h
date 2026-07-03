#pragma once
#include "ast_builder.h"
#include "concept.h"
#include "concept_manager.h"
#include "nnf.h"
#include "reasoner.h"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

class KnowledgeBase {
  private:
    ConceptManager manager;
    std::vector<const Concept*> namedConcepts;
    const Concept* globalTBox; // TBox internalizada
    const Concept* residualTBox; // TBox internalizada sin axiomas absorbidos
    std::unordered_map<Concept*, Concept*> defs;

  public:
    // Inicialización y preprocesamiento a NNF
    void loadAndPreprocess(const std::string& filepath) {
        ConceptManager sourceManager;
        const Concept* rawRoot = parseOntology(sourceManager, filepath);

        NNFTransformer transformer(manager);
        globalTBox = transformer.convertToNNF(rawRoot);

        // Todos los conceptos atómicos para el loop de clasificación
        namedConcepts = manager.getAllAtomicConcepts();
        
        // Aplanamos el arbol de conjunciones
        std::vector<const Concept*> flattenedAxioms;
        flatten(globalTBox, flattenedAxioms);

        auto absorb = [&](const Concept* trigger, const Concept* def) {
            auto it = defs.find(trigger);
            defs[trigger] = (it == defs.end()) ? def : manager.getConjunction(it->second, def);
        };
        
        std::vector<const Concept*> residualAxioms;
        // Absorbemos los axiomas de inclusión
        for (const Concept* axiom : flattenedAxioms) {
            const Concept* trigger = nullptr;
            const Concept* def = nullptr;

            if (axiom->type == ConceptType::DISJUNCTION) {
                auto* disj = static_cast<const DisjunctionConcept*>(axiom);
                auto* negAtomic = [](const Concept* c) -> const Concept* {
                    if (c->type != ConceptType::NEGATION) return nullptr;
                    auto* n = static_cast<const NegationConcept*>(c);
                    return (n->inner->type == ConceptType::ATOMIC) ? n->inner : nullptr;
                };

                if ((trigger = negAtomic(disj->left))) {
                    def = disj->right;
                } else if ((trigger = negAtomic(disj->right))) {
                    def = disj->left;
                }
            }
            if (trigger) {
                absorb(trigger, def);
            } else {
                residualAxioms.push_back(axiom);
            }
        }

        // Rearmamos lo que quedó
        residualTBox = nullptr;
        if (residualAxioms.empty()) {
            return;
        }
        for (const Concept* axiom : residualAxioms) {
            if (residualTBox == nullptr) {
                residualTBox = axiom;
            } else {
                residualTBox = manager.getConjunction(residualTBox, axiom);
            }
        }
    }

    void flatten(const Concept* c, std::vector<const Concept*>& out) {
        if (c->type == ConceptType::CONJUNCTION) {
            auto* conj = static_cast<const ConjunctionConcept*>(c);
            flatten(conj->left, out);
            flatten(conj->right, out);
        } else {
            out.push_back(c);
        }
    }

    // Chequeo de consistencia
    bool isConsistent(std::unique_ptr<Reasoner>& reasoner) {
        // En un benchmark sin ABox (nuestro caso), consistencia es simplemente
        // chequear si la TBox global
        return reasoner->isSatisfiable(residualTBox, defs);
    }

    // Clasificación
    void classify(const bool log, std::unique_ptr<Reasoner>& reasoner) {
        int stride = 100;
        if (log) {
            std::cout << "Comenzando clasificación con " << namedConcepts.size()
                      << "X" << namedConcepts.size() << " = "
                      << namedConcepts.size() * namedConcepts.size()
                      << " subsunciones." << "\n";
        }

        // La idea es chequear A_i \sqsubseteq A_j para pares de conceptos
        // atómicos. Dump opcional de estadisticas por test (para calculo de
        // tau). Solo significativo en runs seriales: el razonador paralelo no
        // llena TestStats.
        std::ofstream statsFile;
        const char* statsPath = std::getenv("STATS_CSV");
        if (statsPath) {
            statsFile.open(statsPath);
            statsFile << "i,j,sat,mu,nodes,ms" << std::endl;
        }

        size_t count = 0;
        for (size_t i = 0; i < namedConcepts.size(); ++i) {
            for (size_t j = 0; j < namedConcepts.size(); ++j) {
                if (i == j)
                    continue;

                // Para saber si A \sqsubseteq B, chequeamos satisfacibilidad de
                // (A \sqcap \neg B) \sqcap TBox
                const auto* A = namedConcepts[i];
                const auto* notB = manager.getNegation(namedConcepts[j]);
                const auto* query = manager.getConjunction(A, notB);

                auto t0 = std::chrono::steady_clock::now();
                bool isSatisfiable = reasoner->isSatisfiable(query, defs, residualTBox);
                if (statsPath) {
                    double ms = std::chrono::duration<double, std::milli>(
                                    std::chrono::steady_clock::now() - t0)
                                    .count();
                    statsFile << i << ',' << j << ',' << isSatisfiable << ','
                              << reasoner->stats.mu << ','
                              << reasoner->stats.nodes << ',' << ms
                              << std::endl;
                }
                if (!isSatisfiable) {
                    // Se cumple la subsunción: A es subclase de B
                    recordSubsumption(A, namedConcepts[j]);
                }
                ++count;

                // Naive logging
                if (log && (count % stride == 0)) {
                    std::cout << count << "/"
                              << namedConcepts.size() * namedConcepts.size()
                              << "\n";
                }
            }
        }
    }

  private:
    void recordSubsumption(const Concept* sub, const Concept* super) {
        // No es pertinente al benchmark, pero quizás a futuro sea útil para
        // testing.
        return;
    }
};
