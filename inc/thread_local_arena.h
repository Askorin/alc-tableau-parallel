// ==================================
// thread_local_arena.h
// ==================================

#pragma once
#include "tableau_node.h"
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <algorithm>

class ThreadLocalArena {
private:
    TableauNode* node_pool;
    const Concept** label_pool;
    Edge* edge_pool;

    size_t capacity;
    
    size_t node_bump = 0;
    size_t label_bump = 0;
    size_t edge_bump = 0;
    const ThreadLocalArena* prefix = nullptr;
    size_t base_node = 0;
    size_t base_label = 0;
    size_t base_edge = 0;
    size_t task_root_idx = SIZE_MAX;

    const ThreadLocalArena* getOwner(size_t idx) const {
        const ThreadLocalArena* curr = this;
        while (curr != nullptr) {
            if ((idx >= curr->base_node && idx < curr->node_bump) || idx == curr->task_root_idx) {
                return curr;
            }
            curr = curr->prefix;
        }
        std::cerr << "FATAL: Node index out of prefix bounds.\n";
        exit(1);
    }

    void reallocatePools(size_t new_capacity) {
        TableauNode* new_node_pool = static_cast<TableauNode*>(std::realloc(node_pool, new_capacity * sizeof(TableauNode)));
        const Concept** new_label_pool = static_cast<const Concept**>(std::realloc(label_pool, new_capacity * 10 * sizeof(const Concept*)));
        Edge* new_edge_pool = static_cast<Edge*>(std::realloc(edge_pool, new_capacity * 2 * sizeof(Edge)));
        
        if (!new_node_pool || !new_label_pool || !new_edge_pool) {
            std::cerr << "FATAL: Arena reallocation failed.\n"; 
            exit(1);
        }
        
        node_pool = new_node_pool;
        label_pool = new_label_pool;
        edge_pool = new_edge_pool;
        capacity = new_capacity;
    }

public:
    explicit ThreadLocalArena(size_t initial_nodes = 50000) : capacity(initial_nodes) {
        node_pool = static_cast<TableauNode*>(std::malloc(capacity * sizeof(TableauNode)));
        label_pool = static_cast<const Concept**>(std::malloc(capacity * 10 * sizeof(const Concept*)));
        edge_pool = static_cast<Edge*>(std::malloc(capacity * 2 * sizeof(Edge)));
        
        if (!node_pool || !label_pool || !edge_pool) {
            std::cerr << "FATAL: Arena OOM.\n"; exit(1);
        }
    }

    ~ThreadLocalArena() {
        std::free(node_pool);
        std::free(label_pool);
        std::free(edge_pool);
    }

    ThreadLocalArena(const ThreadLocalArena&) = delete;
    ThreadLocalArena& operator=(const ThreadLocalArena&) = delete;

    void reset() {
        node_bump = label_bump = edge_bump = 0;
        base_node = base_label = base_edge = 0;
        prefix = nullptr;
        task_root_idx = SIZE_MAX;
    }

    // ==========================================
    // SEMANTICS MOVE 
    // ==========================================
    ThreadLocalArena(ThreadLocalArena&& other) noexcept 
        : node_pool(other.node_pool), label_pool(other.label_pool), edge_pool(other.edge_pool),
          capacity(other.capacity), node_bump(other.node_bump), 
          label_bump(other.label_bump), edge_bump(other.edge_bump) {
        
        other.node_pool = nullptr;
        other.label_pool = nullptr;
        other.edge_pool = nullptr;
        other.capacity = 0;
        other.node_bump = 0;
        other.label_bump = 0;
        other.edge_bump = 0;
    }

    ThreadLocalArena& operator=(ThreadLocalArena&& other) noexcept {
        if (this != &other) {
            std::free(node_pool);
            std::free(label_pool);
            std::free(edge_pool);

            node_pool = other.node_pool;
            label_pool = other.label_pool;
            edge_pool = other.edge_pool;
            capacity = other.capacity;
            node_bump = other.node_bump;
            label_bump = other.label_bump;
            edge_bump = other.edge_bump;
            
            other.node_pool = nullptr;
            other.label_pool = nullptr;
            other.edge_pool = nullptr;
            other.capacity = 0;
            other.node_bump = 0;
            other.label_bump = 0;
            other.edge_bump = 0;
        }
        return *this;
    }

    // ==========================================
    // ASIGNACIÓN
    // ==========================================
    size_t allocateNode(size_t parent_idx = SIZE_MAX) {
        if (node_bump >= capacity) {
            reallocatePools(capacity * 2);
        }
        size_t idx = node_bump++;
        node_pool[idx] = {label_bump, 0, edge_bump, 0, parent_idx};
        return idx;
    }

    // ==========================================
    // MUTACION ORIENTADA A DATOS 
    // ==========================================
    void addLabelToNode(size_t node_idx, const Concept* c) {
        if (node_idx < base_node && node_idx != task_root_idx) {
            std::cerr << "FATAL: CoW violation! Node " << node_idx << " is immutable.\n";
            exit(1);
        }
        // Bounds checking
        if (label_bump + node_pool[node_idx].label_count + 1 > capacity * 10) {
            reallocatePools(capacity * 2);
        }

        TableauNode& node = node_pool[node_idx];
        
        auto start_it = label_pool + node.label_offset;
        auto end_it = start_it + node.label_count;
        auto it = std::lower_bound(start_it, end_it, c);
        
        if (it != end_it && *it == c) return;

        if (node.label_offset + node.label_count == label_bump) {
            std::move_backward(it, end_it, end_it + 1);
            *it = c;
            node.label_count++;
            label_bump++;
        } else {
            size_t new_offset = label_bump;
            std::memcpy(label_pool + new_offset, label_pool + node.label_offset, node.label_count * sizeof(const Concept*));
            
            node.label_offset = new_offset;
            
            start_it = label_pool + node.label_offset;
            end_it = start_it + node.label_count;
            it = std::lower_bound(start_it, end_it, c);
            
            std::move_backward(it, end_it, end_it + 1);
            *it = c;
            
            node.label_count++;
            label_bump += node.label_count; 
        }
    }

    void addEdgeToNode(size_t node_idx, std::string_view role, size_t child_idx) {
        if (edge_bump + node_pool[node_idx].edge_count + 1 > capacity * 2) {
            reallocatePools(capacity * 2);
        }

        TableauNode& node = node_pool[node_idx];

        if (node.edge_offset + node.edge_count == edge_bump) {
            edge_pool[edge_bump] = {role, child_idx};
            node.edge_count++;
            edge_bump++;
        } else {
            size_t new_offset = edge_bump;
            std::memcpy(edge_pool + new_offset, edge_pool + node.edge_offset, node.edge_count * sizeof(Edge));
            
            node.edge_offset = new_offset;
            edge_pool[edge_bump + node.edge_count] = {role, child_idx};
            
            node.edge_count++;
            edge_bump += node.edge_count; 
        }
    }

    // ==========================================
    // BACKTRACKING & ESTADO
    // ==========================================
    struct State { size_t n, l, e; };

    State saveState() const {
        return {node_bump, label_bump, edge_bump};
    }

    size_t getCapacity() const { return capacity; }

    void bindPrefix(const ThreadLocalArena& parent, State s, size_t override_idx) {
        if (capacity < parent.getCapacity()) {
            reallocatePools(parent.getCapacity()); 
        }

        prefix = &parent;
        base_node = s.n;
        base_label = s.l;
        base_edge = s.e;
        
        node_bump = base_node;
        label_bump = base_label;
        edge_bump = base_edge;
        task_root_idx = override_idx;

        if (override_idx != SIZE_MAX) {
            const ThreadLocalArena* p_owner = parent.getOwner(override_idx);
            
            TableauNode imported_node = p_owner->node_pool[override_idx];
            
            auto l_start = p_owner->label_pool + imported_node.label_offset;
            imported_node.label_offset = label_bump;
            for (size_t i = 0; i < imported_node.label_count; ++i) {
                label_pool[label_bump++] = *(l_start + i);
            }
            
            auto e_start = p_owner->edge_pool + imported_node.edge_offset;
            imported_node.edge_offset = edge_bump;
            for (size_t i = 0; i < imported_node.edge_count; ++i) {
                edge_pool[edge_bump++] = *(e_start + i);
            }
            
            node_pool[override_idx] = imported_node;
        }
    }

    // ==========================================
    // VISTAS ZERO-COST & ACCESORES
    // ==========================================


    struct LabelView {
        const Concept** begin_ptr;
        const Concept** end_ptr;
        const Concept** begin() const { return begin_ptr; }
        const Concept** end() const { return end_ptr; }
        size_t size() const { return end_ptr - begin_ptr; }
    };

    struct EdgeView {
        Edge* begin_ptr;
        Edge* end_ptr;
        Edge* begin() const { return begin_ptr; }
        Edge* end() const { return end_ptr; }
        size_t size() const { return end_ptr - begin_ptr; }
    };

    const TableauNode& getNode(size_t index) const {
        return getOwner(index)->node_pool[index];
    }
    
    TableauNode& getNode(size_t index) {
        return node_pool[index];
    }

    LabelView getLabels(size_t index) const {
        const ThreadLocalArena* owner = getOwner(index);
        const TableauNode& node = owner->node_pool[index];
        return {owner->label_pool + node.label_offset, 
                owner->label_pool + node.label_offset + node.label_count};
    }

    EdgeView getEdges(size_t index) const {
        const ThreadLocalArena* owner = getOwner(index);
        const TableauNode& node = owner->node_pool[index];
        return {owner->edge_pool + node.edge_offset, 
                owner->edge_pool + node.edge_offset + node.edge_count};
    }

    bool hasLabel(size_t index, const Concept* c) const {
        const ThreadLocalArena* owner = getOwner(index);
        const TableauNode& node = owner->node_pool[index];
        
        auto start_it = owner->label_pool + node.label_offset;
        auto end_it = start_it + node.label_count;
        auto it = std::lower_bound(start_it, end_it, c);
        return it != end_it && *it == c;
    }
};
