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


/**
 * @brief Initialize tracker tree.
 *
 * @param root pointer to tracker tree
 */
void tracker_init(tracker_tree_t *root);

// Add node to tracker tree
/**
 * @brief Add node to tracker tree.
 *
 * @param t pointer to tracker tree.
 * @param start base address.
 * @param len size in bytes.
 * @return true if success, false otherwise.
 */
bool tracker_add(tracker_tree_t *t, uintptr_t start, size_t len);

// reserve memory in tracker tree
/**
 * @brief Reserve memory in tracker tree. 
 *
 * @param t pointer to tracker tree.
 * @param size size in bytes.
 * @param align alignment.
 * @param out_addr output address.
 * @return true if success, false otherwise.
 */
bool tracker_reserve(tracker_tree_t *t, size_t size, size_t align, uintptr_t *out_addr);





#endif // ARACHNYAA_MM_TRACKER_H_
