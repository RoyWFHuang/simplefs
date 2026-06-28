#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

/* ============================================================================
 * Btrfs-style Checksum Support (Error Correction Only, No Replay Protection)
 * ============================================================================
 * Btrfs-style checksums use an 8-byte trailer at block end:
 *   - Bytes 0-1: Checksum type (1=CRC32c, 2=xxHash64)
 *   - Bytes 2-3: Reserved (0)
 *   - Bytes 4-7: Checksum value (32-bit)
 *
 * This implementation supports CRC32c for error correction.
 * No generation counter is used (no replay attack prevention needed).
 */

#include <linux/buffer_head.h>
#include <linux/crc32c.h>
#include <linux/kernel.h>

#include "simplefs.h"

#define SIMPLEFS_NEW_DIR_BLOCK_CSUM 0xcd5db637
#define SIMPLEFS_NEW_EI_BLOCK_CSUM 0xf84b05e3

/* Checksum type constants */
#define SIMPLEFS_CSUM_TYPE_CRC32C 1

static inline struct simplefs_block_csum_trailer *simplefs_block_csum_trailer(
    void *block_data)
{
    return (struct simplefs_block_csum_trailer *) ((char *) block_data +
                                                   SIMPLEFS_CSUM_TAIL_OFFSET);
}

/* Per-bh "csum already verified or freshly written" flag. Set after a
 * successful verify or csum_set, cleared automatically when the buffer is
 * re-read from disk. Lets repeated accesses skip a 4 KiB CRC pass. */
enum {
    BH_SfsCsumOk = BH_PrivateStart,
};
BUFFER_FNS(SfsCsumOk, sfs_csum_ok)

static uint32_t simplefs_ei_block_csum(void *block_data)
{
    return crc32c(~0U, (uint8_t *) block_data,
                  sizeof(struct simplefs_file_ei_block) -
                      sizeof(struct simplefs_block_csum_trailer));
}

int simplefs_ei_block_csum_verify(struct buffer_head *bh)
{
    struct simplefs_block_csum_trailer *trailer;
    uint32_t computed;

    if (buffer_sfs_csum_ok(bh))
        return 1;

    trailer = simplefs_block_csum_trailer(bh->b_data);

    if (trailer->csum_type != SIMPLEFS_CSUM_TYPE_CRC32C) {
        pr_err_ratelimited("simplefs: ei_block invalid csum type %u\n",
                           trailer->csum_type);
        return 0;
    }

    computed = simplefs_ei_block_csum(bh->b_data);
    if (trailer->csum_value != computed) {
        pr_err_ratelimited(
            "simplefs: ei_block checksum mismatch: stored=0x%08x "
            "computed=0x%08x\n",
            trailer->csum_value, computed);
        return 0;
    }
    set_buffer_sfs_csum_ok(bh);
    return 1;
}

void simplefs_ei_block_csum_set(struct buffer_head *bh)
{
    struct simplefs_block_csum_trailer *trailer =
        simplefs_block_csum_trailer(bh->b_data);
    trailer->csum_type = SIMPLEFS_CSUM_TYPE_CRC32C;
    trailer->csum_reserved = 0;
    trailer->csum_value = simplefs_ei_block_csum(bh->b_data);
    set_buffer_sfs_csum_ok(bh);
}

/* --- dir_block (directory block) checksum --- */

static uint32_t simplefs_dir_block_csum(void *block_data)
{
    return crc32c(~0U, (uint8_t *) block_data,
                  sizeof(struct simplefs_dir_block) -
                      sizeof(struct simplefs_block_csum_trailer));
}

int simplefs_dir_block_csum_verify(struct buffer_head *bh)
{
    struct simplefs_block_csum_trailer *trailer;
    uint32_t computed;

    if (buffer_sfs_csum_ok(bh))
        return 1;

    trailer = simplefs_block_csum_trailer(bh->b_data);

    if (trailer->csum_type != SIMPLEFS_CSUM_TYPE_CRC32C) {
        pr_err_ratelimited("simplefs: dir_block invalid csum type %u\n",
                           trailer->csum_type);
        return 0;
    }

    computed = simplefs_dir_block_csum(bh->b_data);
    if (trailer->csum_value != computed) {
        pr_err_ratelimited(
            "simplefs: dir_block checksum mismatch: stored=0x%08x "
            "computed=0x%08x\n",
            trailer->csum_value, computed);
        return 0;
    }
    set_buffer_sfs_csum_ok(bh);
    return 1;
}

void simplefs_dir_block_csum_set(struct buffer_head *bh)
{
    struct simplefs_block_csum_trailer *trailer =
        simplefs_block_csum_trailer(bh->b_data);
    trailer->csum_type = SIMPLEFS_CSUM_TYPE_CRC32C;
    trailer->csum_reserved = 0;
    trailer->csum_value = simplefs_dir_block_csum(bh->b_data);
    set_buffer_sfs_csum_ok(bh);
}

/* --- Precomputed CRCs for freshly-zeroed blocks ---
 *
 * Both structs are padded so csum_trailer sits in the last 8 bytes of the
 * block; the hashed region is sizeof(struct) - sizeof(trailer) =
 * SIMPLEFS_CSUM_TAIL_OFFSET (4088) bytes for both block types.
 *
 * - SIMPLEFS_NEW_DIR_BLOCK_CSUM: dir block where every hashed byte is 0
 *   except files[0].nr_blk = SIMPLEFS_FILES_PER_BLOCK.
 * - SIMPLEFS_NEW_EI_BLOCK_CSUM: ei block where every hashed byte is 0.
 */

void simplefs_set_new_dir_block_csum(struct buffer_head *bh)
{
    struct simplefs_block_csum_trailer *t =
        simplefs_block_csum_trailer(bh->b_data);
    t->csum_type = SIMPLEFS_CSUM_TYPE_CRC32C;
    t->csum_reserved = 0;
    t->csum_value = SIMPLEFS_NEW_DIR_BLOCK_CSUM;
    set_buffer_sfs_csum_ok(bh);
}

void simplefs_set_new_ei_block_csum(struct buffer_head *bh)
{
    struct simplefs_block_csum_trailer *t =
        simplefs_block_csum_trailer(bh->b_data);
    t->csum_type = SIMPLEFS_CSUM_TYPE_CRC32C;
    t->csum_reserved = 0;
    t->csum_value = SIMPLEFS_NEW_EI_BLOCK_CSUM;
    set_buffer_sfs_csum_ok(bh);
}
