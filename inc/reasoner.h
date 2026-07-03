// ==================================
// reasoner.h
// ==================================

#pragma once
#include "concept.h"

class Reasoner {
  public:
    virtual ~Reasoner() = default;
    
    virtual bool isSatisfiable(const Concept* query, const Concept* tbox = nullptr) = 0;
};
