// ==================================
// reasoner.h
// ==================================

#pragma once
#include "concept.h"

class Reasoner {
  public:
    virtual ~Reasoner() = default;
    
    virtual bool isSatisfiable(const Concept* rootConcept) = 0;
};
