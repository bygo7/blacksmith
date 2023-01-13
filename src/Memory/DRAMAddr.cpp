#include "Memory/DRAMAddr.hpp"
#include "GlobalDefines.hpp"

// initialize static variable
std::map<size_t, MemConfiguration> DRAMAddr::Configs;

void DRAMAddr::initialize(uint64_t num_bank_rank_functions, volatile char *start_address) {
  // TODO: This is a shortcut to check if it's a single rank dimm or dual rank in order to load the right memory
  //  configuration. We should get these infos from dmidecode to do it properly, but for now this is easier.
  size_t num_ranks;
  if (num_bank_rank_functions==5) {
    num_ranks = RANKS(2);
  } else if (num_bank_rank_functions==4) {
    num_ranks = RANKS(1);
  } else {
    Logger::log_error("Could not initialize DRAMAddr as #ranks seems not to be 1 or 2.");
    exit(1);
  }
  DRAMAddr::load_mem_config((CHANS(CHANNEL) | DIMMS(DIMM) | num_ranks | BANKS(NUM_BANKS)));
  DRAMAddr::set_base_msb((void *) start_address);
}

void DRAMAddr::set_base_msb(void *buff) {
  base_msb = (size_t) buff & (~((size_t) (1ULL << 36UL) - 1UL));  // get higher order bits above the super page
}

// TODO we can create a DRAMconfig class to load the right matrix depending on
// the configuration. You could also test it by checking if you can trigger bank conflcits
void DRAMAddr::load_mem_config(mem_config_t cfg) {
  DRAMAddr::initialize_configs();
  MemConfig = Configs[cfg];
}

DRAMAddr::DRAMAddr() = default;

DRAMAddr::DRAMAddr(size_t bk, size_t r, size_t c) {
  bank = bk;
  row = r;
  col = c;
}

DRAMAddr::DRAMAddr(void *addr) {
  auto p = (size_t) addr;
  size_t res = 0;
  for (unsigned long i : MemConfig.DRAM_MTX) {
    res <<= 1ULL;
    res |= (size_t) __builtin_parityll(p & i);
  }
  bank = (res >> MemConfig.BK_SHIFT) & MemConfig.BK_MASK;
  row = (res >> MemConfig.ROW_SHIFT) & MemConfig.ROW_MASK;
  col = (res >> MemConfig.COL_SHIFT) & MemConfig.COL_MASK;
}

int DRAMAddr::get_bank(void *addr) {
  auto p = (size_t) addr;
  size_t res = 0;
  for (unsigned long i : MemConfig.DRAM_MTX) {
    res <<= 1ULL;
    res |= (size_t) __builtin_parityll(p & i);
  }
  int bank = (res >> MemConfig.BK_SHIFT) & MemConfig.BK_MASK;
  return bank;
}

size_t DRAMAddr::linearize() const {
  return (this->bank << MemConfig.BK_SHIFT) | (this->row << MemConfig.ROW_SHIFT) | (this->col << MemConfig.COL_SHIFT);
}

void *DRAMAddr::to_virt() {
  return const_cast<const DRAMAddr *>(this)->to_virt();
}

void *DRAMAddr::to_virt() const {
  size_t res = 0;
  size_t l = this->linearize();
  for (unsigned long i : MemConfig.ADDR_MTX) {
    res <<= 1ULL;
    res |= (size_t) __builtin_parityll(l & i);
  }
  void *v_addr = (void *) (base_msb | res);
  return v_addr;
}

std::string DRAMAddr::to_string() {
  char buff[1024];
  sprintf(buff, "DRAMAddr(b: %zu, r: %zu, c: %zu) = %p",
      this->bank,
      this->row,
      this->col,
      this->to_virt());
  return std::string(buff);
}

std::string DRAMAddr::to_string_compact() const {
  char buff[1024];
  sprintf(buff, "(%ld,%ld,%ld)",
      this->bank,
      this->row,
      this->col);
  return std::string(buff);
}

DRAMAddr DRAMAddr::add(size_t bank_increment, size_t row_increment, size_t column_increment) const {
  return {bank + bank_increment, row + row_increment, col + column_increment};
}

void DRAMAddr::add_inplace(size_t bank_increment, size_t row_increment, size_t column_increment) {
  bank += bank_increment;
  row += row_increment;
  col += column_increment;
}

// Define the static DRAM configs
MemConfiguration DRAMAddr::MemConfig;
size_t DRAMAddr::base_msb;

#ifdef ENABLE_JSON

nlohmann::json DRAMAddr::get_memcfg_json() {
  std::map<size_t, nlohmann::json> memcfg_to_json = {
      {(CHANS(1UL) | DIMMS(1UL) | RANKS(1UL) | BANKS(16UL)),
       nlohmann::json{
           {"channels", 1},
           {"dimms", 1},
           {"ranks", 1},
           {"banks", 16}}},
      {(CHANS(1UL) | DIMMS(1UL) | RANKS(2UL) | BANKS(16UL)),
       nlohmann::json{
           {"channels", 1},
           {"dimms", 1},
           {"ranks", 2},
           {"banks", 16}}}
  };
  return memcfg_to_json[MemConfig.IDENTIFIER];
}

#endif

void DRAMAddr::initialize_configs() {
  struct MemConfiguration single_rank = {
      .IDENTIFIER = (CHANS(1UL) | DIMMS(1UL) | RANKS(1UL) | BANKS(64UL)),
      
      .BK_SHIFT = 28,
      .BK_MASK = 0b111111,
      .ROW_SHIFT = 0,
      .ROW_MASK = 0b1111111111111111,
      .COL_SHIFT = 16,
      .COL_MASK = 0b111111111111,

      /* maps a virtual addr -> DRAM addr: bank (4 bits) | col (13 bits) | row (13 bits) */
      .DRAM_MTX =  {
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
          },
      /* maps a DRAM addr (bank | col | row) --> virtual addr */
      .ADDR_MTX =  {
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
    }
  };
  // struct MemConfiguration dual_rank = {
  //     .IDENTIFIER = (CHANS(1UL) | DIMMS(1UL) | RANKS(2UL) | BANKS(16UL)),
  //     .BK_SHIFT =  25,
  //     .BK_MASK =  (0b11111),
  //     .ROW_SHIFT =  0,
  //     .ROW_MASK =  (0b111111111111),
  //     .COL_SHIFT =  12,
  //     .COL_MASK =  (0b1111111111111),
  //     .DRAM_MTX =  {
  //         0b000000000000000010000001000000,
  //         0b000000000001000100000000000000,
  //         0b000000000010001000000000000000,
  //         0b000000000100010000000000000000,
  //         0b000000001000100000000000000000,
  //         0b000000000000000010000000000000,
  //         0b000000000000000001000000000000,
  //         0b000000000000000000100000000000,
  //         0b000000000000000000010000000000,
  //         0b000000000000000000001000000000,
  //         0b000000000000000000000100000000,
  //         0b000000000000000000000010000000,
  //         0b000000000000000000000000100000,
  //         0b000000000000000000000000010000,
  //         0b000000000000000000000000001000,
  //         0b000000000000000000000000000100,
  //         0b000000000000000000000000000010,
  //         0b000000000000000000000000000001,
  //         0b100000000000000000000000000000,
  //         0b010000000000000000000000000000,
  //         0b001000000000000000000000000000,
  //         0b000100000000000000000000000000,
  //         0b000010000000000000000000000000,
  //         0b000001000000000000000000000000,
  //         0b000000100000000000000000000000,
  //         0b000000010000000000000000000000,
  //         0b000000001000000000000000000000,
  //         0b000000000100000000000000000000,
  //         0b000000000010000000000000000000,
  //         0b000000000001000000000000000000
  //     },
  //     .ADDR_MTX =  {
  //         0b000000000000000000100000000000,
  //         0b000000000000000000010000000000,
  //         0b000000000000000000001000000000,
  //         0b000000000000000000000100000000,
  //         0b000000000000000000000010000000,
  //         0b000000000000000000000001000000,
  //         0b000000000000000000000000100000,
  //         0b000000000000000000000000010000,
  //         0b000000000000000000000000001000,
  //         0b000000000000000000000000000100,
  //         0b000000000000000000000000000010,
  //         0b000000000000000000000000000001,
  //         0b000010000000000000000000001000,
  //         0b000100000000000000000000000100,
  //         0b001000000000000000000000000010,
  //         0b010000000000000000000000000001,
  //         0b000001000000000000000000000000,
  //         0b000000100000000000000000000000,
  //         0b000000010000000000000000000000,
  //         0b000000001000000000000000000000,
  //         0b000000000100000000000000000000,
  //         0b000000000010000000000000000000,
  //         0b000000000001000000000000000000,
  //         0b100001000000000000000000000000,
  //         0b000000000000100000000000000000,
  //         0b000000000000010000000000000000,
  //         0b000000000000001000000000000000,
  //         0b000000000000000100000000000000,
  //         0b000000000000000010000000000000,
  //         0b000000000000000001000000000000
  //     }
  // };
  DRAMAddr::Configs = {
      {(CHANS(1UL) | DIMMS(1UL) | RANKS(1UL) | BANKS(16UL)), single_rank},
      // {(CHANS(1UL) | DIMMS(1UL) | RANKS(2UL) | BANKS(16UL)), dual_rank}
  };
}

#ifdef ENABLE_JSON

void to_json(nlohmann::json &j, const DRAMAddr &p) {
  j = {{"bank", p.bank},
       {"row", p.row},
       {"col", p.col}
  };
}

void from_json(const nlohmann::json &j, DRAMAddr &p) {
  j.at("bank").get_to(p.bank);
  j.at("row").get_to(p.row);
  j.at("col").get_to(p.col);
}

#endif
