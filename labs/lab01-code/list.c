#include "list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void list_init(list_t *list) {
    list->head = NULL;
    list->size = 0;
}

void list_add(list_t *list, const char *data) {
    // Allocate Memory for a new node.
    // If malloc fails, print an error message and return.
    node_t *new_node = malloc(sizeof(node_t));
    if (new_node == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for the new node.\n");
        return;
    }

    // Copy the data into the new node.
    snprintf(new_node->data, MAX_LEN, "%s", data);

    // Set the new node's next pointer to NULL.
    new_node->next = NULL;

    // If the list is empty, set the head to the new node.
    // Else loop through the list until we reach the last node.
    // Setting the last node's next pointer to the new node.
    if (!list->head) {
        list->head = new_node;
    } else {
        node_t *curr_node = list->head;
        while (curr_node->next) {
            curr_node = curr_node->next;
        }
        curr_node->next = new_node;
    }

    // Increment the list's size.
    list->size++;
}

int list_size(const list_t *list) {
    return list->size;
}

char *list_get(const list_t *list, int index) {
    // Checking if index is within bounds.
    // If not, return NULL.
    if (index < 0 || index >= list->size) {
        return NULL;
    }

    // Loop through the list until we reach the index.
    node_t *curr_node = list->head;
    for (int i = 0; i < index; i++) {
        curr_node = curr_node->next;
    }

    // Return the data at the index.
    return curr_node->data;
}

void list_clear(list_t *list) {
    node_t *curr_node = list->head;

    // Traverse the list and free each node.
    while (curr_node != NULL) {
        node_t *next = curr_node->next;
        free(curr_node);
        curr_node = next;
    }

    // Reset the list's head and size.
    list->head = NULL;
    list->size = 0;
}

int list_contains(const list_t *list, const char *query) {
    // If the list is empty or the query is NULL, return 0.
    if (list == NULL || list->size == 0 || query == NULL) {
        return 0;
    }

    // Go through the list, comparing the query to each node in the list.
    node_t *curr_node = list->head;
    while (curr_node != NULL) {
        // If the query matches the current node's data, return 1.
        // Otherwise, move to the next node.
        if (strcmp(curr_node->data, query) == 0) {
            return 1;
        }
        curr_node = curr_node->next;
    }

    return 0;
}

void list_print(const list_t *list) {
    int i = 0;
    node_t *current = list->head;
    while (current != NULL) {
        printf("%d: %s\n", i, current->data);
        current = current->next;
        i++;
    }
}
