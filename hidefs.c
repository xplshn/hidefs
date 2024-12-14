#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/slab.h>
#include <linux/hashtable.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/path.h>
#include <linux/mount.h>
#include <linux/dirent.h>
#include <linux/kprobes.h>

#define MAX_HIDDEN_PATHS 1024
#define MAX_PATH_LENGTH 4096

MODULE_LICENSE("3BSD"); // This **SOURCE CODE** is 3BSD licensed. You can remove and change this source code in any way you'd like, just be sure to let people know where upstream is located. (or you can choose not to)
MODULE_AUTHOR("xplshn");
MODULE_DESCRIPTION("Hide files across filesystems");
MODULE_VERSION("0.1");

// Hashtable for tracking hidden files/directories
struct hidden_entry {
    char path[MAX_PATH_LENGTH];
    struct hlist_node node;
};

static DEFINE_HASHTABLE(hidden_paths_hash, 10); // 2^10 = 1024 buckets
static DEFINE_MUTEX(hidden_paths_mutex);
static struct kobject *hidefs_kobj;

// Function prototypes
static int register_hidden_path(const char __user *path);
static int unregister_hidden_path(const char __user *path);
static bool is_path_hidden(const char *path);

// Sysfs interface attributes
static ssize_t hide_store(struct kobject *kobj, struct kobj_attribute *attr,
                          const char *buf, size_t count);
static ssize_t unhide_store(struct kobject *kobj, struct kobj_attribute *attr,
                            const char *buf, size_t count);
static ssize_t list_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);

// Sysfs attributes
static struct kobj_attribute hide_attr = __ATTR_WO(hide);
static struct kobj_attribute unhide_attr = __ATTR_WO(unhide);
static struct kobj_attribute list_attr = __ATTR_RO(list);

// Helper function to register a hidden path
static int register_hidden_path(const char __user *user_path)
{
    struct hidden_entry *entry;
    char *kernel_path;

    if (!capable(CAP_SYS_ADMIN))
        return -EPERM;

    kernel_path = kmalloc(MAX_PATH_LENGTH, GFP_KERNEL);
    if (!kernel_path)
        return -ENOMEM;

    if (strncpy_from_user(kernel_path, user_path, MAX_PATH_LENGTH) < 0) {
        kfree(kernel_path);
        return -EFAULT;
    }

    // Check path validity
    struct path path;
    int err = kern_path(kernel_path, LOOKUP_DIRECTORY, &path);
    if (err) {
        kfree(kernel_path);
        return err;
    }
    path_put(&path);

    mutex_lock(&hidden_paths_mutex);

    // Check if path already exists
    hash_for_each_possible(hidden_paths_hash, entry, node,
                            full_name_hash(NULL, kernel_path, strlen(kernel_path))) {
        if (strcmp(entry->path, kernel_path) == 0) {
            mutex_unlock(&hidden_paths_mutex);
            kfree(kernel_path);
            return -EEXIST;
        }
    }

    // Create and insert new entry
    entry = kmalloc(sizeof(struct hidden_entry), GFP_KERNEL);
    if (!entry) {
        mutex_unlock(&hidden_paths_mutex);
        kfree(kernel_path);
        return -ENOMEM;
    }

    strncpy(entry->path, kernel_path, MAX_PATH_LENGTH);
    hash_add(hidden_paths_hash, &entry->node,
             full_name_hash(NULL, kernel_path, strlen(kernel_path)));

    mutex_unlock(&hidden_paths_mutex);
    kfree(kernel_path);

    printk(KERN_INFO "hidefs: Registered hidden path %s\n", entry->path);
    return 0;
}

// Helper function to unregister a hidden path
static int unregister_hidden_path(const char __user *user_path)
{
    struct hidden_entry *entry;
    char *kernel_path;
    unsigned long bkt;

    if (!capable(CAP_SYS_ADMIN))
        return -EPERM;

    kernel_path = kmalloc(MAX_PATH_LENGTH, GFP_KERNEL);
    if (!kernel_path)
        return -ENOMEM;

    if (strncpy_from_user(kernel_path, user_path, MAX_PATH_LENGTH) < 0) {
        kfree(kernel_path);
        return -EFAULT;
    }

    mutex_lock(&hidden_paths_mutex);

    hash_for_each(hidden_paths_hash, bkt, entry, node) {
        if (strcmp(entry->path, kernel_path) == 0) {
            hash_del(&entry->node);
            kfree(entry);
            mutex_unlock(&hidden_paths_mutex);
            kfree(kernel_path);

            printk(KERN_INFO "hidefs: Unregistered hidden path %s\n", kernel_path);
            return 0;
        }
    }

    mutex_unlock(&hidden_paths_mutex);
    kfree(kernel_path);

    return -ENOENT;
}

// Check if a path is hidden
static bool is_path_hidden(const char *path)
{
    struct hidden_entry *entry;
    unsigned long bkt;

    mutex_lock(&hidden_paths_mutex);

    hash_for_each(hidden_paths_hash, bkt, entry, node) {
        if (strstr(path, entry->path) == path) {
            mutex_unlock(&hidden_paths_mutex);
            return true;
        }
    }

    mutex_unlock(&hidden_paths_mutex);
    return false;
}

// Sysfs interface to hide a path
static ssize_t hide_store(struct kobject *kobj, struct kobj_attribute *attr,
                          const char *buf, size_t count)
{
    char *path;
    int ret;

    path = kmalloc(MAX_PATH_LENGTH, GFP_KERNEL);
    if (!path)
        return -ENOMEM;

    if (sscanf(buf, "%4095s", path) != 1) {
        kfree(path);
        return -EINVAL;
    }

    ret = register_hidden_path(path);

    kfree(path);
    return ret < 0 ? ret : count;
}

// Sysfs interface to unhide a path
static ssize_t unhide_store(struct kobject *kobj, struct kobj_attribute *attr,
                            const char *buf, size_t count)
{
    char *path;
    int ret;

    path = kmalloc(MAX_PATH_LENGTH, GFP_KERNEL);
    if (!path)
        return -ENOMEM;

    if (sscanf(buf, "%4095s", path) != 1) {
        kfree(path);
        return -EINVAL;
    }

    ret = unregister_hidden_path(path);

    kfree(path);
    return ret < 0 ? ret : count;
}

// Sysfs interface to list hidden paths
static ssize_t list_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    struct hidden_entry *entry;
    unsigned long bkt;
    ssize_t len = 0;

    mutex_lock(&hidden_paths_mutex);

    hash_for_each(hidden_paths_hash, bkt, entry, node) {
        len += snprintf(buf + len, PAGE_SIZE - len, "%s\n", entry->path);
        if (len >= PAGE_SIZE)
            break;
    }

    mutex_unlock(&hidden_paths_mutex);

    return len;
}

/* Hook into directory listing functions
static int hidefs_filldir(void *__buf, const char *name, int namlen, loff_t offset, u64 ino, unsigned int d_type)
{
    struct dir_context *ctx = __buf;
    if (is_path_hidden(name)) {
        return 0;
    }
    return ctx->actor(__buf, name, namlen, offset, ino, d_type);
} */

// Hook into VFS functions
static struct kprobe kp_rmdir = {
    .symbol_name = "vfs_rmdir",
};

static struct kprobe kp_unlink = {
    .symbol_name = "vfs_unlink",
};

static struct kprobe kp_symlink = {
    .symbol_name = "vfs_symlink",
};

/* static struct kprobe kp_filldir = {
    .symbol_name = "filldir",
}; */

static int kprobe_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
    struct dentry *dentry = (struct dentry *)regs->di;
    if (is_path_hidden(dentry->d_name.name)) {
        printk(KERN_INFO "hidefs: Attempt to operate on hidden file %s\n", dentry->d_name.name);
        return -ENOENT;
    }
    return 0;
}

/* static int kprobe_filldir_handler(struct kprobe *p, struct pt_regs *regs)
{
    void *__buf = (void *)regs->di;
    const char *name = (const char *)regs->si;
    int namlen = (int)regs->dx;
    loff_t offset = (loff_t)regs->cx;
    u64 ino = (u64)regs->r8;
    unsigned int d_type = (unsigned int)regs->r9;

    return hidefs_filldir(__buf, name, namlen, offset, ino, d_type);
} */

// Module initialization
static int __init hidefs_init(void)
{
    int ret;

    // Create sysfs entry
    hidefs_kobj = kobject_create_and_add("hidefs", kernel_kobj);
    if (!hidefs_kobj)
        return -ENOMEM;

    // Create sysfs files
    ret = sysfs_create_file(hidefs_kobj, &hide_attr.attr);
    if (ret)
        goto fail_hide;

    ret = sysfs_create_file(hidefs_kobj, &unhide_attr.attr);
    if (ret)
        goto fail_unhide;

    ret = sysfs_create_file(hidefs_kobj, &list_attr.attr);
    if (ret)
        goto fail_list;

    // Initialize hash table
    hash_init(hidden_paths_hash);

    // Hook VFS functions
    kp_rmdir.pre_handler = kprobe_pre_handler;
    kp_unlink.pre_handler = kprobe_pre_handler;
    kp_symlink.pre_handler = kprobe_pre_handler;
    /* kp_filldir.pre_handler = kprobe_filldir_handler; */

    ret = register_kprobe(&kp_rmdir);
    if (ret < 0)
        goto fail_kprobe;

    ret = register_kprobe(&kp_unlink);
    if (ret < 0)
        goto fail_kprobe;

    ret = register_kprobe(&kp_symlink);
    if (ret < 0)
        goto fail_kprobe;

    /* ret = register_kprobe(&kp_filldir);
    if (ret < 0)
        goto fail_kprobe; */

    printk(KERN_INFO "hidefs: Module loaded successfully\n");
    return 0;

fail_kprobe:
    /* unregister_kprobe(&kp_filldir); */
    unregister_kprobe(&kp_symlink);
    unregister_kprobe(&kp_unlink);
    unregister_kprobe(&kp_rmdir);
fail_list:
    sysfs_remove_file(hidefs_kobj, &unhide_attr.attr);
fail_unhide:
    sysfs_remove_file(hidefs_kobj, &hide_attr.attr);
fail_hide:
    kobject_put(hidefs_kobj);
    return ret;
}

// Module cleanup
static void __exit hidefs_exit(void)
{
    struct hidden_entry *entry;
    struct hlist_node *tmp;
    unsigned long bkt;

    // Remove sysfs files and kobject
    sysfs_remove_file(hidefs_kobj, &hide_attr.attr);
    sysfs_remove_file(hidefs_kobj, &unhide_attr.attr);
    sysfs_remove_file(hidefs_kobj, &list_attr.attr);
    kobject_put(hidefs_kobj);

    // Free all hidden path entries
    hash_for_each_safe(hidden_paths_hash, bkt, tmp, entry, node) {
        hash_del(&entry->node);
        kfree(entry);
    }

    // Unhook VFS functions
    /* unregister_kprobe(&kp_filldir); */
    unregister_kprobe(&kp_symlink);
    unregister_kprobe(&kp_unlink);
    unregister_kprobe(&kp_rmdir);

    printk(KERN_INFO "hidefs: Module unloaded successfully\n");
}

module_init(hidefs_init);
module_exit(hidefs_exit);
