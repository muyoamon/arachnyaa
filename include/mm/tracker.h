#ifndef ARACHNYAA_MM_TRACKER_H_
#define ARACHNYAA_MM_TRACKER_H_

#include <lib/stddef.h>
#include <stdbool.h>
#include <stdint.h>


//
// Virtual Address Tracker
//

typedef struct tracker_node tracker_node_t;

typedef struct {
  tracker_node_t *root;
} tracker_tree_t;


// Initialize tracker tree
void tracker_init(tracker_tree_t *root);

// Add node to tracker tree
bool tracker_add(tracker_tree_t *t, uintptr_t start, size_t len);

// reserve memory in tracker tree
bool tracker_reserve(tracker_tree_t *t, size_t size, size_t align, uintptr_t *out_addr);





#endif // ARACHNYAA_MM_TRACKER_H_
