#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "simplefs.h"

/* Mount a simplefs partition */
struct dentry *simplefs_mount(struct file_system_type *fs_type,
                              int flags,
                              const char *dev_name,
                              void *data)
{
    struct dentry *dentry =
        mount_bdev(fs_type, flags, dev_name, data, simplefs_fill_super);
    if (IS_ERR(dentry))
        pr_err("'%s' mount failure\n", dev_name);
    else
        pr_info("'%s' mount success\n", dev_name);

    return dentry;
}

/* Unmount a simplefs partition */
void simplefs_kill_sb(struct super_block *sb)
{
    struct simplefs_sb_info *sbi = SIMPLEFS_SB(sb);
#if SIMPLEFS_AT_LEAST(6, 9, 0)
    if (sbi->s_journal_bdev_file)
        fput(sbi->s_journal_bdev_file);
#elif SIMPLEFS_AT_LEAST(6, 7, 0)
    if (sbi->s_journal_bdev_handle)
        bdev_release(sbi->s_journal_bdev_handle);
#endif
    kill_block_super(sb);

    pr_info("unmounted disk\n");
}

static struct file_system_type simplefs_file_system_type = {
    .owner = THIS_MODULE,
    .name = "simplefs",
    .mount = simplefs_mount,
    .kill_sb = simplefs_kill_sb,
    .fs_flags = FS_REQUIRES_DEV,
    .next = NULL,
};

static int __init simplefs_init(void)
{
    int ret = simplefs_init_inode_cache();
    if (ret) {
        pr_err("Failed to create inode cache\n");
        goto err;
    }

    ret = register_filesystem(&simplefs_file_system_type);
    if (ret) {
        pr_err("Failed to register file system\n");
        goto err_inode;
    }

    pr_info("module loaded\n");
    return 0;

err_inode:
    simplefs_destroy_inode_cache();
    /* Only after rcu_barrier() is the memory guaranteed to be freed. */
    rcu_barrier();
err:
    return ret;
}

static void __exit simplefs_exit(void)
{
    int ret = unregister_filesystem(&simplefs_file_system_type);
    if (ret)
        pr_err("Failed to unregister file system\n");

    simplefs_destroy_inode_cache();
    /* Only after rcu_barrier() is the memory guaranteed to be freed. */
    rcu_barrier();

    pr_info("module unloaded\n");
}

module_init(simplefs_init);
module_exit(simplefs_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("a simple file system");
