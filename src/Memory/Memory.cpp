#include "Memory/Memory.hpp"

#include <sys/mman.h>
#include <sys/stat.h> 
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <utility>
#include <string.h>

#define CL_SIZE 64
#define NOT_OPENED -1

#define BK_SHIFT  28
#define BK_MASK  (0b111111)
#define ROW_SHIFT  0
#define ROW_MASK  (0b1111111111111111)
#define COL_SHIFT  16
#define COL_MASK  (0b111111111111)
int pmap_fd;
size_t DRAM_MTX[34]  =  {
// # 0b9876543210987654321098765432109876543210,
    0b0000000000000010000001000100000000,
    0b0000000000000011000011001000000000,
    0b0001000100010000100000000000000000, 
    0b0010001000100001000100000000000000, 
    0b0100010001000100001000000000000000, 
    0b1000100010001000010000000000000000, 
    0b0000000000000000000010000000000000, 
    0b0000000000000000000001000000000000, 
    0b0000000000000000000000100000000000, 
    0b0000000000000000000000010000000000, 
    0b0000000000000000000000000010000000, 
    0b0000000000000000000000000001000000, 
    0b0000000000000000000000000000100000, 
    0b0000000000000000000000000000010000, 
    0b0000000000000000000000000000001000, 
    0b0000000000000000000000000000000100, 
    0b0000000000000000000000000000000010, 
    0b0000000000000000000000000000000001, 
    0b1000000000000000000000000000000000, 
    0b0100000000000000000000000000000000, 
    0b0010000000000000000000000000000000, 
    0b0001000000000000000000000000000000, 
    0b0000100000000000000000000000000000, 
    0b0000010000000000000000000000000000, 
    0b0000001000000000000000000000000000, 
    0b0000000100000000000000000000000000, 
    0b0000000010000000000000000000000000, 
    0b0000000001000000000000000000000000, 
    0b0000000000100000000000000000000000, 
    0b0000000000010000000000000000000000, 
    0b0000000000001000000000000000000000, 
    0b0000000000001100000000000000000000, 
    0b0000000000001010000000000000000000, 
    0b0000000000000001000000000000000000,
    };
/* maps a DRAM addr (bank | col | row) --> virtual addr */
size_t ADDR_MTX[34] =  {
//             #     BBBBBBCCCCCCCCCCCCRRRRRRRRRRRRRRRR | B6 C12 R16
// # 0b9876543210987654321098765432109876543210,
  0b0000000000000000001000000000000000,
  0b0000000000000000000100000000000000,
  0b0000000000000000000010000000000000,
  0b0000000000000000000001000000000000,
  0b0000000000000000000000100000000000,
  0b0000000000000000000000010000000000,
  0b0000000000000000000000001000000000,
  0b0000000000000000000000000100000000,
  0b0000000000000000000000000010000000,
  0b0000000000000000000000000001000000,
  0b0000000000000000000000000000100000,
  0b0000000000000000000000000000010000,
  0b0000000000000000000000000000001000,
  0b0000000000000000000000000000001100,
  0b0000000000000000000000000000001010,
  0b0000000000000000000000000000000001,
  0b0010000000000000000001000100010000, 
  0b0000010000000000001000100010001000, 
  0b0000100000000000000100010001001100, 
  0b0001000000000000000010001000100001, 
  0b0000001000000000000000000000000000,
  0b0000000100000000000000000000000000,
  0b0000000010000000000000000000000000,
  0b0000000001000000000000000000000000,
  0b0100001100000000000000000000001011, 
  0b1000000100000000000000000000001010, 
  0b0000000000100000000000000000000000,
  0b0000000000010000000000000000000000,
  0b0000000000001000000000000000000000,
  0b0000000000000100000000000000000000,
  0b0000000000000010000000000000000000,
  0b0000000000000001000000000000000000,
  0b0000000000000000100000000000000000,
  0b0000000000000000010000000000000000,
};
// int phys_cmp(const void *p1, const void *p2)
// {
// 	return ((pte_t *) p1)->p_addr - ((pte_t *) p2)->p_addr;
// }

std::string phys_2_dram(physaddr_t addr) {
  auto p = addr;
  uint64_t res = 0;
  for (uint64_t i : DRAM_MTX) {
    res = res << 1ULL;
    res |= (uint64_t) __builtin_parityll(p & i);
  }
  int bank = (res >> BK_SHIFT) & BK_MASK;
  int row = (res >> ROW_SHIFT) & ROW_MASK;
  int col = (res >> COL_SHIFT) & COL_MASK;
  std::string dram_addr;
  dram_addr += std::to_string(bank);
  dram_addr += "_";
  dram_addr += std::to_string(row);
  dram_addr += "_";
  dram_addr += std::to_string(col);
  return dram_addr;
}

physaddr_t dram_2_phys(std::string dram_addr) {
  std::string bank, row, col;
  std::istringstream f(dram_addr);
  getline(f, bank, '_');
  getline(f, row, '_');
  getline(f, col, '_');
  unsigned long long l = 0;
  l = (stoll(bank) << BK_SHIFT);
  l |= (stoll(row) << ROW_SHIFT); 
  l |= (stoll(col) << COL_SHIFT);
  unsigned long long res = 0;
  for (uint64_t i : ADDR_MTX) {
    res <<= 1ULL;
    res |= (uint64_t) __builtin_parityll(l & i);
  }
  return res;
}

std::string modify_row_daddr(std::string dram_addr, int val){
  std::string bank, row, col;
  std::istringstream f(dram_addr);
  getline(f, bank, '_');
  getline(f, row, '_');
  getline(f, col, '_');
  unsigned long long row_val = stoll(row) + val;
  if (row_val < 0 || row_val > ROW_MASK){
    fprintf(stderr, "modify_row_daddr:: GOT PHERIPERAL DRAM_ADDR, %s - %lx. PICK ANOTHER\n", dram_addr.c_str(), dram_2_phys(dram_addr));
    return NULL;
  }
  std::string modified_dram_addr;
  modified_dram_addr += (bank);
  modified_dram_addr += "_";
  modified_dram_addr += std::to_string(row_val);
  modified_dram_addr += "_";
  modified_dram_addr += (col);
  return modified_dram_addr;
}

uint64_t get_pfn(uint64_t entry)
{
	return ((entry) & 0x7fffffffffffffff);
}

physaddr_t virt_2_phys(uint64_t v_addr)
{
  // fprintf(stderr, "virt_2_phys:: got %lx\n", v_addr);
	uint64_t entry;
	uint64_t offset = (v_addr / PAGE_SIZE) * sizeof(entry);
	uint64_t pfn;
	bool to_open = false;
	// assert(fd >= 0);
	if (pmap_fd == NOT_OPENED) {
		pmap_fd = open("/proc/self/pagemap", O_RDONLY);
		assert(pmap_fd >= 0);
		to_open = true;
	}
	// int rd = fread(&entry, sizeof(entry), 1 ,fp);
	int bytes_read = pread(pmap_fd, &entry, sizeof(entry), offset);

	assert(bytes_read == 8);
	assert(entry & (1ULL << 63));

	if (to_open) {
		close(pmap_fd);
	}

	pfn = get_pfn(entry);
	assert(pfn != 0);
	return (pfn << 12) | (v_addr & 4095);
}

char *phys_2_virt(physaddr_t p_addr)
{
	physaddr_t p_page = p_addr & ~(((uint64_t) PAGE_SIZE - 1));
  // fprintf(stderr, "\nphys_2_virt:: p_page:%lx\t", p_page);
	// pte_t src_pte = {.v_addr = 0, .p_addr = p_page};
	// pte_t *res_pte =
	    // (pte_t *) bsearch(&src_pte, g_physmap, mem_size / PAGE_SIZE,
			//       sizeof(pte_t), phys_cmp);
  // fprintf(stderr, ", v_page:%p\t", res_pte->v_addr);
  // fprintf(stderr, ", map_v_page:%p\n", ppage_2_vpage[p_page]);
	if (ppage_2_vpage.find(p_page) == ppage_2_vpage.end()){
    // fprintf(stderr, "phys_2_virt:: p_page not present in hashmap, %lx\n", p_page);
		return (char *) 0xfffffffff;
  }

	return (char *)((uint64_t) ppage_2_vpage[p_page] | 
  ((uint64_t) p_addr & (((uint64_t) PAGE_SIZE - 1))));
}



/// Allocates a MEM_SIZE bytes of memory by using super or huge pages.
void Memory::allocate_memory(size_t mem_size) {
  this->size = mem_size;
  volatile char *target = nullptr;
  FILE *fp;

  if (superpage) {
    // allocate memory using super pages
    fp = fopen(hugetlbfs_mountpoint.c_str(), "w+");
    if (fp==nullptr) {
      Logger::log_info(format_string("Could not mount superpage from %s. Error:", hugetlbfs_mountpoint.c_str()));
      Logger::log_data(std::strerror(errno));
      exit(EXIT_FAILURE);
    }
    auto mapped_target = mmap((void *) start_address, MEM_SIZE, PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB | (30UL << MAP_HUGE_SHIFT), fileno(fp), 0);
    if (mapped_target==MAP_FAILED) {
      perror("mmap");
      exit(EXIT_FAILURE);
    }
    target = (volatile char*) mapped_target;
  } else {
    // allocate memory using huge pages
    assert(posix_memalign((void **) &target, MEM_SIZE, MEM_SIZE)==0);
    assert(madvise((void *) target, MEM_SIZE, MADV_HUGEPAGE)==0);
    memset((char *) target, 'A', MEM_SIZE);
    // for khugepaged
    Logger::log_info("Waiting for khugepaged.");
    sleep(10);
  }

  if (target!=start_address) {
    Logger::log_error(format_string("Could not create mmap area at address %p, instead using %p.",
        start_address, target));
    start_address = target;
  }

  // initialize memory with random but reproducible sequence of numbers
  // initialize(DATA_PATTERN::RANDOM);
  initialize(DATA_PATTERN::ONES);
  pmap_fd = open(PAGEMAP, O_CREAT | O_RDWR, 0755);
  assert(pmap_fd != -1);
  set_physmap(pmap_fd);
}

int Memory::check_victim_bank(int bank){
  // checking whole bank if possible
  uint8_t data;
  data = 0xff;

  int flip = 0;
  // if (val == 0)
  //   data = 0x00;
  // else
  //   data = 0xff;
  // fprintf(stderr, "check_victim_bank:: bank:%d, val:%u\n", bank, data);
  for(int cur_row = 0; cur_row < ROW_MASK; cur_row ++){
    for(int cur_col = 0; cur_col < COL_MASK; cur_col += CL_SIZE){
      std::string cur_dram_addr;
      cur_dram_addr += std::to_string(bank);
      cur_dram_addr += "_";
      cur_dram_addr += std::to_string(cur_row);
      cur_dram_addr += "_";
      cur_dram_addr += std::to_string(cur_col);
      
      char * cur_virt_addr = phys_2_virt(dram_2_phys(cur_dram_addr));
      if ((pointer) cur_virt_addr == 0xfffffffff){
        continue;
      }
      // fprintf(stderr, "check_victim_bank:: d:%s, p:%lx, v:%lx, val:%u\n", cur_dram_addr.c_str(), dram_2_phys(cur_dram_addr),(pointer)cur_virt_addr, data);
      asm volatile("clflush (%0)" : : "r" (cur_virt_addr) : "memory");
      asm volatile("mfence" : : : "memory");
      if ((uint8_t)*cur_virt_addr != data){
        flip ++;
        fprintf(stderr, "check_victim_bank:: ERROR! GOTCHA %x at d:%s, p:%lx, \n", (uint8_t) *cur_virt_addr, cur_dram_addr.c_str(), dram_2_phys(cur_dram_addr));
        fprintf(stdout, "check_victim_bank:: ERROR! GOTCHA %x at d:%s, p:%lx, \n", (uint8_t) *cur_virt_addr, cur_dram_addr.c_str(), dram_2_phys(cur_dram_addr));
        *cur_virt_addr = data;
      }
    }
  }
  fflush(stdout);
  return flip;
}

void Memory::set_physmap(int pmap_fd)
{
	fprintf(stderr, "MEMORY:: SET_PHYSMAP:: ");
	int l_size = this->size / PAGE_SIZE; // 1 << 30 / 1 << 11
	// pte_t *physmap = (pte_t *) malloc(sizeof(pte_t) * l_size);
	assert(pmap_fd >= 0);
	
	for (uint64_t tmp = (uint64_t) start_address, idx = 0;
	     tmp < (uint64_t) start_address + this->size; tmp += PAGE_SIZE) {
    // fprintf(stdout, "%lx\t%lx\n", tmp, virt_2_phys(tmp));
    
    ppage_2_vpage[virt_2_phys(tmp)] = (char *)tmp;

		if(idx == 0 || (tmp == ((uint64_t) start_address + this->size - PAGE_SIZE)))
			fprintf(stderr, "phys: %016lx\n", virt_2_phys(tmp));
		idx++;
	}

	// qsort(physmap, mem_size / PAGE_SIZE, sizeof(pte_t), phys_cmp);
	// g_physmap = physmap;
  // fflush(stdout);
}

void Memory::initialize(DATA_PATTERN data_pattern) {
  Logger::log_info("Initializing memory with pseudorandom sequence.");

  // for each page in the address space [start, end]
  for (uint64_t cur_page = 0; cur_page < size; cur_page += getpagesize()) {
    // reseed rand to have a sequence of reproducible numbers, using this we can compare the initialized values with
    // those after hammering to see whether bit flips occurred
    srand(static_cast<unsigned int>(cur_page*getpagesize()));
    for (uint64_t cur_pageoffset = 0; cur_pageoffset < (uint64_t) getpagesize(); cur_pageoffset += sizeof(int)) {

      int fill_value = 0;
      if (data_pattern == DATA_PATTERN::RANDOM) {
        fill_value = rand();
      } else if (data_pattern == DATA_PATTERN::ZEROES) {
        fill_value = 0;
      } else if (data_pattern == DATA_PATTERN::ONES) {
        fill_value = ~(int) 0;
      } else {
        Logger::log_error("Could not initialize memory with given (unknown) DATA_PATTERN.");
      }
        
      // write (pseudo)random 4 bytes
      uint64_t offset = cur_page + cur_pageoffset;
      *((int *) (start_address + offset)) = fill_value;
    }
  }
}

size_t Memory::check_memory(PatternAddressMapper &mapping, bool reproducibility_mode, bool verbose) {
  flipped_bits.clear();

  auto victim_rows = mapping.get_victim_rows();
  if (verbose) Logger::log_info(format_string("Checking %zu victims for bit flips.", victim_rows.size()));

  size_t sum_found_bitflips = 0;
  for (const auto &victim_row : victim_rows) {

    // auto victim_dram_addr = DRAMAddr((char*)victim_row);
    // victim_dram_addr.add_inplace(0, 1, 0);
    // sum_found_bitflips += check_memory_internal(mapping, victim_row,
    //     (volatile char *) victim_dram_addr.to_virt(), reproducibility_mode, verbose);
    int victim_bank = DRAMAddr::get_bank((char*)victim_row);

    sum_found_bitflips += check_victim_bank(victim_bank);

  }
  return sum_found_bitflips;
}

size_t Memory::check_memory(const volatile char *start, const volatile char *end) {
  flipped_bits.clear();
  // create a "fake" pattern mapping to keep this method for backward compatibility
  PatternAddressMapper pattern_mapping;
  return check_memory_internal(pattern_mapping, start, end, false, true);
}

size_t Memory::check_memory_internal(PatternAddressMapper &mapping,
                                     const volatile char *start,
                                     const volatile char *end,
                                     bool reproducibility_mode,
                                     bool verbose) {
  // counter for the number of found bit flips in the memory region [start, end]
  size_t found_bitflips = 0;

  if (start==nullptr || end==nullptr || ((uint64_t) start >= (uint64_t) end)) {
    Logger::log_error("Function check_memory called with invalid arguments.");
    Logger::log_data(format_string("Start addr.: %s", DRAMAddr((void *) start).to_string().c_str()));
    Logger::log_data(format_string("End addr.: %s", DRAMAddr((void *) end).to_string().c_str()));
    return found_bitflips;
  }

  auto start_offset = (uint64_t) (start - start_address);

  const auto pagesize = static_cast<size_t>(getpagesize());
  start_offset = (start_offset/pagesize)*pagesize;

  auto end_offset = start_offset + (uint64_t) (end - start);
  end_offset = (end_offset/pagesize)*pagesize;

  void *page_raw = malloc(pagesize);
  if (page_raw == nullptr) {
    Logger::log_error("Could not create temporary page for memory comparison.");
    exit(EXIT_FAILURE);
  }
  memset(page_raw, 0, pagesize);
  int *page = (int*)page_raw;

  // for each page (4K) in the address space [start, end]
  for (uint64_t i = start_offset; i < end_offset; i += pagesize) {
    // reseed rand to have the desired sequence of reproducible numbers
    srand(static_cast<unsigned int>(i*pagesize));

    // fill comparison page with expected values generated by rand()
    for (size_t j = 0; j < (unsigned long) pagesize/sizeof(int); ++j)
      page[j] = rand();

    uint64_t addr = ((uint64_t)start_address+i);

    // check if any bit flipped in the page using the fast memcmp function, if any flip occurred we need to iterate over
    // each byte one-by-one (much slower), otherwise we just continue with the next page
    if ((addr+ pagesize) < ((uint64_t)start_address+size) && memcmp((void*)addr, (void*)page, pagesize) == 0)
      continue;

    // iterate over blocks of 4 bytes (=sizeof(int))
    for (uint64_t j = 0; j < (uint64_t) pagesize; j += sizeof(int)) {
      uint64_t offset = i + j;
      volatile char *cur_addr = start_address + offset;

      // if this address is outside the superpage we must not proceed to avoid segfault
      if ((uint64_t)cur_addr >= ((uint64_t)start_address+size))
        continue;

      // clear the cache to make sure we do not access a cached value
      clflushopt(cur_addr);
      mfence();

      // if the bit did not flip -> continue checking next block
      int expected_rand_value = page[j/sizeof(int)];
      if (*((int *) cur_addr)==expected_rand_value)
        continue;

      // if the bit flipped -> compare byte per byte
      for (unsigned long c = 0; c < sizeof(int); c++) {
        volatile char *flipped_address = cur_addr + c;
        if (*flipped_address != ((char *) &expected_rand_value)[c]) {
          const auto flipped_addr_dram = DRAMAddr((void *) flipped_address);
          assert(flipped_address == (volatile char*)flipped_addr_dram.to_virt());
          const auto flipped_addr_value = *(unsigned char *) flipped_address;
          const auto expected_value = ((unsigned char *) &expected_rand_value)[c];
          if (verbose) {
            Logger::log_bitflip(flipped_address, flipped_addr_dram.row,
                expected_value, flipped_addr_value, (size_t) time(nullptr), true);
          }
          // store detailed information about the bit flip
          BitFlip bitflip(flipped_addr_dram, (expected_value ^ flipped_addr_value), flipped_addr_value);
          // ..in the mapping that triggered this bit flip
          if (!reproducibility_mode) {
            if (mapping.bit_flips.empty()) {
              Logger::log_error("Cannot store bit flips found in given address mapping.\n"
                                "You need to create an empty vector in PatternAddressMapper::bit_flips before calling "
                                "check_memory.");
            }
            mapping.bit_flips.back().push_back(bitflip);
          }
          // ..in an attribute of this class so that it can be retrived by the caller
          flipped_bits.push_back(bitflip);
          found_bitflips += bitflip.count_bit_corruptions();
        }
      }

      // restore original (unflipped) value
      *((int *) cur_addr) = expected_rand_value;

      // flush this address so that value is committed before hammering again there
      clflushopt(cur_addr);
      mfence();
    }
  }
  
  free(page);
  return found_bitflips;
}

Memory::Memory(bool use_superpage) : size(0), superpage(use_superpage) {
}

Memory::~Memory() {
  if (munmap((void *) start_address, size)==-1) {
    Logger::log_error("munmap failed with error:");
    Logger::log_data(std::strerror(errno));
  }
}

volatile char *Memory::get_starting_address() const {
  return start_address;
}

std::string Memory::get_flipped_rows_text_repr() {
  // first extract all rows, otherwise it will not be possible to know in advance whether we we still
  // need to add a separator (comma) to the string as upcoming DRAMAddr instances might refer to the same row
  std::set<size_t> flipped_rows;
  for (const auto &da : flipped_bits) {
    flipped_rows.insert(da.address.row);
  }

  std::stringstream ss;
  for (const auto &row : flipped_rows) {
    ss << row;
    if (row!=*flipped_rows.rbegin()) ss << ",";
  }
  return ss.str();
}


