#ifndef _FS_NTFSAVM_H
#define _FS_NTFSAVM_H

#include <linux/version.h>
#include <linux/err.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
static inline long IS_ERR_OR_NULL(const void *ptr)
{
	return !ptr || IS_ERR_VALUE((unsigned long)ptr);
}
#endif

#define ANTFS_LOGLEVEL_EMER	    0
#define ANTFS_LOGLEVEL_CRIT	    1
#define ANTFS_LOGLEVEL_ERR	    2
#define ANTFS_LOGLEVEL_ERR_EXT      3
#define ANTFS_LOGLEVEL_WARN	    4
#define ANTFS_LOGLEVEL_INFO	    5
#define ANTFS_LOGLEVEL_DBG	    6

/**
 * Make sure to set loglevel to at least 'ANTFS_LOGLEVEL_CRIT' if you want to
 * have debug information in a specific file.
 */
#ifndef ANTFS_LOGLEVEL
#	define ANTFS_LOGLEVEL	    ANTFS_LOGLEVEL_DEFAULT
#endif

#if (ANTFS_LOGLEVEL >= ANTFS_LOGLEVEL_ERR_EXT)
#define antfs_pr_err(fmt, ...) pr_err(fmt, ##__VA_ARGS__)
#define antfs_pr_warn(fmt, ...) pr_warn(fmt, ##__VA_ARGS__)
#define antfs_pr_info(fmt, ...) pr_info(fmt, ##__VA_ARGS__)
#else
#ifdef pr_err_ratelimited
#include <linux/ratelimit.h>
#define antfs_pr_err(fmt, ...) \
	pr_err_ratelimited(fmt, ##__VA_ARGS__)
#define antfs_pr_warn(fmt, ...) \
	pr_warn_ratelimited(fmt, ##__VA_ARGS__)
#define antfs_pr_info(fmt, ...) \
	pr_info_ratelimited(fmt, ##__VA_ARGS__)

#else
#define antfs_pr_err(fmt, ...) \
	do { \
		if (printk_ratelimit()) \
			pr_err(fmt, ##__VA_ARGS__); \
	} while (0)
#define antfs_pr_warn(fmt, ...) \
	do { \
		if (printk_ratelimit()) \
			pr_warn(fmt, ##__VA_ARGS__); \
	} while (0)
#define antfs_pr_info(fmt, ...) \
	do { \
		if (printk_ratelimit()) \
			pr_info(fmt, ##__VA_ARGS__); \
	} while (0)
#define antfs_pr_debug(fmt, ...) \
	do { \
		if (printk_ratelimit()) \
			pr_debug(fmt, ##__VA_ARGS__); \
	} while (0)
#endif
#endif

#if (ANTFS_LOGLEVEL >= ANTFS_LOGLEVEL_CRIT)
#define antfs_log_critical(fmt, ...) \
	pr_err("[%s] <CRITICAL> " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define antfs_log_critical(fmt, ...)
#endif
#if (ANTFS_LOGLEVEL >= ANTFS_LOGLEVEL_ERR_EXT)
#define antfs_log_error_ext(fmt, ...) \
	do { \
		pr_err("[%s] <ERROR> " fmt "\n", __func__, \
			##__VA_ARGS__); \
		dump_stack(); \
	} while (0)
#elif (ANTFS_LOGLEVEL == ANTFS_LOGLEVEL_ERR)
#define antfs_log_error_ext(fmt, ...) \
	antfs_pr_err("[%s] <ERROR> " fmt "\n", __func__, \
			##__VA_ARGS__)
#else
#define antfs_log_error_ext(fmt, ...)
#endif
#if (ANTFS_LOGLEVEL >= ANTFS_LOGLEVEL_ERR)
#define antfs_log_error(fmt, ...) \
	antfs_pr_err("[%s] <ERROR> " fmt "\n", __func__, \
			##__VA_ARGS__)
#else
#define antfs_log_error(fmt, ...)
#endif
#if (ANTFS_LOGLEVEL >= ANTFS_LOGLEVEL_WARN)
#define antfs_log_warning(fmt, ...) \
	antfs_pr_warn("[%s] <WARNING> " fmt "\n", __func__, \
			##__VA_ARGS__)
#else
#define antfs_log_warning(fmt, ...)
#endif
#if (ANTFS_LOGLEVEL >= ANTFS_LOGLEVEL_INFO)
#define antfs_log_info(fmt, ...) \
	pr_info("[%s] <INFO> " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define antfs_log_info(fmt, ...)
#endif
#if (ANTFS_LOGLEVEL >= ANTFS_LOGLEVEL_DBG)
#define antfs_log_debug(fmt, ...) \
	pr_debug("[%s] <DEBUG> " fmt "\n", __func__, ##__VA_ARGS__)
#define antfs_debug(fmt, ...)	\
	pr_debug("[%s] <DEBUG> " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define antfs_log_debug(fmt, ...)
#define antfs_debug(fmt, ...)
#endif
#if (ANTFS_LOGLEVEL >= ANTFS_LOGLEVEL_DBG)
#define antfs_log_enter(fmt, ...) \
	pr_debug("[%s] (%pS) --> " fmt "\n", __func__,\
	       __builtin_return_address(0), ##__VA_ARGS__)
#else
#define antfs_log_enter(fmt, ...)
#endif
#if (ANTFS_LOGLEVEL >= ANTFS_LOGLEVEL_DBG)
#define antfs_log_leave(fmt, ...) \
	pr_debug("[%s] (%pS) <-- " fmt "\n\n", __func__,\
	       __builtin_return_address(0), ##__VA_ARGS__)
#else
#define antfs_log_leave(fmt, ...)
#endif

/*--- #define ANTFS_EARLY_BLALLOC ---*/

#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/backing-dev.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/rbtree.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/buffer_head.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37)
#include <linux/kernel.h>
#else
#include <linux/printk.h>
#endif

#include "inode.h"
#include "lcnalloc.h"

/** Do not allocate MFT records smaller than 24. They are special... */
#define RESERVED_MFT_RECORDS   24
/** If we need to extend MFT $DATA, allocate 1 << this number of MFT records at
 *  once. This is to reduce MFT fragmentation.
 *
 *  Note: ntfs-3g hardcoded 1 << 4 for this purpose in
 *        @ntfs_mft_data_extend_allocation.
 */
#define MFT_DATA_BURST_ALLOC_SHIFT   7

#define IN_KERNEL_VERSION(a, b) ( \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(a, b, 0)) && \
	(LINUX_VERSION_CODE < KERNEL_VERSION(a, b, 256)))

#define ANTFS_DIRENT_ALIGN(x) (((x) + sizeof(__u64) - 1) & ~(sizeof(__u64) - 1))

enum {
	ATIME_ENABLED,
	ATIME_DISABLED,
	ATIME_RELATIVE
};

enum antfs_bh_state_bits {
	/** Marks a block that replaces a hole. */
	BH_Zeronew = BH_PrivateStart,
};

BUFFER_FNS(Zeronew, zeronew)
TAS_BUFFER_FNS(Zeronew, zeronew)

struct antfs_dirent {
	unsigned long ino;
	unsigned int type;
	unsigned long name_len;
	unsigned long offset;
	char *name[0];
};

/**
 * ni->ni_lock mutex nesting subclasses for the lock validator:
 *
 * 0: Target of current operation
 * 1: parent inode
 * 2: another parent inode (e.g. for antfs_rename)
 *
 * Locking order is normal -> parent -> parent2
 */
enum antfs_inode_mutex_lock_class {
	NI_MUTEX_NORMAL,
	NI_MUTEX_PARENT,
	NI_MUTEX_PARENT2
};

struct antfs_filler {
	int (*filldir) (void *buf, const char *name, int name_len,
			loff_t offset, u64 ino, unsigned int d_type);
	struct dir_context *ctx;
	struct file *file;
	void *buffer;
};

struct antfs_sb_info {
	struct ntfs_volume *vol;	/* NTFS volume structure */
	struct super_block *sb;	/* Super block */
	const char *dev;	/* Device name */
	char *mnt_point;	/* Absolute path to mount point */

	/* Mount flags */
	unsigned int atime;	/* 0:ENABLED, 1:DISABLED, 2:RELATIVE */
	unsigned char silent;
	unsigned char recover;
	unsigned char blkdev;
	unsigned char ro;
	unsigned char hiberfile;
	unsigned char utf8;
	unsigned short umask;
	unsigned int uid;
	unsigned int gid;
	unsigned char inherit;	/* for permission checking */
	struct SECURITY_CONTEXT *security;
	char *usermap_path;
};

struct antfs_inode_info {
	struct inode inode;
	struct ntfs_inode ni;
	struct ntfs_attr na;
};

static inline struct antfs_sb_info *ANTFS_SB(struct super_block *sb)
{
	return (struct antfs_sb_info *)sb->s_fs_info;
}

static inline struct ntfs_inode *ANTFS_NI(struct inode *inode)
{
	return &(container_of(inode, struct antfs_inode_info, inode)->ni);
}

static inline struct inode *ANTFS_I(struct ntfs_inode *ni)
{
	return &(container_of(ni, struct antfs_inode_info, ni)->inode);
}

static inline struct ntfs_attr *ANTFS_NA(struct ntfs_inode *ni)
{
	return &(container_of(ni, struct antfs_inode_info, ni)->na);
}
/**
 * Initialize ANTFS
 */
extern struct kmem_cache *antfs_inode_cachep;

/**
 * ANTFS functions
 */
void antfs_fill_super_operations(struct super_block *sb);

void antfs_fill_dentry_operations(struct super_block *sb);

int antfs_inode_setup_root(struct super_block *sb);

void antfs_inode_init_common(struct inode *inode);

void antfs_inode_init_dir(struct inode *inode);

void antfs_inode_init_file(struct inode *inode);

void antfs_inode_init_symlink(struct inode *inode);

void antfs_parse_options(struct antfs_sb_info *sbi, char *data);

int antfs_inode_init(struct inode *inode);

void antfs_sbi_destroy(struct antfs_sb_info *sbi);

int antfs_set_mountpoint(struct antfs_sb_info *sbi);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
int antfs_fsync(struct file *filp, struct dentry *dentry, int datasync);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3, 1, 0)
int antfs_fsync(struct file *filp, int datasync);
#else
int antfs_fsync(struct file *filp, loff_t start, loff_t end, int datasync);
#endif

int antfs_get_block(struct inode *inode, sector_t iblock,
			   struct buffer_head *bh_result, int create);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0)
#define file_inode(file) ((file)->f_path.dentry->d_inode)
#endif


#if KERNEL_VERSION(3, 2, 0) > LINUX_VERSION_CODE
static inline void set_nlink(struct inode *inode, unsigned int nlink)
{
	inode->i_nlink = nlink;
}
#endif

#endif /* _FS_NTFSAVM_H */
