// ==================================
// reasoner.h
// ==================================

#pragma once
#include "concept.h"
#include <cstddef>
#include <cstdint>

class Reasoner {
  public:
    virtual ~Reasoner() = default;

    // Estadisticas por test de satisfacibilidad 
    // mu = ramas conjuntivas del primer frontier generado (paper, Eq. 3)
    struct TestStats {
        size_t mu = 0;
        uint64_t nodes = 0;
    };
    TestStats stats;

    virtual bool isSatisfiable(const Concept* query, const Concept* tbox = nullptr) = 0;
};
