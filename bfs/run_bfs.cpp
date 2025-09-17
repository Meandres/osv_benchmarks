/*
This file is part of UMAP.  For copyright information see the COPYRIGHT
file in the top level directory, or at
https://github.com/LLNL/umap/blob/master/COPYRIGHT
This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free
Software Foundation) version 2.1 dated February 1999.  This program is
distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the terms and conditions of the GNU Lesser General Public License
for more details.  You should have received a copy of the GNU Lesser General
Public License along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
#include <unistd.h>
#include <iostream>
#include <vector>
#include <tuple>
#include <string>
#include <fstream>
#include <sstream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "bfs_kernel.hpp"
#include "utility/bitmap.hpp"
#include "utility/time.hpp"
#include "utility/map_file.hpp"

typedef uint64_t u64;
struct ioctl_req{
  u64 physSize;
  u64 batch;
  std::string filename;
  u64 virtSize;
  u64 bufsize;
  void* ret;
};

struct bfs_options {
  size_t num_vertices{0};
  size_t num_edges{0};
  std::string graph_file_name;
  std::string bfs_level_reference_file_name;
};

void usage() {
  std::cout << "BFS options:"
            << "-n\t#vertices\n"
            << "-m\t#edges\n"
            << "-g\tGraph file name\n" << "\n";
}

inline ssize_t get_file_size(const std::string &file_name) {
  std::ifstream ifs(file_name, std::ifstream::binary | std::ifstream::ate);
  ssize_t size = ifs.tellg();
  if (size == -1) {
    std::cerr << "Failed to get file size: " << file_name << "\n";
  }

  return size;
}

void parse_options(int argc, char **argv,
                   bfs_options &options) {
  int c;
  while ((c = getopt(argc, argv, "n:m:g:l:sh")) != -1) {
    switch (c) {
      case 'n': /// Required
        options.num_vertices = std::stoull(optarg);
        break;

      case 'm': /// Required
        options.num_edges = std::stoull(optarg);
        break;

      case 'g': /// Required
        options.graph_file_name = optarg;
        break;

      case 'l':
        options.bfs_level_reference_file_name = optarg;
        break;

      case 'h':
        usage();
        std::exit(0);
    }
  }
}
  
u64 envOr(const char* env, u64 value) {
  if (getenv(env))
    return atof(getenv(env));
  return value;
}

void disp_bfs_options(const bfs_options &options) {
  std::cout << "BFS options:"
            << "\n#vertices: " << options.num_vertices
            << "\n#edges: " << options.num_edges
            << "\nGraph file: " << options.graph_file_name << "\n";
}

const size_t bufsize = 4096ul;

std::pair<uint64_t *, uint64_t *> map_graph(const bfs_options &options) {

  void* map_raw_address;

  u64 size = get_file_size(options.graph_file_name);
  ioctl_req req;
  req.physSize = envOr("PHYSGB", 16ul*1024*1024*1024);
  req.batch = envOr("BATCH", 64);
  req.filename = options.graph_file_name;
  req.virtSize = size;
  req.bufsize = bufsize;
  req.ret = NULL;
  int fd = open(options.graph_file_name.c_str(), O_RDONLY);
  ioctl(fd, 0, &req);
  printf("ret: %p\n", req.ret);
  map_raw_address = req.ret;
  close(fd);
  /*map_raw_address = utility::map_file(options.graph_file_name,
                        false, false, true, //no write, read only
                        get_file_size(options.graph_file_name));*/
  if (!map_raw_address) {
    std::cerr << "Failed to ucache the graph" << std::endl;
    std::abort();
  }

  uint64_t *const index = static_cast<uint64_t *>(map_raw_address);
  const uint64_t edges_offset = options.num_vertices + 1;
  uint64_t *const edges = static_cast<uint64_t *>(map_raw_address) + edges_offset;

  return std::make_pair(index, edges);
}

uint64_t find_bfs_root(const size_t num_vertices, const uint64_t *const index, uint16_t *const level) {
  for (uint64_t src = 0; src < num_vertices; ++src) {
    const size_t degree = index[src + 1] - index[src];
    if (degree > 0) {
      level[src] = 0;
      std::cout << "BFS root: " << src << std::endl;
      return src;
    }
  }
  std::cerr << "Can not find a proper root vertex; all vertices do not have any edges?" << std::endl;
  std::abort();
}

void count_level(const size_t num_vertices, const uint16_t max_level, const uint16_t *const level) {

  std::vector<size_t> cnt(max_level + 1, 0);
  for (uint64_t i = 0; i < num_vertices; ++i) {
    if (level[i] == bfs::k_infinite_level) continue;
    if (level[i] > max_level) {
      std::cerr << "Invalid level: " << level[i] << " > " << max_level << std::endl;
      return;
    }
    ++cnt[level[i]];
  }

  std::cout << "Level\t#vertices" << std::endl;
  for (uint16_t i = 0; i <= max_level; ++i) {
    std::cout << i << "\t" << cnt[i] << std::endl;
  }
}

void validate_level(const std::vector<uint16_t>& level, const std::string& bfs_level_reference_file_name) {

  std::ifstream ifs(bfs_level_reference_file_name);
  if (!ifs.is_open()) {
    std::cerr << "Can not open: "<< bfs_level_reference_file_name << std::endl;
    std::abort();
  }

  std::vector<uint16_t> ref_level(level.size(), bfs::k_infinite_level);
  uint64_t id;
  uint16_t lv;
  while (ifs >> id >> lv) {
    ref_level[id] = lv;
  }

  if (level != ref_level) {
    std::cerr << "BFS level is wrong" << std::endl;
    std::abort();
  }

}

int main(int argc, char **argv) {
  bfs_options options;

  parse_options(argc, argv, options);
  disp_bfs_options(options);

  const uint64_t *index = nullptr;
  const uint64_t *edges = nullptr;
  std::tie(index, edges) = map_graph(options);

  // Array to store each vertex's level (a distance from the source vertex)
  std::vector<uint16_t> level(options.num_vertices);

  // bitmap data to store 'visited' information
  std::vector<uint64_t> visited_filter(utility::bitmap_size(options.num_vertices));

  bfs::init_bfs(options.num_vertices, level.data(), visited_filter.data());
  uint64_t root = find_bfs_root(options.num_vertices, index, level.data());

  const auto bfs_start_time = utility::elapsed_time_sec();
  const uint16_t max_level = bfs::run_bfs(options.num_vertices, index, edges, level.data(), visited_filter.data(), root);
  const auto bfs_time = utility::elapsed_time_sec(bfs_start_time);
  std::cout << "ucache," << bfs_time << std::endl;

  count_level(options.num_vertices, max_level, level.data());

  if( !options.bfs_level_reference_file_name.empty() ){
      validate_level(level, options.bfs_level_reference_file_name);
      std::cout << "Passed validation" << std::endl;
  }
  
  ioctl_req req;
  req.ret = (void*)index; 
  int fd = open(options.graph_file_name.c_str(), O_RDONLY);
  ioctl(fd, 1, &req);
  close(fd);

  return 0;
}
