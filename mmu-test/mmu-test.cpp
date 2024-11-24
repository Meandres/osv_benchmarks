#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <osv/pagealloc.hh>

void print_sample(void *object) {
  unsigned char *sample = static_cast<unsigned char *>(object);
  std::cout << "Sample from memory: ";
  for (int j = 0; j < 16; ++j) { // Print the first 16 bytes
    std::cout << std::hex << static_cast<int>(sample[j]) << " ";
  }
  std::cout << std::dec << "\n"; // Reset to decimal formatting
}

int get_mem_type(void *object) {
  return reinterpret_cast<uintptr_t>(object) >> 44 & 3;
  // main == 0
  // page == 1
  // mempool == 2
}

void print_memories(void *object) {
  const std::string nameMem[] = {"main", "page", "mempool"};
  auto current = get_mem_type(object);
  std::cout << "current address type: " << nameMem[current] << std::endl;
  std::cout << "in main(0)   : " << memory::translate_mem(current, 0, object)
            << std::endl;
  std::cout << "in page(1)   : " << memory::translate_mem(current, 1, object)
            << std::endl;
  std::cout << "in mempool(2): " << memory::translate_mem(current, 2, object)
            << std::endl;
  std::cout << "----------------------------------\n";
}

int main(int argc, char *argv[]) {
  size_t allocations = 50000;
  size_t iterations = 10;
  size_t threads = 1;
  std::ostream *output = &std::cout; // Default to stdout
  std::ofstream fileOutput;
  bool machineReadable = false;

  void *phys = memory::physically_alloc_page();
  void *phys2 = memory::physically_alloc_page();
  void *mal = malloc(4096);
  void *mal2 = malloc(4096);
  print_memories(phys);
  print_memories(phys2);
  print_memories(mal);
  print_memories(mal2);

  return 0;
}
