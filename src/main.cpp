#include "../inc/concept_manager.h"
#include "../inc/knowledge_base.h"
#include "../inc/parallel_reasoner.h"
#include "../inc/reasoner.h"
#include "../inc/serial_reasoner.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <omp.h>
#include <unordered_map>

template <typename ReasonerT>
void runUnitTestsWith() {
    ConceptManager m;
    ReasonerT reasoner;

    // Sin absorcion: mapa de definiciones vacio
    const std::unordered_map<const Concept*, const Concept*> noDefs;

    // Atomicos Base
    const auto* A = m.getAtomic("A");
    const auto* B = m.getAtomic("B");
    const auto* C = m.getAtomic("C");
    const auto* notA = m.getNegation(A);
    const auto* notB = m.getNegation(B);
    const auto* notC = m.getNegation(C);

    // Test 1: A \sqcap \neg A (Insatisfacible)
    const auto* test1 = m.getConjunction(A, notA);
    assert(reasoner.isSatisfiable(test1, noDefs) == false && "Test 1 failed: Simple Clash");

    // Test 2: (A \sqcup B) \sqcap \neg A (Satisfacible)
    const auto* A_or_B = m.getDisjunction(A, B);
    const auto* test2 = m.getConjunction(A_or_B, notA);
    assert(reasoner.isSatisfiable(test2, noDefs) == true && "Test 2 failed: Simple Disjunction");

    // Test 3: \exists R.A \sqcap \forall R.\neg A (Insatisfacible)
    const auto* someR_A = m.getExistential("R", A);
    const auto* allR_notA = m.getUniversal("R", notA);
    const auto* test3 = m.getConjunction(someR_A, allR_notA);
    assert(reasoner.isSatisfiable(test3, noDefs) == false && "Test 3 failed: Exists + Forall Clash");

    // Test 4: \exists R.A \sqcap \forall R.(\exists R.A) (Satisfacible)
    const auto* someR_A_inner = m.getExistential("R", A);
    const auto* allR_some = m.getUniversal("R", someR_A_inner);
    const auto* test4 = m.getConjunction(someR_A, allR_some);
    assert(reasoner.isSatisfiable(test4, noDefs) == true && "Test 4 failed: Nested Exists + Forall");


    // Test 5: Disyunción profunda 
    // (\exists R. (A \sqcup B)) \sqcap (\forall R. \neg A) \sqcap (\forall R. \neg B)
    // Insatisfacible
    const auto* notA_and_notB = m.getConjunction(notA, notB);
    const auto* allR_notA_notB = m.getUniversal("R", notA_and_notB);
    const auto* someR_A_or_B = m.getExistential("R", A_or_B);
    const auto* test5 = m.getConjunction(someR_A_or_B, allR_notA_notB);
    assert(reasoner.isSatisfiable(test5, noDefs) == false && "Test 5 failed: Deep Disjunction Root-Isolation Bug");

    // Test 6: Independecia de siblings
    // \exists R.(A \sqcup B) \sqcap \exists R.(\neg A \sqcap \neg B)
    // Satisfacible: se crean dos R-sucesores separados, no interactuan.
    const auto* someR_notA_notB = m.getExistential("R", notA_and_notB);
    const auto* test6 = m.getConjunction(someR_A_or_B, someR_notA_notB);
    assert(reasoner.isSatisfiable(test6, noDefs) == true && "Test 6 failed: Sibling Independence Corrupted");

    // Test 7: Backtracking exhaustivo
    // (A \sqcup B) \sqcap (\neg A \sqcup B) \sqcap (A \sqcup \neg B) \sqcap (\neg A \sqcup \neg B)
    // Insatisfacible
    const auto* notA_or_B = m.getDisjunction(notA, B);
    const auto* A_or_notB = m.getDisjunction(A, notB);
    const auto* notA_or_notB = m.getDisjunction(notA, notB);
    
    auto* t7_1 = m.getConjunction(A_or_B, notA_or_B);
    auto* t7_2 = m.getConjunction(t7_1, A_or_notB);
    auto* test7 = m.getConjunction(t7_2, notA_or_notB);
    assert(reasoner.isSatisfiable(test7, noDefs) == false && "Test 7 failed: Backtracking State Corruption");

    // Test 8: Blocking basado en disyunción
    // A \sqcap \exists R. ( B \sqcup \exists R. A )
    // Satisfacible: Rama izquierda evalua B, rama derecha crea \exists R. A,que genera un hijo con {A}
    // ese hijo es subcjto de raiz {A, \exists R...}, así que debe triggerear isBlocked()
    const auto* someR_A_test8 = m.getExistential("R", A);
    const auto* B_or_someR_A = m.getDisjunction(B, someR_A_test8);
    const auto* someR_B_or = m.getExistential("R", B_or_someR_A);
    const auto* test8 = m.getConjunction(A, someR_B_or);
    
    assert(reasoner.isSatisfiable(test8, noDefs) == true && "Test 8 failed: Disjunction Ancestor Blocking Corrupted");

    // ============ Tests de absorcion / lazy unfolding ============

    // Test 9: unfolding encadenado + interaccion con \forall
    // defs: A -> B, B -> \exists R.C ; query: A \sqcap \forall R.\neg C
    // A dispara B, B dispara \exists R.C; el hijo {C} recibe \neg C => UNSAT
    std::unordered_map<const Concept*, const Concept*> defs9;
    defs9[A] = B;
    defs9[B] = m.getExistential("R", C);
    const auto* allR_notC = m.getUniversal("R", notC);
    const auto* test9 = m.getConjunction(A, allR_notC);
    assert(reasoner.isSatisfiable(test9, defs9) == false && "Test 9 failed: Chained Unfolding + Forall");
    // Sin defs, el mismo query es SAT (las definiciones no aplican)
    assert(reasoner.isSatisfiable(test9, noDefs) == true && "Test 9b failed: Unfolding fired without defs");

    // Test 10: definiciones ciclicas terminan via blocking
    // defs: A -> \exists R.B, B -> \exists R.A ; query: A => SAT (cadena bloqueada)
    std::unordered_map<const Concept*, const Concept*> defs10;
    defs10[A] = m.getExistential("R", B);
    defs10[B] = m.getExistential("R", A);
    assert(reasoner.isSatisfiable(A, defs10) == true && "Test 10 failed: Cyclic Unfolding + Blocking");
}

void runUnitTests() {
    runUnitTestsWith<SerialReasoner>();
    runUnitTestsWith<ParallelReasoner>();
}

int main(int argc, char** argv) {

    omp_set_max_active_levels(1);
    if (argc > 1 && std::string(argv[1]) == "test") {
        runUnitTests();
        std::cout << "UNIT_TESTS_PASSED\n";
        return 0; 
    }

    std::string filepath = "../data/tea_ast/tea_for_testing_trace_13.json";
    std::unique_ptr<Reasoner> reasoner = std::make_unique<SerialReasoner>();

    if (argc > 1) {
        filepath = argv[1];
    }
    if (argc > 2) {
        std::string reasonerType(argv[2]);

        if (reasonerType == "parallel") {
            if (argc < 4) {
                std::cerr << "Error: razonador paralelo requiere número de threads"
                             "(e.g., ./main archivo.json parallel 4)\n";
                return 1;
            }
            int n_threads = std::stoi(argv[3]);
            omp_set_num_threads(
                n_threads); 
            reasoner = std::make_unique<ParallelReasoner>();
        } else if (reasonerType != "serial") {
            std::cerr << "Error: tipo de razonador incorrecto'" << reasonerType
                      << "'. Usar 'serial' o 'parallel'.\n";
            return 1;
        }
    }

    KnowledgeBase kb;

    try {
        auto startLoad = std::chrono::high_resolution_clock::now();
        kb.loadAndPreprocess(filepath);
        auto endLoad = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> loadTime =
            endLoad - startLoad;

        auto startCons = std::chrono::high_resolution_clock::now();
        bool isConsistent =
            kb.isConsistent(reasoner); // Variable kept to ensure evaluation
        auto endCons = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> consTime =
            endCons - startCons;

        auto startClass = std::chrono::high_resolution_clock::now();
        kb.classify(false, reasoner);
        auto endClass = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> classTime =
            endClass - startClass;

        std::cout << "LOAD_MS=" << loadTime.count() << "\n";
        std::cout << "CONSISTENCY_MS=" << consTime.count() << "\n";
        std::cout << "CLASSIFICATION_MS=" << classTime.count() << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
