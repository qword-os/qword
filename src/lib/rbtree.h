#ifndef __RB_TREE_H__
#define __RB_TREE_H__

#include <stdint.h>
#include <stddef.h>
#include <lib/alloc.h>

enum rb_color_t {
    RB_COLOR_BLACK,
    RB_COLOR_RED
};
 
struct rb_node {
    struct rb_node *desc[2];
    struct rb_node *anc;
    enum rb_color_t color;
};
 
typedef int (*rb_comp)(struct rb_node*, struct rb_node*, void*);
 
struct rb_root_t {
    struct rb_node *root;
    size_t node_size;
};
 
static inline struct rb_node *rb_get_par(struct rb_node *node) {
    if (node == NULL) {
        return NULL;
    }
    return node->anc;
}
 
static inline void rb_set_par(struct rb_node *node, struct rb_node *par) {
    if (node != NULL) {
        node->anc = par;
    }
}
 
static inline struct rb_node *rb_get_desc(struct rb_node *node, int pos) {
    if (node == NULL) {
        return NULL;
    }
    return node->desc[pos];
}
 
static inline void rb_set_desc(struct rb_node *node, int pos, struct rb_node *desc) {
    if (node != NULL) {
        node->desc[pos] = desc;
        rb_set_par(rb_get_desc(node, pos), node);
    }
}
 
static inline struct rb_node *rb_get_gpar(struct rb_node *node) {
    return rb_get_par(rb_get_par(node));
}
 
static inline enum rb_color_t rb_get_color(struct rb_node *node) {
    if (node == NULL) {
        return RB_COLOR_BLACK;
    }
    return node->color;
}
 
static inline void rb_set_color(struct rb_node *node, enum rb_color_t color) {
    if (node != NULL) {
        node->color = color;
    }
}
 
static inline int rb_get_pos(struct rb_node *node) {
    return rb_get_desc(rb_get_par(node), 1) == node;
}
 
static inline struct rb_node *rb_get_sib(struct rb_node *node) {
    return rb_get_desc(rb_get_par(node), 1 - rb_get_pos(node));
}
 
static inline struct rb_node *rb_get_unc(struct rb_node *node) {
    return rb_get_sib(rb_get_par(node));
}
 
static inline void *rb_rotate(struct rb_node *root, int direction, struct rb_root_t* tree) {
    int root_pos = rb_get_pos(root);
    struct rb_node *root_parent = rb_get_par(root);
    struct rb_node *new_root = rb_get_desc(root, 1 - direction);
    struct rb_node *middle = rb_get_desc(new_root, direction);
    rb_set_desc(root, 1 - direction, middle);
    rb_set_desc(new_root, direction, root);
    if (root_parent != NULL) {
        rb_set_desc(root_parent, root_pos, new_root);
    } else {
        tree->root = new_root;
        rb_set_color(new_root, RB_COLOR_BLACK);
        rb_set_par(new_root, NULL);
    }
}
 
static inline void rb_fix_insertion(struct rb_node* node, struct rb_root_t* root) {
    if (node == NULL) {
        return;
    }
    if (rb_get_color(rb_get_par(node)) == RB_COLOR_BLACK) {
        return;
    }
    struct rb_node *uncle = rb_get_unc(node);
    struct rb_node *par = rb_get_par(node);
    struct rb_node *gpar = rb_get_par(par);
    int node_pos = rb_get_pos(node);
    if (rb_get_color(uncle) == RB_COLOR_BLACK) {
        // black uncle
        if (rb_get_pos(par) == node_pos) {
            //line
            rb_rotate(gpar, 1 - node_pos, root);
            rb_set_color(gpar, RB_COLOR_RED);
            rb_set_color(par, RB_COLOR_BLACK);
        } else {
            //triangle
            rb_rotate(par, 1 - node_pos, root);
            rb_rotate(gpar, node_pos, root);
            rb_set_color(gpar, RB_COLOR_RED);
            rb_set_color(node, RB_COLOR_BLACK);
        }
    } else {
        // red uncle
        rb_set_color(par, RB_COLOR_BLACK);
        rb_set_color(uncle, RB_COLOR_BLACK);
        rb_set_color(gpar, RB_COLOR_RED);
        rb_fix_insertion(gpar, root);
    }
}
 
// 0 - node is added already, 1 - new node added
static inline int rb_insert(struct rb_root_t* root, rb_comp comp, void* comp_arg, struct rb_node* node) {
    rb_set_desc(node, 0, NULL);
    rb_set_desc(node, 1, NULL);
    if (root->root == NULL) {
        root->root = node;
        rb_set_color(root->root, RB_COLOR_BLACK);
        return 1;
    }
    struct rb_node *cur = root->root;
    while (cur != NULL) {
        int cmp = comp(node, cur, comp_arg);
        if (cmp == 0) {
            return 0;
        }
        int pos = cmp == 1;
        struct rb_node *next = rb_get_desc(cur, pos);
        if (next == NULL) {
            rb_set_desc(cur, pos, node);
            rb_set_color(node, RB_COLOR_RED);
            break;
        }
        cur = next;
    }
    rb_fix_insertion(node, root);
    rb_set_color(root->root, RB_COLOR_BLACK);
    return 1;
}
 
 
 
static inline struct rb_node *rb_find_replacement(struct rb_node *node) {
    node = rb_get_desc(node, 0);
    struct rb_node* prev = node;
    while (node != NULL) {
        prev = node;
        node = rb_get_desc(node, 1);
    }
    return prev;
}
 
static inline void rb_memswap(char* p1, char* p2, size_t size) {
    if (size % 8 == 0) {
        uint64_t* pl1 = (uint64_t*)p1;
        uint64_t* pl2 = (uint64_t*)p2;
        size_t count = size / 8;
        for (size_t i = 0; i < count; ++i) {
            uint64_t tmp = *pl1;
            *pl1 = *pl2;
            *pl2 = tmp;
        }
    } else {
        for (size_t i = 0; i < size; ++i) {
            char tmp = *p1;
            *p1 = *p2;
            *p2 = tmp;
        }
    }
}
 
static inline void rb_swap_nodes_content(struct rb_node *node1, struct rb_node *node2, struct rb_root_t *root) {
    char* content1 = (char*)(node1 + 1);
    char* content2 = (char*)(node2 + 1);
    size_t size = root->node_size - sizeof(struct rb_node);
    rb_memswap(content1, content2, size);
}
 
static struct rb_node* rb_remove_internal_nodes(struct rb_root_t *root, struct rb_node *node) {
    if (rb_get_desc(node, 0) != NULL && rb_get_desc(node, 1) != NULL) {
        struct rb_node* next = rb_find_replacement(node);
        rb_swap_nodes_content(next, node, root);
        return next;
    }
    return node;
}
 
 
static void rb_fix_double_black(struct rb_root_t *root, struct rb_node *node) {
    if (rb_get_par(node) == NULL) {
        return;
    }
    // red subling case
    struct rb_node *par = rb_get_par(node);
    struct rb_node *sib = rb_get_sib(node);
    if (rb_get_color(sib) == RB_COLOR_RED) {
        rb_rotate(par, rb_get_pos(node), root);
        rb_set_color(sib, RB_COLOR_BLACK);
        rb_set_color(par, RB_COLOR_RED);
        sib = rb_get_sib(node);
        // now the sibling will be black
    }
    // black sibling case
    // if sibling's child are all black
    if (rb_get_color(rb_get_desc(sib, 0)) == RB_COLOR_BLACK && rb_get_color(rb_get_desc(sib, 1)) == RB_COLOR_BLACK) {
        if (rb_get_color(par) == RB_COLOR_RED) {
            rb_set_color(par, RB_COLOR_BLACK);
            rb_set_color(sib, RB_COLOR_RED);
        } else {
            rb_set_color(sib, RB_COLOR_RED);
            rb_fix_double_black(root, par);
        }
        return;
    }
    // visualizer preferred 1 instead of 0, so I used
    // it to remain consistent
    struct rb_node *red_nephew = rb_get_desc(sib, 1);
    if (rb_get_color(red_nephew) == RB_COLOR_BLACK) {
        red_nephew = rb_get_desc(sib, 0);
    }
    if (rb_get_pos(node) == rb_get_pos(red_nephew)) {
        rb_rotate(sib, 1 - rb_get_pos(node), root);
        rb_rotate(par, rb_get_pos(node), root);
        rb_set_color(red_nephew, RB_COLOR_BLACK);
    } else {
        rb_rotate(par, rb_get_pos(node), root);
        rb_set_color(red_nephew, RB_COLOR_BLACK);
    }
}
 
// 0 - there is no such node, 1 - node deleted
static inline void rb_delete(struct rb_root_t *root, struct rb_node *node) {
    // handle case with internal node
    node = rb_remove_internal_nodes(root, node);
    // get node's child
    struct rb_node *chld = rb_get_desc(node, 0);
    if (chld == NULL) {
        chld = rb_get_desc(node, 1);
    }
    // if it is a single-child parent, it must be black
    if (chld != NULL) {
        rb_set_desc(rb_get_par(node), rb_get_pos(node), chld);
        rb_set_color(chld, RB_COLOR_BLACK);
        kfree(node);
        return;
    }
    // if node to be deleted is a red leaf, remove leaf, done
    if (chld == NULL && rb_get_color(node) == RB_COLOR_RED) {
        rb_set_desc(rb_get_par(node), rb_get_pos(node), NULL);
        kfree(node);
        return;
    }
    // now node is nothing but black leaf
    rb_fix_double_black(root, node);
    rb_set_desc(rb_get_par(node), rb_get_pos(node), NULL);
    kfree(node);
}
 
static inline struct rb_node *rb_query(struct rb_root_t *root, struct rb_node* node, rb_comp comp, void* comp_arg) {
    struct rb_node *current = root->root;
    while (current != NULL) {
        int cmp = comp(node, current, comp_arg);
        int pos = cmp == 1;
        if (cmp == 0) {
            return current;
        } else {
            current = rb_get_desc(current, pos);
        }
    }
    return NULL;
}

#endif