#include "SkipList.hpp"

auto SkipList::destroy(const Node *node) -> void {
    if (node == nullptr) return;

    SkipList::destroy(node->down);
    SkipList::destroy(node->next);
    delete node;
}
