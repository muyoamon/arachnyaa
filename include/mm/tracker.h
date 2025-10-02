#ifndef ARACHNYAA_MM_TRACKER_H_
#define ARACHNYAA_MM_TRACKER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


//
// Virtual Address Tracker
//

typedef struct tracker_node tracker_node_t;

typedef struct {
  tracker_node_t *root;
} tracker_tree_t;

void tracker_init(tracker_tree_t *root);

bool tracker_add_free(tracker_tree_t *t, uintptr_t start, size_t len);

bool tracker_free(tracker_tree_t *t, uintptr_t start, size_t len);

bool tracker_alloc(tracker_tree_t *t, size_t size, size_t align, uintptr_t *out_addr);





#endif // ARACHNYAA_MM_TRACKER_H_
