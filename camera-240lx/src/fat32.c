#include "fat_compat.h"
#include "rpi.h"
#include "fat32.h"
#include "fat32-helpers.h"
#include "pi-sd.h"

static int trace_p = 0;   // quiet by default (per-cluster printk is slow)
static int init_p = 0;

fat32_boot_sec_t boot_sector;

// FAT dirty-sector tracking (perf). flushing the WHOLE on-disk FAT (often MBs)
// on every call dominated save time, so we track the range of entries touched
// since the last flush and write only the 512-byte sectors that changed. every
// `fs->fat[i] = ...` is paired with fat_mark_dirty(i).
static uint32_t fat_dirty_lo = 0xFFFFFFFF;  // lowest dirty cluster index
static uint32_t fat_dirty_hi = 0;           // highest dirty cluster index

static void fat_mark_dirty(uint32_t cluster) {
    if (cluster < fat_dirty_lo) fat_dirty_lo = cluster;
    if (cluster > fat_dirty_hi) fat_dirty_hi = cluster;
}

// A real data cluster index is in [2, n_entries). Following a corrupted chain
// can yield a wild index; guard every chain-follow so a bad card fails
// gracefully instead of data-aborting.
static int fat_cluster_valid(fat32_fs_t *fs, uint32_t c) {
    return c >= 2 && c < fs->n_entries;
}

// the next cluster after `c`. FAT32 entries are 28-bit -- the top 4 bits are
// reserved, so mask them before using an entry as a cluster index.
static uint32_t fat_next(fat32_fs_t *fs, uint32_t c) {
    return fs->fat[c] & 0x0FFFFFFF;
}

// is cluster `c` the end of its chain? you classify the ENTRY VALUE fs->fat[c]
// (does c point onward, or is it an end-of-chain marker?), NOT the index c.
static int fat_is_last(fat32_fs_t *fs, uint32_t c) {
    return fat32_fat_entry_type(fs->fat[c]) == LAST_CLUSTER;
}


// DONE
fat32_fs_t fat32_mk(mbr_partition_ent_t *partition) {
  demand(!init_p, "the fat32 module is already in use\n");
  // TODO: Read the boot sector (of the partition) off the SD card.
  fat32_boot_sec_t *boot_sector_p = pi_sec_read(partition->lba_start, 1);
  boot_sector = *boot_sector_p;

  // TODO: Verify the boot sector (also called the volume id, `fat32_volume_id_check`)
  fat32_volume_id_check(boot_sector_p); ///fat32_boot_sec_t *b

  // TODO: Read the FS info sector (the sector immediately following the boot
  // sector) and check it (`fat32_fsinfo_check`, `fat32_fsinfo_print`)
  assert(boot_sector.info_sec_num == 1);
  // void fat32_fsinfo_check(struct fsinfo *info);
  struct fsinfo *fsinfo = (struct fsinfo *)(pi_sec_read(partition->lba_start + 1, 1)); // read sector after boot ==> fsinfo
  fat32_fsinfo_check(fsinfo);
  fat32_fsinfo_print("fsinfo printed: ", fsinfo);

  // END OF PART 2
  // The rest of this is for Part 3:

  // TODO: calculate the fat32_fs_t metadata, which we'll need to return.
  unsigned lba_start = -1; // from the partition
  unsigned fat_begin_lba = -1; // the start LBA + the number of reserved sectors
  unsigned cluster_begin_lba = -1; // the beginning of the FAT, plus the combined length of all the FATs
  unsigned sec_per_cluster = -1; // from the boot sector
  unsigned root_first_cluster = -1; // from the boot sector
  unsigned n_entries = -1; // from the boot sector

  // aditi wrote this!!
  lba_start = partition->lba_start;
  fat_begin_lba = lba_start + boot_sector_p->reserved_area_nsec; // double check this is the num of reserved sectors
  cluster_begin_lba = lba_start + boot_sector_p->reserved_area_nsec + (boot_sector_p->nfats * boot_sector_p->nsec_per_fat);
  sec_per_cluster = boot_sector_p->sec_per_cluster;
  root_first_cluster = boot_sector_p->first_cluster;
  n_entries = boot_sector_p->nsec_per_fat * boot_sector_p->bytes_per_sec / 4;  // div by 4 bc 32 bit = 4 bytes per entry

  /*
   * TODO: Read in the entire fat (one copy: worth reading in the second and
   * comparing).
   *
   * The disk is divided into clusters. The number of sectors per
   * cluster is given in the boot sector byte 13. <sec_per_cluster>
   *
   * The File Allocation Table has one entry per cluster. This entry
   * uses 12, 16 or 28 bits for FAT12, FAT16 and FAT32.
   *
   * Store the FAT in a heap-allocated array.
   */
  uint32_t *fat;
  fat = (uint32_t *)(pi_sec_read(fat_begin_lba, boot_sector_p->nsec_per_fat));

  // Create the FAT32 FS struct with all the metadata
  fat32_fs_t fs = (fat32_fs_t) {
    .lba_start = lba_start,
      .fat_begin_lba = fat_begin_lba,
      .cluster_begin_lba = cluster_begin_lba,
      .sectors_per_cluster = sec_per_cluster,
      .root_dir_first_cluster = root_first_cluster,
      .fat = fat,
      .n_entries = n_entries,
  };

  if (trace_p) {
    trace("begin lba = %d\n", fs.fat_begin_lba);
    trace("cluster begin lba = %d\n", fs.cluster_begin_lba);
    trace("sectors per cluster = %d\n", fs.sectors_per_cluster);
    trace("root dir first cluster = %d\n", fs.root_dir_first_cluster);
  }

  init_p = 1;
  return fs;
}

// Given cluster_number, get lba.  Helper function.
// TODO
static uint32_t cluster_to_lba(fat32_fs_t *f, uint32_t cluster_num) {
  assert(cluster_num >= 2);
  // TODO: calculate LBA from cluster number, cluster_begin_lba, and
  // sectors_per_cluster
  unsigned lba = (uint32_t)(f->cluster_begin_lba + (cluster_num - 2) * f->sectors_per_cluster);  // from prelab

  // unsigned lba;
  if (trace_p) trace("cluster %d to lba: %d\n", cluster_num, lba);
  return lba;
}

// DONE
pi_dirent_t fat32_get_root(fat32_fs_t *fs) {
  demand(init_p, "fat32 not initialized!");
  // TODO: return the information corresponding to the root directory (just
  // cluster_id, in this case)

  return (pi_dirent_t) {
    .name = "",
      .raw_name = "",
      .cluster_id = fs->root_dir_first_cluster, // ADITI EDIT
      .is_dir_p = 1,
      .nbytes = 0,//fs->n_entries * 32,
  };
}

// Given the starting cluster index, get the length of the chain.  Helper
// function.
// DONE
static uint32_t get_cluster_chain_length(fat32_fs_t *fs, uint32_t start_cluster) {
  // walk the chain until this cluster's entry is an end-of-chain marker, counting
  // each cluster (including the last). guard against a corrupt/cyclic chain by
  // stopping at an invalid index and capping at the table size.
  if (start_cluster == 0) return 0;

  uint32_t cur = start_cluster;
  uint32_t n = 1;
  while (fat_cluster_valid(fs, cur) && !fat_is_last(fs, cur) && n < fs->n_entries) {
    cur = fat_next(fs, cur);
    n++;
  }
  return n;
}

// Given the starting cluster index, read a cluster chain into a contiguous
// buffer.  Assume the provided buffer is large enough for the whole chain.
// Helper function.
// DONE
static void read_cluster_chain(fat32_fs_t *fs, uint32_t start_cluster, uint8_t *data) {
  // walk the chain, copying each cluster (including the last) into `data`.
  if (start_cluster == 0) return;   // empty: buffer may be 0-length, don't touch

  uint32_t cur = start_cluster;
  uint8_t *next_data = data;
  uint32_t cbytes = fs->sectors_per_cluster * boot_sector.bytes_per_sec;

  while (fat_cluster_valid(fs, cur)) {
    void *read_data = pi_sec_read(cluster_to_lba(fs, cur), fs->sectors_per_cluster);
    memcpy(next_data, read_data, cbytes);
    next_data += cbytes;

    if (fat_is_last(fs, cur)) break;
    cur = fat_next(fs, cur);
  }
}

// Converts a fat32 internal dirent into a generic one suitable for use outside
// this driver.
// DONE
static pi_dirent_t dirent_convert(fat32_dirent_t *d) {
  pi_dirent_t e = {
    .cluster_id = fat32_cluster_id(d),
    .is_dir_p = d->attr == FAT32_DIR,
    .nbytes = d->file_nbytes,
  };
  // can compare this name
  memcpy(e.raw_name, d->filename, sizeof d->filename);
  // for printing.
  fat32_dirent_name(d,e.name);
  return e;
}

// Gets all the dirents of a directory which starts at cluster `cluster_start`.
// Return a heap-allocated array of dirents.
// DONE
static fat32_dirent_t *get_dirents(fat32_fs_t *fs, uint32_t cluster_start, uint32_t *dir_n) {
  uint32_t cl_ch_len = get_cluster_chain_length(fs, cluster_start);

  // cap the chain length: a corrupt/cyclic chain can report a huge length and
  // make the alloc below enormous. a sane directory is only a few clusters.
  #define MAX_DIR_CLUSTERS 64
  if (cl_ch_len > MAX_DIR_CLUSTERS) {
    printk("[fat32] directory chain length %d > %d (corrupt?) -- capping\n",
           cl_ch_len, MAX_DIR_CLUSTERS);
    cl_ch_len = MAX_DIR_CLUSTERS;
  }

  uint32_t amnt_alloc = cl_ch_len * boot_sector.sec_per_cluster * boot_sector.bytes_per_sec;
  uint8_t *data = kmalloc(amnt_alloc);

  read_cluster_chain(fs, cluster_start, data);

  *dir_n = amnt_alloc / sizeof(fat32_dirent_t);
  return (fat32_dirent_t *)data;
}


// DONE
pi_directory_t fat32_readdir(fat32_fs_t *fs, pi_dirent_t *dirent) {
  demand(init_p, "fat32 not initialized!");
  demand(dirent->is_dir_p, "tried to readdir a file!");
  // TODO: use `get_dirents` to read the raw dirent structures from the disk
  uint32_t n_dirents;
  fat32_dirent_t *fat_dirents = get_dirents(fs, dirent->cluster_id, &n_dirents);

  // TODO: allocate space to store the pi_dirent_t return values
  // 16 dir entries in a sector. can be multiple sectors
  pi_dirent_t *pi_dirents = kmalloc(n_dirents * sizeof(pi_dirent_t)); // TODO COME BACK!!

  // TODO: iterate over the directory and create pi_dirent_ts for every valid
  // file.  Don't include empty dirents, LFNs, or Volume IDs.  You can use
  // `dirent_convert`.
  int pi_j = 0;
  for (int i = 0; i < n_dirents; i++) {
    if (fat32_dirent_free(&fat_dirents[i])) continue; // free space
    if (fat32_dirent_is_lfn(&fat_dirents[i])) continue; // LFN version of name
    if (fat_dirents[i].attr & FAT32_VOLUME_LABEL) continue; // volume label

    pi_dirents[pi_j] = dirent_convert(&fat_dirents[i]);
    pi_j++;
  }

  // TODO: create a pi_directory_t using the dirents and the number of valid
  // dirents we found
  return (pi_directory_t) {
    .dirents = pi_dirents, // aditi edit
    .ndirents = pi_j, // these are the valid pi_dirents I copied into pi_dirents
  };
}

// DONE
static int find_dirent_with_name(fat32_dirent_t *dirents, int n, char *filename) {
  // TODO: iterate through the dirents, looking for a file which matches the
  // name; use `fat32_dirent_name` to convert the internal name format to a
  // normal string.
  for (int i = 0; i < n; i++) {
    // check if the first 11 chars are the same
    char name[15];
    fat32_dirent_name(&dirents[i], name); // convert the file name to fat format
    if (strcmp(name, filename) == 0) return i;
  }

  return -1;
}

// DONE
pi_dirent_t *fat32_stat(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  demand(init_p, "fat32 not initialized!");
  demand(directory->is_dir_p, "tried to use a file as a directory");

  // TODO: use `get_dirents` to read the raw dirent structures from the disk
  uint32_t dir_n = 0;
  fat32_dirent_t *fat_dirents = get_dirents(fs, directory->cluster_id, &dir_n);

  // TODO: Iterate through the directory's entries and find a dirent with the
  // provided name.  Return NULL if no such dirent exists.  You can use
  // `find_dirent_with_name` if you've implemented it.
  int found_name_index = find_dirent_with_name(fat_dirents, dir_n, filename);
  if (found_name_index == -1) return NULL;

  // found_name
  pi_dirent_t *dirent = kmalloc(sizeof(pi_dirent_t));
  *dirent = dirent_convert(&fat_dirents[found_name_index]);

  // TODO: allocate enough space for the dirent, then convert
  // (`dirent_convert`) the fat32 dirent into a Pi dirent.
  // pi_dirent_t *dirent = NULL;
  return dirent;
}

// DONE
pi_file_t *fat32_read(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  // This should be pretty similar to readdir, but simpler.
  demand(init_p, "fat32 not initialized!");
  demand(directory->is_dir_p, "tried to use a file as a directory!");

  // TODO: read the dirents of the provided directory and look for one matching the provided name
  pi_dirent_t* pi_dirent = fat32_stat(fs, directory, filename);
  if (pi_dirent == NULL) {printk ("ERROR!"); return NULL;}

  // TODO: figure out the length of the cluster chain
  uint32_t cl_ch_len = get_cluster_chain_length(fs, pi_dirent->cluster_id);

  // TODO: allocate a buffer large enough to hold the whole file
  uint32_t num_bytes = pi_dirent->nbytes;

  uint8_t *buf = kmalloc(cl_ch_len * 512 * fs->sectors_per_cluster); // * num_bytes * 512 / fs->sectors_per_cluster);

  // TODO: read in the whole file (if it's not empty)
  read_cluster_chain(fs, pi_dirent->cluster_id, buf);

  // TODO: fill the pi_file_t
  pi_file_t *file = kmalloc(sizeof(pi_file_t));
  *file = (pi_file_t) {
    .data = buf,  // aditi edit!
    .n_data = num_bytes,
    .n_alloc = cl_ch_len * 512 * fs->sectors_per_cluster,
  };
  return file;
}


/******************************************************************************
 * Everything below here is for writing to the SD card (Part 7/Extension).  If
 * you're working on read-only code, you don't need any of this.
 ******************************************************************************/

// DONE
static uint32_t find_free_cluster(fat32_fs_t *fs, uint32_t start_cluster) {
  // TODO: loop through the entries in the FAT until you find a free one
  // (fat32_fat_entry_type == FREE_CLUSTER).  Start from cluster 3.  Panic if
  // there are none left.
  if (start_cluster < 3) start_cluster = 3;

  // uint32_t cur_cluster = start_cluster;

  uint32_t num_clusters = fs->n_entries;
  for (int i = start_cluster; i < num_clusters; i++) {
    if (fat32_fat_entry_type(fs->fat[i]) == FREE_CLUSTER) return i;
    // cur_cluster += 1;
  }

  if (trace_p) trace("failed to find free cluster from %d\n", start_cluster);
  panic("No more clusters on the disk!\n");
}

static void write_fat_to_disk(fat32_fs_t *fs) {
  if (fat_dirty_lo > fat_dirty_hi)
    return;   // nothing changed since last flush

  // FAT32 entries are 4 bytes; (bytes_per_sec/4) entries per sector.
  uint32_t entries_per_sec = boot_sector.bytes_per_sec / 4;
  uint32_t lo_sec = fat_dirty_lo / entries_per_sec;
  uint32_t hi_sec = fat_dirty_hi / entries_per_sec;
  uint32_t nsec   = hi_sec - lo_sec + 1;

  if (trace_p) trace("syncing FAT sectors %d..%d (%d of %d)\n",
                     lo_sec, hi_sec, nsec, boot_sector.nsec_per_fat);

  uint8_t *fat_bytes = (uint8_t *)fs->fat;
  // write the dirty range to EVERY FAT copy (usually 2). updating only FAT #1
  // makes the mirror disagree -> macOS flags the volume inconsistent ("error
  // -50") until fsck reconciles them.
  for (uint32_t f = 0; f < boot_sector.nfats; f++) {
    pi_sd_write(fat_bytes + lo_sec * boot_sector.bytes_per_sec,
                fs->fat_begin_lba + f * boot_sector.nsec_per_fat + lo_sec, nsec);
  }

  fat_dirty_lo = 0xFFFFFFFF;   // range consumed
  fat_dirty_hi = 0;
}


// credit: Suze
static uint32_t min(uint32_t a, uint32_t b) {
  if (a < b) {
    return a;
  }
  return b;
}

// Write `data` over the chain starting at start_cluster, extending it with new
// clusters if the data is longer. Returns 1 if every cluster reached the card,
// 0 if a pi_sd_write failed or the chain was corrupt. Callers that record a
// file's size MUST check this: a silently-failed data write would otherwise
// leave a dirent with the right byte count pointing at clusters that never got
// the bytes (the "right size, all zeros" bug).
//
// credit: Suze
static int write_cluster_chain(fat32_fs_t *fs, uint32_t start_cluster, uint8_t *data, uint32_t nbytes) {
  if (nbytes == 0) return 1;
  if (trace_p) trace("writing %d bytes to cluster %d\n", nbytes, start_cluster);

  uint32_t cluster = start_cluster;
  uint32_t bytes_written = 0;
  while (bytes_written < nbytes) {
    // never index fs->fat / write a sector with a wild cluster number. abort
    // instead of data-aborting on a corrupt chain.
    if (!fat_cluster_valid(fs, cluster)) {
      printk("[fat32] write: bad cluster %d (n_entries=%d), aborting\n",
             cluster, fs->n_entries);
      write_fat_to_disk(fs);
      return 0;
    }

    uint32_t bytes_per_cluster = boot_sector.bytes_per_sec * fs->sectors_per_cluster;
    uint32_t bytes_to_write = min(nbytes - bytes_written, bytes_per_cluster);

    uint32_t lba = cluster_to_lba(fs, cluster);
    if (pi_sd_write(data + bytes_written, lba, fs->sectors_per_cluster) < 0) {
      printk("[fat32] write: pi_sd_write FAILED at cluster %d (lba %d), aborting\n",
             cluster, lba);
      write_fat_to_disk(fs);
      return 0;
    }
    bytes_written += bytes_to_write;

    // advance to (or allocate) the next cluster only if more data remains. doing
    // this unconditionally would append an extra cluster when the data exactly
    // fills the last one.
    if (bytes_written < nbytes) {
      if (fat_is_last(fs, cluster)) {
        uint32_t new_cluster = find_free_cluster(fs, cluster);
        fs->fat[cluster] = new_cluster;      fat_mark_dirty(cluster);
        fs->fat[new_cluster] = LAST_CLUSTER; fat_mark_dirty(new_cluster);
        cluster = new_cluster;
      } else {
        cluster = fat_next(fs, cluster);
      }
    }
  }

  // free any leftover clusters from a previously-longer chain, then truncate
  // here. masking via fat_next stops a reserved/EOC entry from being read as a
  // wild index.
  if (fat_cluster_valid(fs, cluster)) {
    uint32_t c = fat_next(fs, cluster);
    fs->fat[cluster] = LAST_CLUSTER;  fat_mark_dirty(cluster);
    while (fat_cluster_valid(fs, c)) {
      int was_last = fat_is_last(fs, c);
      uint32_t nxt = fat_next(fs, c);
      fs->fat[c] = FREE_CLUSTER;  fat_mark_dirty(c);
      if (was_last) break;
      c = nxt;
    }
  }
  write_fat_to_disk(fs);
  return 1;
}




// DONE
int fat32_rename(fat32_fs_t *fs, pi_dirent_t *directory, char *oldname, char *newname) {
  // TODO: Get the dirents `directory` off the disk, and iterate through them
  // looking for the file.  When you find it, rename it and write it back to
  // the disk (validate the name first).  Return 0 in case of an error, or 1
  // on success.
  // Consider:
  //  - what do you do when there's already a file with the new name?

  // Credit: Suze

  demand(init_p, "fat32 not initialized!");
  if (!fat32_is_valid_name(newname)) return 0;
  if (trace_p) trace("renaming %s to %s\n", oldname, newname);

  // TODO: get the dirents and find the right one
  uint32_t n_dirents;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n_dirents);
  int dirent_to_rename = find_dirent_with_name(dirents, n_dirents, oldname);
  if (dirent_to_rename == -1) {
    panic("cannot find old file");
    return 0;
  }
  if (find_dirent_with_name(dirents, n_dirents, newname) != -1) {
    panic("file with new name already exists\n");
    return 0;
  }

  // TODO: update the dirent's name
  fat32_dirent_set_name(&dirents[dirent_to_rename], newname);

  // TODO: write out the directory, using the existing cluster chain (or
  // appending to the end); implementing `write_cluster_chain` will help
  write_cluster_chain(fs, directory->cluster_id, (uint8_t *)dirents, n_dirents * sizeof(fat32_dirent_t));
  
  return 1;

}

// DONE
// Create a new directory entry for an empty file (or directory).
pi_dirent_t *fat32_create(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, int is_dir) {
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("creating %s\n", filename);
  if (!fat32_is_valid_name(filename)) return NULL;

  // TODO: read the dirents and make sure there isn't already a file with the
  // same name

  // check that the directory is actually a directory 
  if (!directory->is_dir_p) {printk("directory is not a dir NOT FOUND!"); return NULL;}

  uint32_t dir_n = 0;
  fat32_dirent_t *fat_dirents = get_dirents(fs, directory->cluster_id, &dir_n);
  int dirent_with_name = find_dirent_with_name(fat_dirents, dir_n, filename);
  if (dirent_with_name != -1) {printk("dirent_with_name already exists!"); return NULL;}

  // TODO: look for a free directory entry and use it to store the data for the
  // new file.  If there aren't any free directory entries, either panic or
  // (better) handle it appropriately by extending the directory to a new
  // cluster.
  // When you find one, update it to match the name and attributes
  // specified; set the size and cluster to 0.
  int dir_found = -1;
  for (int i = 0; i < dir_n; i++) {
    fat32_dirent_t *fdir = &fat_dirents[i];
    if (fat32_dirent_free(fdir)) {
      dir_found = i;
      // set name
      fat32_dirent_set_name(fdir, filename); // dirent entry, then file name
      // set attr
      fdir->attr = is_dir ? FAT32_DIR : FAT32_ARCHIVE;
      // set size
      fdir->file_nbytes = 0; // set to 0 all the time
      // set cluster - sus ?? TODO CHECK is this how we handle the cluster correctly??
      fdir->hi_start = 0;
      fdir->lo_start = 0;
      break;
    }
  }
  if (dir_found == -1) {panic("dir_found NOT FOUND!");}

  // TODO: write out the updated directory to the disk
  // joe comment: [do the math] figure out sector num i of entry, and divide by number of sectors in cluster. then jst write back that specific cluster
  // figure out which sector and just write that one back
  write_fat_to_disk(fs);
  write_cluster_chain(fs, directory->cluster_id, (uint8_t *)fat_dirents, dir_n * sizeof(fat32_dirent_t));

  // TODO: convert the dirent to a `pi_dirent_t` and return a (kmalloc'ed) pointer
  // dir_found / [entries in a cluster] = cluster num
  // dir_found % [entries in a sector] = sector num
  pi_dirent_t *dirent = kmalloc(sizeof(pi_dirent_t));
  *dirent = dirent_convert(&fat_dirents[dir_found]);
  return dirent;
}



// Delete a file, including its directory entry.
int fat32_delete(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  
  // CREDIT: Suze

  // TODO: edit suze
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("deleting %s\n", filename);
  if (!fat32_is_valid_name(filename)) return 0;
  // TODO: look for a matching directory entry, and set the first byte of the
  // name to 0xE5 to mark it as free
  // unimplemented();
  uint32_t n_dirents;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n_dirents);
  int dirent_want = find_dirent_with_name(dirents, n_dirents, filename);
  if (dirent_want == -1) {
    trace("no matching file to delete\n");
    return 1;
  }
  trace("dirent_want to delete: %d\n", dirent_want);
  dirents[dirent_want].filename[0] = 0xE5;

  uint32_t cluster = dirents[dirent_want].lo_start | (dirents[dirent_want].hi_start << 16);

  // free the clusters referenced by this dirent (classify the entry value, mask
  // before following, and guard the index so a corrupt chain can't run wild).
  write_cluster_chain(fs, directory->cluster_id, (uint8_t *)dirents, n_dirents * sizeof(fat32_dirent_t));
  while (fat_cluster_valid(fs, cluster) &&
         fat32_fat_entry_type(fs->fat[cluster]) != FREE_CLUSTER) {
    if (trace_p) trace("freeing cluster %d\n", cluster);
    int was_last = fat_is_last(fs, cluster);
    uint32_t nxt = fat_next(fs, cluster);
    fs->fat[cluster] = FREE_CLUSTER;  fat_mark_dirty(cluster);
    if (was_last) break;
    cluster = nxt;
  }

  // TODO: write out the updated directory to the disk
  if (trace_p) trace("writing out the updated directory\n");
  write_fat_to_disk(fs);
  
  return 1;
}

int fat32_truncate(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, unsigned length) {
  // Credit: Suze
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("truncating %s\n", filename);

  uint32_t n_dirents;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n_dirents);
  int dirent_want_idx = find_dirent_with_name(dirents, n_dirents, filename);
  if (dirent_want_idx == -1) {
    trace("no matching directory to truncate found\n");
    return 0;
  }
  fat32_dirent_t *dirent_to_truncate = &dirents[dirent_want_idx];
  dirent_to_truncate->file_nbytes = length;
  pi_file_t *cur_file = fat32_read(fs, directory, filename);
  // should be writing what is currently in the file
  write_cluster_chain(fs, dirent_to_truncate->lo_start | (dirent_to_truncate->hi_start << 16), cur_file->data, length);


  write_cluster_chain(fs, directory->cluster_id, (uint8_t *)dirents, n_dirents * sizeof(fat32_dirent_t));
  write_fat_to_disk(fs);
  return 1;
}


int fat32_write(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, pi_file_t *file) {

  // credit: Suze
  demand(init_p, "fat32 not initialized!");
  demand(directory->is_dir_p, "tried to use a file as a directory!");

  uint32_t dir_n;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &dir_n);
  int dirent_index = find_dirent_with_name(dirents, dir_n, filename);
  if (dirent_index == -1) {
    trace("no matching directory entry\n");
    return 0; // exit with error?
  }
  // update the directory entry with the new size

  fat32_dirent_t *dirent_to_write = &dirents[dirent_index];
  dirent_to_write->file_nbytes = file->n_data;
  // allocate a fresh start cluster unless the dirent already points at a VALID
  // data cluster. checking just `lo==0 && hi==0` isn't enough: a dirty card can
  // leave the start field holding an EOC sentinel, which isn't 0 but also isn't
  // a real cluster -- treat 0, EOC, or out-of-range all as "needs allocation".
  uint32_t start = dirent_to_write->lo_start | (dirent_to_write->hi_start << 16);
  if (!fat_cluster_valid(fs, start)) {
    trace("no valid start cluster (%d), allocating new one\n", start);
    uint32_t new_cluster = find_free_cluster(fs, 0);
    dirent_to_write->lo_start = new_cluster & 0xFFFF;
    dirent_to_write->hi_start = (new_cluster >> 16) & 0xFFFF;
    fs->fat[new_cluster] = LAST_CLUSTER;  fat_mark_dirty(new_cluster);
  }
  // write file data. if this fails the bytes never reached the card, so we must
  // NOT record a dirent advertising file->n_data bytes (that's how a file ends
  // up the right size but all zeros) -- bail and report failure.
  if (!write_cluster_chain(fs, dirent_to_write->lo_start | (dirent_to_write->hi_start << 16), file->data, file->n_data)) {
    printk("[fat32] write: data write failed for %s, not committing dirent\n", filename);
    write_fat_to_disk(fs);
    return 0;
  }

  // write out directory entries
  if (!write_cluster_chain(fs, directory->cluster_id, (uint8_t *)dirents, dir_n * sizeof(fat32_dirent_t))) {
    printk("[fat32] write: dirent write failed for %s\n", filename);
    write_fat_to_disk(fs);
    return 0;
  }

  // write out fat
  write_fat_to_disk(fs);
  return 1;

}

int fat32_flush(fat32_fs_t *fs) {
  demand(init_p, "fat32 not initialized!");
  // no-op
  return 0;
}
