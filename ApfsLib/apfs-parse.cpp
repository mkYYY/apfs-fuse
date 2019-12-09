#include <byteswap.h>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include "DiskStruct.h"

using namespace std;

void dump(uint8_t const *, char const *, size_t);
void print_fields(nx_superblock_t const *);
void print_obj_phys(obj_phys_t const *);

int main(int argc, char const* argv[])
{
  if (argc < 2)
    exit(EXIT_FAILURE);

  size_t sz_sblk = sizeof(nx_superblock_t); // 1384 == 0x568 bytes
  uint8_t const *buf = new uint8_t[sz_sblk];
  FILE *fp = fopen(argv[1], "r");

  long offset = 0;
  if (argc == 3) {
    char *endptr;
    offset = strtol(argv[2], &endptr, 0);
    if ((argv[2] == endptr) || ((fseek(fp, offset, SEEK_CUR)) != 0)) 
      exit(EXIT_FAILURE);
  }
  printf("[+] Parse superblock at offste 0x%lx\n\n", offset);

  size_t cnt = fread((void *)buf, sizeof(uint8_t), sz_sblk, fp);
  if (cnt != sz_sblk) {
    fprintf(stderr, "[-] The size of bytes read unmatches!\n");
    exit(EXIT_FAILURE);
  }
  
  // dump(buf, nullptr, sz_sblk);

  nx_superblock_t const *psblk = new nx_superblock_t;
  memcpy((void *)psblk, (void *)buf, sz_sblk);

  print_fields(psblk);

  delete[] buf;
  fclose(fp);
  return 0;
}

void dump(uint8_t const *ptr, char const *delim, size_t sz) {
  int count = 0;
  char const *d = "\\x";
  if (delim != nullptr) 
    d = delim;
  for (int i = 0; i < sz; i++) {
    if ((count != 0) && (count % 16 == 0))
      printf("\n");
    printf("%s%02x", d, ptr[i]);
    count++;
  }
}

void print_obj_phys(obj_phys_t const *ptr) {
  printf("Checksum:\t\""); dump(ptr->o_cksum, nullptr, 8); printf("\"\n");
  printf("Object id:\t0x%016lx\n", ptr->o_oid.get());
  printf("Transaction id:\t0x%016lx\n", ptr->o_xid.get());
  printf("Block type:\t0x%08x\n", ptr->o_type.get());
  printf("\tObject type:\t0x%04hx\n", (uint16_t)(ptr->o_type.get() & 0xffff));
  printf("\tObject type flags:\t0x%04hx\n", (uint16_t)(ptr->o_type.get() >> 16));
  printf("Block sub-type:\t0x%08x\n", ptr->o_subtype.get());
}

void print_fields(nx_superblock_t const *ptr) {
  print_obj_phys(&ptr->nx_o);
  printf("\n");
  uint32_t nx_cigam = bswap_32(ptr->nx_magic.get());
  printf("nx_magic:\t0x%08x == \"%.4s\"\n", ptr->nx_magic.get(), (char const *)(&nx_cigam));
  printf("nx_block_size:\t0x%08x\n", ptr->nx_block_size.get());
  printf("nx_block_count:\t0x%016lx\n", ptr->nx_block_count.get());
  printf("\n");
  printf("nx_features:\t0x%016lx\n", ptr->nx_features.get());
  printf("nx_readonly_compatible_features:\t0x%016lx\n", ptr->nx_readonly_compatible_features.get());
  printf("nx_incompatible_features:\t0x%016lx\n", ptr->nx_incompatible_features.get());
  printf("\n");
  printf("nx_uuid:\t\""); dump(ptr->nx_uuid, nullptr, 16); printf("\"\n");
  printf("\n");
  printf("nx_next_oid:\t0x%016lx\n", ptr->nx_next_oid.get());
  printf("nx_next_xid:\t0x%016lx\n", ptr->nx_next_xid.get());
  printf("\n");
  printf("nx_xp_desc_blocks:\t0x%08x\n", ptr->nx_xp_desc_blocks.get());
  printf("nx_xp_data_blocks:\t0x%08x\n", ptr->nx_xp_data_blocks.get());
  printf("nx_xp_desc_base:\t0x%016lx\n", ptr->nx_xp_desc_base.get());
  printf("nx_xp_data_base:\t0x%016lx\n", ptr->nx_xp_data_base.get());
  printf("nx_xp_desc_next:\t0x%08x\n", ptr->nx_xp_desc_next.get());
  printf("nx_xp_data_next:\t0x%08x\n", ptr->nx_xp_data_next.get());
  printf("nx_xp_desc_index:\t0x%08x\n", ptr->nx_xp_desc_index.get());
  printf("nx_xp_desc_len:\t0x%08x\n", ptr->nx_xp_desc_len.get());
  printf("nx_xp_data_index:\t0x%08x\n", ptr->nx_xp_data_index.get());
  printf("nx_xp_data_len:\t0x%08x\n", ptr->nx_xp_data_len.get());
  printf("\n");
  printf("nx_spaceman_oid:\t0x%016lx\n", ptr->nx_spaceman_oid.get());
  printf("nx_omap_oid:\t0x%016lx\n", ptr->nx_omap_oid.get());
  printf("nx_reaper_oid:\t0x%016lx\n", ptr->nx_reaper_oid.get());
  printf("\n");
  printf("nx_test_type:\t0x%08x\n", ptr->nx_test_type.get());
  printf("\n");
  printf("nx_max_file_systems:\t0x%08x\n", ptr->nx_max_file_systems.get());
  printf("... Skip nx_fs_oid (800 bytes) ...\n");
  printf("... Skip nx_counters (256 bytes) ...\n");
  printf("nx_blocked_out_prange.pr_start_addr:\t0x%016lx\n", ptr->nx_blocked_out_prange.pr_start_addr.get());
  printf("nx_blocked_out_prange.pr_block_count:\t0x%016lx\n", ptr->nx_blocked_out_prange.pr_block_count.get());
  printf("nx_evict_mapping_tree_oid:\t0x%016lx\n", ptr->nx_evict_mapping_tree_oid.get());
  printf("nx_flags:\t0x%016lx\n", ptr->nx_flags.get());
  printf("nx_efi_jumpstart:\t0x%016lx\n", ptr->nx_efi_jumpstart.get());
  printf("nx_fusion_uuid:\t\""); dump(ptr->nx_fusion_uuid, nullptr, 16); printf("\"\n");
  printf("nx_keylocker.pr_start_addr:\t0x%016lx\n", ptr->nx_keylocker.pr_start_addr.get());
  printf("nx_keylocker.pr_block_count:\t0x%016lx\n", ptr->nx_keylocker.pr_block_count.get());
  for (int i = 0; i < 4; i++)
    printf("nx_ephemeral_info[%d]:\t0x%016lx\n", i, ptr->nx_ephemeral_info[i].get());
  printf("\n");
  printf("nx_test_oid:\t0x%016lx\n", ptr->nx_test_oid.get());
  printf("\n");
  printf("nx_fusion_mt_oid:\t0x%016lx\n", ptr->nx_fusion_mt_oid.get());
  printf("nx_fusion_wbc_oid:\t0x%016lx\n", ptr->nx_fusion_wbc_oid.get());
  printf("nx_fusion_wbc.pr_start_addr:\t0x%016lx\n", ptr->nx_fusion_wbc.pr_start_addr.get());
  printf("nx_fusion_wbc.pr_block_count:\t0x%016lx\n", ptr->nx_fusion_wbc.pr_block_count.get());
  printf("\n");
}

