// #include <bits/stdint-uintn.h>
#include <cstdint>
// #include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <tuple>
#include "DiskStruct.h"

const char *default_file = "iir_vol40.pdf";
const char *search_file;

int main(int argc, char const* argv[])
{
  if (argc < 2) {
    fprintf(stderr, "usage: %s <apfs container file> [superblock offset] [target file]\n", argv[0]);
    exit(1);
  }

  if (argc == 4) {
    search_file = argv[3];
  } else {
    search_file = default_file;
  }

  size_t sz_sf = strlen(search_file) + 1; //  +1 for trailing '\0'
  nx_superblock_t sb;
  FILE *fp = fopen(argv[1], "r");
  long offset = 0x0;
  if (argc == 3) {
    offset = strtol(argv[2], NULL, 0);
    if (offset % 0x1000 != 0) {
      // TODO: Operate on larger block sizes
      fprintf(stderr, "[-] The program assumes block size is 0x1000 only\n");
      exit(1);
    }
    fseek(fp, offset, SEEK_SET);
  }
  fread((void *)&sb, sizeof(nx_superblock_t), 1, fp);
  if (!(sb.nx_o.o_type.get() & OBJECT_TYPE_NX_SUPERBLOCK) || (sb.nx_magic != 0x4253584e)) {
    fprintf(stderr, "[-] Invalid superblock!\n");
    exit(1);
  }
  printf("[*] APFS superblock at 0x%lx (oid: 0x%lx, xid: 0x%lx)\n", offset, sb.nx_o.o_oid.get(), sb.nx_o.o_xid.get());

  offset = ((long)sb.nx_omap_oid.get()) << 12;
  fseek(fp, offset, SEEK_SET);
  printf("[*] Container object map at 0x%lx\n", offset);
  printf("[*] Seeking to offset 0x%lx for omap_phys_t\n", offset);

  // Now fp should point to an object map (omap_phys_t)
  omap_phys_t om;
  fread((void *)&om, sizeof(omap_phys_t), 1, fp);
  if (!(om.om_o.o_type.get() & OBJECT_TYPE_CHECKPOINT_MAP)) {
    fprintf(stderr, "[-] Invalid object map!\n");
    exit(1);
  }
  offset = ((long)om.om_tree_oid) << 12;
  fseek(fp, offset, SEEK_SET);
  printf("[*] Seeking to offset 0x%lx for btree_node_phys_t\n", offset);

  // Now fp should point to a B-tree node (btree_node_phys_t)
  btree_node_phys_t btn;
  fread((void *)&btn, sizeof(btree_node_phys_t), 1, fp);
  if (!(btn.btn_o.o_type.get() & OBJECT_TYPE_BTREE)) {  // Either 2 or 3 in lower 2-bytes
    fprintf(stderr, "[-] Invalid B-tree node!\n");
    exit(1);
  }
  
  // Now fp should point to the end of btree_node_phys_t
  uint32_t nvols = btn.btn_nkeys.get();
  long toc_offset = ftell(fp) + btn.btn_table_space.off.get();
  FILE *toc_fp = fopen(argv[1], "r");
  fseek(toc_fp, toc_offset, SEEK_SET);
  long key_offset = toc_offset + btn.btn_table_space.len.get();
  FILE *key_fp = fopen(argv[1], "r");
  fseek(key_fp, key_offset, SEEK_SET);
  long val_offset = ftell(fp) - sizeof(btree_node_phys_t) + 0x1000 - sizeof(btree_info_t);
  FILE *val_fp = fopen(argv[1], "r");
  fseek(val_fp, val_offset, SEEK_SET);
  
  if (btn.btn_flags & BTNODE_FIXED_KV_SIZE) {
    printf("[*] KV size is fixed to 0x10\n");
  } else {
    fprintf(stderr, "[-] KV length not fixed. The program cannot handle this yet...\n");
    exit(1);
  }

  uint16_t key_metaoffset, val_metaoffset;
  omap_key_t ok;
  omap_val_t ov;
  uint64_t paddr_volumes[nvols];
  for (int i = 0; i < nvols; i++) {
    fread(&key_metaoffset, sizeof(uint16_t), 1, toc_fp);
    fread(&val_metaoffset, sizeof(uint16_t), 1, toc_fp);

    fseek(key_fp, (long)key_metaoffset, SEEK_CUR);
    fread(&ok, sizeof(omap_key_t), 1, key_fp);
    fseek(key_fp, key_offset, SEEK_SET);

    fseek(val_fp, -1 * (long)val_metaoffset, SEEK_CUR);
    fread(&ov, sizeof(omap_val_t), 1, val_fp);
    fseek(val_fp, val_offset, SEEK_SET);

    paddr_volumes[i] = ov.ov_paddr.get() << 12;
    printf("[*] Volume superblock at 0x%lx\n", paddr_volumes[i]);
  }

  putchar('\n');
  apfs_superblock_t apfs;
  FILE *fstree_fp = NULL;
  for (int i = 0; i < nvols; i++) {
    printf("======================== Volume %03d ========================\n", i + 1);
    fseek(fp, (long)paddr_volumes[i], SEEK_SET);
    fread(&apfs, sizeof(apfs_superblock_t), 1, fp);
    if ((apfs.apfs_magic.get() != 0x42535041) || (apfs.apfs_o.o_type.get() != OBJECT_TYPE_FS)) {
      fprintf(stderr, "[-] Invalid APFS superblock!\n");
      exit(1);
    }
    fseek(fp, (long)(apfs.apfs_omap_oid.get() << 12), SEEK_SET);
    fread(&om, sizeof(omap_phys_t), 1, fp);
    if (!(om.om_o.o_type & OBJECT_TYPE_OMAP)) {
      fprintf(stderr, "[-] Invalid Object map!\n");
      exit(1);
    }
    fseek(fp, (long)(om.om_tree_oid.get() << 12), SEEK_SET);
    fread(&btn, sizeof(btree_node_phys_t), 1, fp);
    // This B-tree node should contain references to file/directory. 
    if (!(btn.btn_o.o_type & OBJECT_TYPE_BTREE)) {
      fprintf(stderr, "[-] Invalid B-tree node!\n");
      exit(1);
    }
    // Now fp should point to the end of btree_node_phys_t
    toc_offset = ftell(fp) + btn.btn_table_space.off.get();
    fseek(toc_fp, toc_offset, SEEK_SET);

    key_offset = toc_offset + btn.btn_table_space.len.get();
    fseek(key_fp, key_offset, SEEK_SET);

    val_offset = ftell(fp) - sizeof(btree_node_phys_t) + 0x1000 - sizeof(btree_info_t);
    fseek(val_fp, val_offset, SEEK_SET);

    uint32_t nfs = btn.btn_nkeys.get();
    fstree_fp = fopen(argv[1], "r");
    long target_file_keyoff = -1, target_file_valoff = -1;
    for (int j = 0; j < nfs; j++) {
      printf("------------------------ FSTREE %03d ------------------------\n", j + 1);
      fread(&key_metaoffset, sizeof(uint16_t), 1, fp);
      fread(&val_metaoffset, sizeof(uint16_t), 1, fp);
      // printf("[+] key=0x%04hx, val=0x%04hx\n", key_metaoffset, val_metaoffset);
      fseek(key_fp, (long)key_metaoffset, SEEK_CUR);
      fread(&ok, sizeof(omap_key_t), 1, key_fp);
      fseek(key_fp, key_offset, SEEK_SET);
      fseek(val_fp, -1 * (long)val_metaoffset, SEEK_CUR);
      fread(&ov, sizeof(omap_val_t), 1, val_fp);
      fseek(val_fp, val_offset, SEEK_SET);

      fseek(fstree_fp, ov.ov_paddr.get() << 12, SEEK_SET);
      fread(&btn, sizeof(btree_node_phys_t), 1, fstree_fp);

      uint16_t key_metalen, val_metalen;
      FILE *fs_key_fp = fopen(argv[1], "r");
      FILE *fs_val_fp = fopen(argv[1], "r");
      long keyfld_begin = (ov.ov_paddr.get() << 12) + sizeof(btree_node_phys_t) + btn.btn_table_space.len.get();
      long valfld_begin = (ov.ov_paddr.get() << 12) + 0x1000; // no btree_info_t resides!
      char checker[sz_sf];
      uint64_t obj_id = 0, obj_type = 0, logical_addr = 0;
      bool check_onward = false;
      uint64_t target_file_id;
      uint64_t tmp_file_id, tmp_koff, tmp_voff;
      uint64_t tmp_pbn;
      uint64_t tmp_len_and_flags;
      // Create a vector of <{obj_id, logical_addr, phys_block_num}> for file-extents
      std::vector<std::tuple<uint64_t, uint64_t, uint64_t, uint64_t>> vec_id_laddr_pbn_laf;
      std::tuple<uint64_t, uint64_t, uint64_t, uint64_t> tmp_tup;
      printf("[*] Dumping %d KEY/VAL pair with their IDs\n", btn.btn_nkeys.get());
      for (int k = 0; k < btn.btn_nkeys.get(); k++) {
        fread(&key_metaoffset, sizeof(uint16_t), 1, fstree_fp);
        fread(&key_metalen, sizeof(uint16_t), 1, fstree_fp);
        fread(&val_metaoffset, sizeof(uint16_t), 1, fstree_fp);
        fread(&val_metalen, sizeof(uint16_t), 1, fstree_fp);

        fseek(fs_key_fp, keyfld_begin + key_metaoffset, SEEK_SET);
        fseek(fs_val_fp, valfld_begin - val_metaoffset, SEEK_SET);
        j_key_t hdr;
        fread(&hdr, sizeof(j_key_t), 1, fs_key_fp);
        fread(&logical_addr, sizeof(uint64_t), 1, fs_key_fp);
        fseek(fs_key_fp, key_metalen - ((long)(sizeof(j_key_t))) - ((long)(sizeof(uint64_t))) - sz_sf, SEEK_CUR);
        fread(checker, sizeof(char), sizeof(checker), fs_key_fp);
        fseek(fs_key_fp, -1 * key_metalen, SEEK_CUR);
        uint64_t id_and_type = hdr.obj_id_and_type.get();
        if (strcmp(checker, search_file) == 0) {
          target_file_keyoff = ftell(fs_key_fp);
          target_file_valoff = ftell(fs_val_fp);
        }
        obj_type = id_and_type >> OBJ_TYPE_SHIFT;
        obj_id = id_and_type & OBJ_ID_MASK;
        printf("\t╭──j_key_t header, TYPE: 0x%1lx, ID: 0x%lx", obj_type, obj_id);
        if (obj_type == APFS_TYPE_FILE_EXTENT) {
          // Look for logical id 
          printf(", FrgmntFileOffset: 0x%016lx", logical_addr);
          fread(&tmp_len_and_flags, sizeof(uint64_t), 1, fs_val_fp);
          fread(&tmp_pbn, sizeof(uint64_t), 1, fs_val_fp);
          fseek(fs_val_fp, -16L, SEEK_CUR); // Seek back
          tmp_tup = std::make_tuple(obj_id, logical_addr, tmp_pbn, tmp_len_and_flags);
          vec_id_laddr_pbn_laf.push_back(tmp_tup);
        }
        putchar('\n');
        printf("\t╰──0x%04hx byte val begins @ 0x%016lx\n", val_metalen, ftell(fs_val_fp));

        // Check whether file id matches if target file id has already been found
        if ((check_onward) && (obj_type == APFS_TYPE_FILE_EXTENT) && (target_file_id == obj_id)) {
          // Hit!
          printf("\t[+] Target file %s starts at 0x%lx with logical address 0x%lx and length 0x%lx\n\n", search_file, std::get<2>(tmp_tup) << 12, std::get<1>(tmp_tup), std::get<3>(tmp_tup) & J_FILE_EXTENT_LEN_MASK);
        }

        if ((target_file_keyoff != -1) && (target_file_valoff != -1)) {
          fseek(fs_val_fp, target_file_valoff, SEEK_SET);
          fread(&target_file_id, sizeof(uint64_t), 1, fs_val_fp);
          printf("\t[+] Got KEY: 0x%016lx, VAL: 0x%016lx for %s (id: 0x%lx)\n", target_file_keyoff, target_file_valoff, search_file, target_file_id);
          // Find from vector
          for (auto &tup : vec_id_laddr_pbn_laf) {
            if (std::get<0>(tup) == target_file_id) {
              // Hit!
              printf("\t[+] Target file %s starts at 0x%lx with logical address 0x%lx and length 0x%lx\n\n", search_file, std::get<2>(tup) << 12, std::get<1>(tup), std::get<3>(tup) & J_FILE_EXTENT_LEN_MASK);
            }
          }
          // If not found, continue searching onwards
          check_onward = true;
          target_file_keyoff = -1;
          target_file_valoff = -1;
        }
      }
      fclose(fs_key_fp);
      fclose(fs_val_fp);
    }

  }

  if (fstree_fp != NULL) fclose(fstree_fp);
  fclose(val_fp);
  fclose(key_fp);
  fclose(toc_fp);
  fclose(fp);
  return 0;
}

