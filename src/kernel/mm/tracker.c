#include "mm/kheap.h"
#include <lib/stddef.h>
#include <mm/tracker.h>
#include <stdint.h>

struct tracker_node {
  uintptr_t start; // inclusive
  size_t len;      // number of bytes
  struct tracker_node *left, *right;
  int height;

  size_t max_free;
};

// ----- helpers ------

static inline uintptr_t uadd_overflow(uintptr_t a, uintptr_t b,
                                      uintptr_t *out) {
  uintptr_t c = a + b;
  if (c < a)
    return 1;
  *out = c;
  return 0;
}

// static inline size_t ssub_sat(size_t a, size_t b) {
//   return (a > b) ? (a - b) : 0;
// }

static inline uintptr_t align_up(uintptr_t x, size_t a) {
  if (a == 0 || (a & (a - 1)) != 0) { // power of two or zero
    return (a == 0) ? x : ((x + (a - 1)) / a) * a;
  }
  return (x + (a - 1)) & ~(uintptr_t)(a - 1);
}

static inline int height(tracker_node_t *n) { return n ? n->height : 0; }

static inline size_t max_sz(size_t a, size_t b) { return a > b ? a : b; }

// static inline size_t node_len(tracker_node_t *n) {
//   return n ? n->len : 0;
// }

static inline size_t subtree_max_free(tracker_node_t *n) {
  return n ? n->max_free : 0;
}

static void pull_up(tracker_node_t *n) {
  if (!n)
    return;
  int hl = height(n->left), hr = height(n->right);
  n->height = (hl > hr ? hl : hr) + 1;
  n->max_free = max_sz(
      n->len, max_sz(subtree_max_free(n->left), subtree_max_free(n->right)));
}

static tracker_node_t *rotate_right(tracker_node_t *y) {
  tracker_node_t *x = y->left;
  tracker_node_t *T2 = x->left;
  x->right = y;
  y->left = T2;
  pull_up(y);
  pull_up(x);
  return x;
}

static tracker_node_t *rotate_left(tracker_node_t *x) {
  tracker_node_t *y = x->right;
  tracker_node_t *T2 = y->left;
  y->left = x;
  x->right = T2;
  pull_up(x);
  pull_up(y);
  return y;
}

static int balance_factor(tracker_node_t *n) {
  return n ? height(n->left) - height(n->right) : 0;
}

static tracker_node_t *new_node(uintptr_t start, size_t len) {
  tracker_node_t *n = kmalloc(sizeof(*n));
  if (!n)
    return NULL;
  n->start = start;
  n->len = len;
  n->left = n->right = NULL;
  n->height = 1;
  n->max_free = len;
  return n;
}

// insert/delete

static tracker_node_t *avl_insert_exact(tracker_node_t *root, uintptr_t start,
                                        size_t len, bool *ok) {
  if (!root) {
    *ok = true;
    return new_node(start, len);
  }
  if (start < root->start) {
    root->left = avl_insert_exact(root->left, start, len, ok);
  } else if (start > root->start) {
    root->right = avl_insert_exact(root->right, start, len, ok);
  } else {
    // duplicate start is unexpected in free map (overlap); reject
    *ok = false;
    return root;
  }


  // balance tree
  pull_up(root);
  int bf = balance_factor(root);

  if (bf > 1 && start < root->left->start)
    return rotate_right(root);
  if (bf < -1 && start > root->right->start)
    return rotate_left(root);
  if (bf > 1 && start > root->left->start) {
    root->left = rotate_left(root->left);
    return rotate_right(root);
  }
  if (bf < -1 && start < root->right->start) {
    root->right = rotate_right(root->right);
    return rotate_left(root);
  }

  return root;
}

static tracker_node_t *min_node(tracker_node_t *n) {
  while (n && n->left)
    n = n->left;
  return n;
}

static tracker_node_t *avl_delete_by_start(tracker_node_t *root,
                                           uintptr_t start,
                                           tracker_node_t **removed) {
  if (!root)
    return NULL;

  if (start < root->start) {
    root->left = avl_delete_by_start(root->left, start, removed);
  } else if (start > root->start) {
    root->right = avl_delete_by_start(root->right, start, removed);
  } else {
    // found
    *removed = root;
    if (!root->left || !root->right) {
      tracker_node_t *tmp = root->left ? root->left : root->right;
      // adopt child (may be NULL)
      if (!tmp) {
        root = NULL;
      } else {
        *root = *tmp; // copy over contents
        kfree(tmp);
      }
    } else {
      // two children: lift successor
      tracker_node_t *succ = min_node(root->right);
      root->start = succ->start;
      root->len = succ->len;
      // IMPORTANT: we cannot just copy max_free/height; recompute via pull_up
      tracker_node_t *dummy = NULL;
      root->right = avl_delete_by_start(root->right, succ->start, &dummy);
      if (dummy)
        kfree(dummy);
    }
  }

  if (!root)
    return NULL;

  pull_up(root);
  int bf = balance_factor(root);

  if (bf > 1 && balance_factor(root->left) >= 0)
    return rotate_right(root);
  if (bf > 1 && balance_factor(root->left) < 0) {
    root->left = rotate_left(root->left);
    return rotate_right(root);
  }
  if (bf < -1 && balance_factor(root->right) <= 0)
    return rotate_left(root);
  if (bf < -1 && balance_factor(root->right) > 0) {
    root->right = rotate_right(root->right);
    return rotate_left(root);
  }

  return root;
}

/* --------- predecessor / successor --------- */

static tracker_node_t *find_predecessor(tracker_node_t *root, uintptr_t start) {
  tracker_node_t *pred = NULL;
  while (root) {
    if (start <= root->start) {
      root = root->left;
    } else {
      pred = root;
      root = root->right;
    }
  }
  return pred;
}

static tracker_node_t *find_successor(tracker_node_t *root, uintptr_t start) {
  tracker_node_t *succ = NULL;
  while (root) {
    if (start < root->start) {
      succ = root;
      root = root->left;
    } else {
      root = root->right;
    }
  }
  return succ;
}

/* --------- find-fit (first-fit w/ alignment & pruning via max_free) ---------
 */

static tracker_node_t *find_fit(tracker_node_t *n, size_t size, size_t align,
                                uintptr_t *alloc_start_out) {
  if (!n || subtree_max_free(n) < size)
    return NULL;

  // Try left first
  tracker_node_t *left_fit = find_fit(n->left, size, align, alloc_start_out);
  if (left_fit)
    return left_fit;

  // Check current node
  uintptr_t s_aligned = align_up(n->start, align);
  uintptr_t node_end;
  if (uadd_overflow(n->start, n->len, &node_end)) {
    // corrupt node; treat as unusable
  } else if (s_aligned >= n->start && s_aligned <= node_end) {
    size_t lead = (size_t)(s_aligned - n->start);
    if (lead <= n->len) {
      size_t usable = n->len - lead;
      if (usable >= size) {
        *alloc_start_out = s_aligned;
        return n;
      }
    }
  }

  // Try right
  return find_fit(n->right, size, align, alloc_start_out);
}

/* --------- public API --------- */

void tracker_init(tracker_tree_t *t) { t->root = NULL; }

// Insert a free range WITHOUT coalescing (internal helper).
bool tracker_add_free(tracker_tree_t *t, uintptr_t start, size_t len) {
  if (len == 0)
    return true;
  // overflow check (start + len)
  uintptr_t end;
  if (uadd_overflow(start, len, &end))
    return false;

  bool ok = false;
  t->root = avl_insert_exact(t->root, start, len, &ok);
  return ok;
}

// Insert with coalescing (public API)
bool tracker_add(tracker_tree_t *t, uintptr_t start, size_t len) {
  if (len == 0)
    return true;

  uintptr_t end;
  if (uadd_overflow(start, len, &end))
    return false;

  // Find neighbors
  tracker_node_t *L = find_predecessor(t->root, start);
  tracker_node_t *R = find_successor(t->root, start);

  // Merge with left if adjacent
  if (L) {
    uintptr_t Lend;
    if (!uadd_overflow(L->start, L->len, &Lend) && Lend == start) {
      // Remove L, extend [start,len] leftwards
      tracker_node_t *removed = NULL;
      t->root = avl_delete_by_start(t->root, L->start, &removed);
      if (removed)
        kfree(removed);
      start = L->start;
      len = (size_t)(end - start);
    }
  }

  // Merge with right if adjacent
  if (R) {
    if (end == R->start) {
      tracker_node_t *removed = NULL;
      t->root = avl_delete_by_start(t->root, R->start, &removed);
      if (removed) {
        // extend rightwards by R->len
        uintptr_t new_end;
        if (uadd_overflow(end, removed->len, &new_end)) {
          kfree(removed);
          return false;
        }
        end = new_end;
        len = (size_t)(end - start);
        kfree(removed);
      }
    }
  }

  return tracker_add_free(t, start, len);
}

// Allocate `size` bytes with `align` (0 or power-of-two is best). Returns
// true/false. On success, *out_addr is set.
bool tracker_reserve(tracker_tree_t *t, size_t size, size_t align,
                   uintptr_t *out_addr) {
  if (!t)
    return false;
  if (size == 0) {
    *out_addr = 0;
    return true;
  }

  uintptr_t alloc_start = 0;
  tracker_node_t *fit =
      find_fit(t->root, size, align ? align : 1, &alloc_start);
  if (!fit)
    return false;

  // Remove the chosen node, then reinsert any leftover fragments
  tracker_node_t *removed = NULL;
  t->root = avl_delete_by_start(t->root, fit->start, &removed);
  if (!removed)
    return false; // should not happen
  uintptr_t node_start = removed->start;
  size_t node_len = removed->len;
  kfree(removed);

  // Split into up to two fragments around [alloc_start, alloc_start+size)
  uintptr_t alloc_end;
  if (uadd_overflow(alloc_start, size, &alloc_end))
    return false;

  // Left fragment
  if (alloc_start > node_start) {
    size_t left_len = (size_t)(alloc_start - node_start);
    bool ok = tracker_add_free(t, node_start, left_len);
    if (!ok)
      return false;
  }

  // Right fragment
  uintptr_t node_end = node_start + node_len;
  if (alloc_end < node_end) {
    size_t right_len = (size_t)(node_end - alloc_end);
    bool ok = tracker_add_free(t, alloc_end, right_len);
    if (!ok)
      return false;
  }

  *out_addr = alloc_start;
  return true;
}
