#include "route.h"
#include "lib_record.h"

#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

#define MAX_NODE_NUMBERS 600
#define MAX_OUTWARD_PATH_NUMBERS 8

struct node;
struct path;

struct path {
    unsigned short index;
    struct node* from;
    struct node* to;
    unsigned char weight;
};

struct node {
    // Constant
    unsigned int outward_paths_size:4;
    struct path* outward_paths[MAX_OUTWARD_PATH_NUMBERS];
    bool is_via_node:1;

    // Variable
    bool has_been_visited:1;
    unsigned int current_outward_path_index:4;
};

struct stack {
    struct path** bottom;
    struct path** top;
};

static void stack_init(struct stack* stack, unsigned int max_count) {
    stack->bottom = (struct path**) malloc(max_count * sizeof(struct path*));
    stack->top = stack->bottom;
}

static void stack_copy(struct stack* to, struct stack* from) {
    size_t size = from->top - from->bottom;
    to->top = to->bottom + size;
    memcpy(to->bottom, from->bottom, size * sizeof(struct path**));
}

static void stack_free(struct stack* stack) {
    free(stack->bottom);
    stack->bottom = NULL;
    stack->top = NULL;
}

static void stack_push(struct stack* stack, struct path* path) {
    *stack->top = path;
    stack->top++;
}

static struct path* stack_pop(struct stack* stack) {
    assert(stack->top >= stack->bottom);

    stack->top--;
    return *stack->top;
}

static void parse_demand(char* demand, struct node* node_data, unsigned int* start_ptr, unsigned int* end_ptr, unsigned int* via_nodes_count_ptr) {
    unsigned int start = 0, end = 0, index = 0, via_nodes_count = 0;
    char* p = demand;

    do {
        start = start * 10 + *p - '0';
        p++;
    } while (*p != ',');
    *start_ptr = start;
    p++;

    do {
        end = end * 10 + *p - '0';
        p++;
    } while (*p != ',');
    *end_ptr = end;
    p++;

    do {
        index = index * 10 + *p - '0';
        p++;
        if (*p == '|') {
            p++;
            via_nodes_count++;
            node_data[index].is_via_node = true;
            index = 0;
            continue;
        }
    } while (*p != 0);
    node_data[index].is_via_node = true;
    *via_nodes_count_ptr = via_nodes_count + 1;
}

static void parse_nodes(unsigned int edge_num, struct path* paths) {
    struct path* path;
    struct node* node;
    unsigned int index;

    for (index = 0; index < edge_num; index++) {
        path = &paths[index];
        node = path->from;
        node->outward_paths[node->outward_paths_size] = path;
        node->outward_paths_size++;
    }
}

static void parse_paths(char* topo[], unsigned int edge_num, struct path* paths, struct node* nodes) {
    unsigned short index;
    unsigned int from, to;
    unsigned char weight;
    struct path* path;
    char* p;
    for (index = 0; index < edge_num; index++) {
        path = &paths[index];
        p = topo[index];

        do {
            p++;
        } while (*p != ',');
        p++;
        path->index = index;

        from = 0;
        do {
            from *= 10;
            from += *p - '0';
            p++;
        } while (*p != ',');
        p++;
        path->from = &nodes[from];

        to = 0;
        do {
            to *= 10;
            to += *p - '0';
            p++;
        } while (*p != ',');
        p++;
        path->to = &nodes[to];

        weight = 0;
        do {
            weight *= 10;
            weight += *p - '0';
            p++;
        } while (*p != 0 && *p != '\n');
        path->weight = weight;
    }
}

static void output_result(struct stack stack) {
    struct path** path;
    for (path = stack.bottom; path < stack.top; path++) {
        record_result((*path)->index);
    }
}

static void perform_actual_search(unsigned int edge_num, struct node* start, struct node* end, unsigned int via_nodes_count) {
    struct stack stack, previous_stack;
    unsigned int weight = 0, previous_weight = UINT_MAX, via_nodes_visited_count = 0;
    struct node* current_node;
    struct node* next_node;
    struct path* current_path;

    stack_init(&stack, edge_num);
    stack_init(&previous_stack, edge_num);

    current_node = start;
    current_node->has_been_visited = true;
    current_node->current_outward_path_index = 0;
    if (current_node->is_via_node) {
        via_nodes_visited_count++;
    }
    for (; ;) {
        assert(current_node->current_outward_path_index <= current_node->outward_paths_size);

        if (current_node == end) {
            assert(via_nodes_visited_count <= via_nodes_count);
            if (via_nodes_visited_count == via_nodes_count && weight < previous_weight) {
                stack_copy(&previous_stack, &stack);
                previous_weight = weight;
            }
        }

        if (current_node == end || current_node->current_outward_path_index == current_node->outward_paths_size) {
            if (current_node == start) {
                break;
            }

            current_path = stack_pop(&stack);

            if (current_node->is_via_node) {
                via_nodes_visited_count--;
            }
            current_node->has_been_visited = false;
            weight -= current_path->weight;

            current_node = current_path->from;
        } else {
            current_path = current_node->outward_paths[current_node->current_outward_path_index];
            next_node = current_path->to;
            current_node->current_outward_path_index++;
            if (!next_node->has_been_visited) {
                weight += current_path->weight;
                stack_push(&stack, current_path);
                current_node = next_node;
                current_node->has_been_visited = true;
                current_node->current_outward_path_index = 0;
                if (current_node->is_via_node) {
                    via_nodes_visited_count++;
                }
            }
        }
    }

    output_result(previous_stack);

    stack_free(&stack);
    stack_free(&previous_stack);
}

/**
 * topo: An array of strings, the maximum length is 5000
 * edge_num: The number of elements in topo
 * demand: The demand
 */
void search_route(char* topo[5000], int edge_num, char* demand) {
    struct path* paths;
    struct node nodes[MAX_NODE_NUMBERS] = {};
    unsigned int start_index, end_index, via_nodes_count;

    paths = (struct path*) malloc(edge_num * sizeof(struct path));
    parse_paths(topo, (unsigned int) edge_num, paths, nodes);
    parse_nodes((unsigned int) edge_num, paths);
    parse_demand(demand, nodes, &start_index, &end_index, &via_nodes_count);

    perform_actual_search((unsigned int) edge_num, &nodes[start_index], &nodes[end_index], via_nodes_count);

    free(paths);
}
