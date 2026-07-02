#pragma once
#include "concept_manager.h"
#include "../third_party/rapidjson/reader.h"
#include "../third_party/rapidjson/filereadstream.h"
#include <stack>
#include <string>
#include <stdexcept>
#include <cstdio>

using namespace rapidjson;

struct ASTBuilderHandler : public BaseReaderHandler<UTF8<>, ASTBuilderHandler> {
    ConceptManager& manager;
    const Concept* root = nullptr;

    struct NodeState {
        std::string type;
        std::string name;
        std::string role;
        const Concept* left = nullptr;
        const Concept* right = nullptr;
        const Concept* inner = nullptr;
        
        std::string currentKey; 
    };

    std::stack<NodeState> stateStack; 

    explicit ASTBuilderHandler(ConceptManager& m) : manager(m) {}

    bool StartObject() {
        stateStack.push(NodeState());
        return true;
    }

    bool Key(const char* str, SizeType length, bool copy) {
        stateStack.top().currentKey = std::string(str, length);
        return true;
    }

    bool String(const char* str, SizeType length, bool copy) {
        std::string val(str, length);
        auto& top = stateStack.top();
        
        if (top.currentKey == "type") top.type = val;
        else if (top.currentKey == "name") top.name = val;
        else if (top.currentKey == "role") top.role = val;
        
        return true;
    }

    bool EndObject(SizeType memberCount) {
        auto state = stateStack.top();
        stateStack.pop();

        const Concept* c = nullptr;
        if (state.type == "ATOMIC")           c = manager.getAtomic(state.name);
        else if (state.type == "NEGATION")    c = manager.getNegation(state.inner);
        else if (state.type == "CONJUNCTION") c = manager.getConjunction(state.left, state.right);
        else if (state.type == "DISJUNCTION") c = manager.getDisjunction(state.left, state.right);
        else if (state.type == "EXISTENTIAL") c = manager.getExistential(state.role, state.inner);
        else if (state.type == "UNIVERSAL")   c = manager.getUniversal(state.role, state.inner);
        else throw std::runtime_error("Unknown concept type: " + state.type);

        if (stateStack.empty()) {
            root = c;
        } else {
            auto& parent = stateStack.top();
            if (parent.currentKey == "left") parent.left = c;
            else if (parent.currentKey == "right") parent.right = c;
            else if (parent.currentKey == "inner") parent.inner = c;
        }
        return true;
    }
};


const Concept* parseOntology(ConceptManager& manager, const std::string& filepath) {
    FILE* fp = fopen(filepath.c_str(), "rb");
    if (!fp) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }

    // A 65KB buffer chunk size is optimal for standard file I/O
    char readBuffer[65536];
    FileReadStream is(fp, readBuffer, sizeof(readBuffer));

    ASTBuilderHandler handler(manager);
    Reader reader;
    
    // Parses sequentially. Handlers trigger automatically.
    ParseResult res = reader.Parse(is, handler);
    fclose(fp);

    if (!res) {
        throw std::runtime_error("JSON Parse Error: " + std::to_string(res.Code()));
    }

    return handler.root;
}
