#pragma once
#include "concept_manager.h"
#include "nnf.h"
#include "reasoner.h"
#include "ast_builder.h"
#include <iostream>
#include <vector>

class KnowledgeBase {
private:
    ConceptManager manager;
    std::vector<const Concept*> namedConcepts; // Discovered during parsing
    const Concept* globalTBox; // The internalized, GCI-combined \sqcap axiom

public:
    // Inicialización y preprocesamiento a NNF
    void loadAndPreprocess(const std::string& filepath) {
        ConceptManager sourceManager;
        const Concept* rawRoot = parseOntology(sourceManager, filepath);
        
        NNFTransformer transformer(manager);
        globalTBox = transformer.convertToNNF(rawRoot);
        
        // Todos los conceptos atómicos para el loop de clasificación
        namedConcepts = manager.getAllAtomicConcepts(); 
    }

    // Chequeo de consistencia
    bool isConsistent(std::unique_ptr<Reasoner>& reasoner) {
        // En un benchmark sin ABox (nuestro caso), consistencia es simplemente chequear si la TBox global
        return reasoner->isSatisfiable(globalTBox);
    }

    // Clasificación
    void classify(const bool log, std::unique_ptr<Reasoner>& reasoner) {
        int stride = 100;
        if (log) {
            std::cout << "Comenzando clasificación con " << namedConcepts.size() << "X" << namedConcepts.size() << " = " << namedConcepts.size() * namedConcepts.size() << " subsunciones." << "\n";
        }
        
        // La idea es chequear A_i \sqsubseteq A_j para pares de conceptos atómicos. 
        size_t count = 0;
        for (size_t i = 0; i < namedConcepts.size(); ++i) {
            for (size_t j = 0; j < namedConcepts.size(); ++j) {
                if (i == j) continue;
                
                // Para saber si A \sqsubseteq B, chequeamos satisfacibilidad de (A \sqcap \neg B) \sqcap TBox
                const auto* A = namedConcepts[i];
                const auto* notB = manager.getNegation(namedConcepts[j]);
                const auto* query = manager.getConjunction(A, notB);
                
                bool isSatisfiable = reasoner->isSatisfiable(query, globalTBox);
                if (!isSatisfiable) {
                    // Se cumple la subsunción: A es subclase de B
                    recordSubsumption(A, namedConcepts[j]);
                }
                ++count;

                // Naive logging
                if (log && (count % stride == 0)) {
                    std::cout << count << "/" << namedConcepts.size() * namedConcepts.size() << "\n";
                }
            }
        }
    }

private:
    void recordSubsumption(const Concept* sub, const Concept* super) {
        // No es pertinente al benchmark, pero quizás a futuro sea útil para testing.
        return;
    }
};
