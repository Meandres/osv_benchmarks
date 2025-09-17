#ifndef BFS_BFS_KERNEL_HPP
#define BFS_BFS_KERNEL_HPP

#include <iostream>
#include <limits>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <pthread.h>
#include <unistd.h>

#include "utility/bitmap.hpp"

namespace bfs {

static const uint16_t k_infinite_level = std::numeric_limits<uint16_t>::max();

void init_bfs(const size_t num_vertices, uint16_t *const level, uint64_t *visited_filter) {
    for (size_t i = 0; i < num_vertices; ++i)
        level[i] = k_infinite_level;
    for (size_t i = 0; i < utility::bitmap_size(num_vertices); ++i)
        visited_filter[i] = 0;
}

namespace {
struct ThreadData {
    const uint64_t* index;
    const uint64_t* edges;
    uint16_t* level;
    uint64_t* visited_filter;
    uint16_t current_level;
    const std::vector<uint64_t>* frontier;
    std::vector<uint64_t>* next_frontier;
    pthread_mutex_t* next_frontier_mutex;
    size_t start_index;
    size_t end_index;
};

void* bfs_level_worker(void* arg) {
    ThreadData* data = static_cast<ThreadData*>(arg);
    std::vector<uint64_t> local_next_frontier;

    for (size_t i = data->start_index; i < data->end_index; ++i) {
        const uint64_t src = (*data->frontier)[i];
        for (size_t j = data->index[src]; j < data->index[src + 1]; ++j) {
            const uint64_t trg = data->edges[j];
            if (__sync_val_compare_and_swap(&data->level[trg], k_infinite_level, data->current_level + 1) == k_infinite_level) {
                utility::set_bit(data->visited_filter, trg);
                local_next_frontier.push_back(trg);
            }
        }
    }
    
    if (!local_next_frontier.empty()) {
        pthread_mutex_lock(data->next_frontier_mutex);
        data->next_frontier->insert(data->next_frontier->end(), local_next_frontier.begin(), local_next_frontier.end());
        pthread_mutex_unlock(data->next_frontier_mutex);
    }
    return nullptr;
}
}

uint16_t run_bfs(const size_t num_vertices,
                   const uint64_t *const index,
                   const uint64_t *const edges,
                   uint16_t *const level,
                   uint64_t *visited_filter,
                   const uint64_t root) {

    const char* num_threads_str = std::getenv("OMP_NUM_THREADS");
    const int num_threads = num_threads_str ? std::atoi(num_threads_str) : sysconf(_SC_NPROCESSORS_ONLN);
    std::cout << "Run with " << num_threads << " threads" << std::endl;

    pthread_t threads[num_threads];
    ThreadData thread_data[num_threads];
    pthread_mutex_t next_frontier_mutex;
    pthread_mutex_init(&next_frontier_mutex, nullptr);

    std::vector<uint64_t> frontier, next_frontier;
    
    level[root] = 0;
    utility::set_bit(visited_filter, root);
    frontier.push_back(root);

    uint16_t current_level = 0;
    while (!frontier.empty()) {
        const size_t frontier_size = frontier.size();
        const size_t chunk_size = (frontier_size + num_threads - 1) / num_threads;
        next_frontier.clear();

        for (int i = 0; i < num_threads; ++i) {
            size_t start = i * chunk_size;
            size_t end = std::min(start + chunk_size, frontier_size);
            if (start >= end) continue;
            thread_data[i] = {index, edges, level, visited_filter, current_level, &frontier, &next_frontier, &next_frontier_mutex, start, end};
            pthread_create(&threads[i], nullptr, bfs_level_worker, &thread_data[i]);
        }

        for (int i = 0; i < num_threads; ++i) {
            if (i * chunk_size < frontier_size) {
                 pthread_join(threads[i], nullptr);
            }
        }
        
        if (next_frontier.empty()) {
            break;
        }

        frontier.swap(next_frontier);
        ++current_level;
        printf("current_level = %u \n", current_level);
    }
    
    pthread_mutex_destroy(&next_frontier_mutex);
    return current_level;
}

}
#endif //BFS_BFS_KERNEL_HPP
