#ifndef utils_h
#define utils_h

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <mach/mach.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/swap.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <dirent.h>
#include <libgen.h>
#include <spawn.h>
#include <CoreFoundation/CoreFoundation.h>

#define CS_VALID                                0x00000001
#define CS_ADHOC                                0x00000002
#define CS_GET_TASK_ALLOW                       0x00000004
#define CS_INSTALLER                            0x00000008
#define CS_FORCED_LV                            0x00000010
#define CS_INVALID_ALLOWED                      0x00000020
#define CS_HARD                                 0x00000100
#define CS_KILL                                 0x00000200
#define CS_CHECK_EXPIRATION                     0x00000400
#define CS_RESTRICT                             0x00000800
#define CS_ENFORCEMENT                          0x00001000
#define CS_REQUIRE_LV                           0x00002000
#define CS_ENTITLEMENTS_VALIDATED               0x00004000
#define CS_NVRAM_UNRESTRICTED                   0x00008000
#define CS_RUNTIME                              0x00010000
#define CS_EXEC_SET_HARD                        0x00100000
#define CS_EXEC_SET_KILL                        0x00200000
#define CS_EXEC_SET_ENFORCEMENT                 0x00400000
#define CS_EXEC_INHERIT_SIP                     0x00800000
#define CS_KILLED                               0x01000000
#define CS_DYLD_PLATFORM                        0x02000000
#define CS_PLATFORM_BINARY                      0x04000000
#define CS_PLATFORM_PATH                        0x08000000
#define CS_DEBUGGED                             0x10000000
#define CS_SIGNED                               0x20000000
#define CS_DEV_CODE                             0x40000000
#define CS_DATAVAULT_CONTROLLER                 0x80000000
#define CS_EXECSEG_MAIN_BINARY                  0x00000001
#define CS_EXECSEG_ALLOW_UNSIGNED               0x00000010
#define CS_EXECSEG_DEBUGGER                     0x00000020
#define CS_EXECSEG_JIT                          0x00000040
#define CS_EXECSEG_SKIP_LV                      0x00000080
#define CS_EXECSEG_CAN_LOAD_CDHASH              0x00000100
#define CS_EXECSEG_CAN_EXEC_CDHASH              0x00000200

#define TF_PLATFORM                             0x00000400
#define CS_OPS_STATUS                           0x00000000
#define P_SUGID                                 0x00000100
#define VISSHADOW                               0x00008000

#define IKOT_HOST_PRIV 0x00000004
#define IO_BITS_ACTIVE 0x80000000
#define IE_BITS_SEND (1<<16)
#define IE_BITS_RECEIVE (1<<17)

#define PROC_PIDPATHINFO            11
#define PROC_PIDPATHINFO_SIZE       1024
#define PROC_PIDPATHINFO_MAXSIZE    (4*1024)
#define PROC_ALL_PIDS               1

#define TAR_REGTYPE  '0'
#define TAR_AREGTYPE '\0'
#define TAR_LNKTYPE  '1'
#define TAR_SYMTYPE  '2'
#define TAR_CHRTYPE  '3'
#define TAR_BLKTYPE  '4'
#define TAR_DIRTYPE  '5'
#define TAR_FIFOTYPE '6'

#define TAR_NAME_OFF 0
#define TAR_MODE_OFF 100
#define TAR_UID_OFF 108
#define TAR_GID_OFF 116
#define TAR_TYPEFLAG_OFF 156
#define TAR_LINKNAME_OFF 157
#define TAR_OCTAL_OFF 103
#define TAR_MTIME_OFF 136

#define TAR_UID parse_octal(data + TAR_UID_OFF, 8)
#define TAR_GID parse_octal(data + TAR_GID_OFF, 8)
#define TAR_MODE parse_mode(data + TAR_OCTAL_OFF, &mode)

#define is_valid_macho(magic) \
    (magic == MH_MAGIC_64 || \
     magic == MH_CIGAM_64 || \
     magic == MH_MAGIC || \
     magic == MH_CIGAM || \
     magic == FAT_MAGIC_64 || \
     magic == FAT_CIGAM_64 || \
     magic == FAT_MAGIC || \
     magic == FAT_CIGAM)

#define need_swap_macho(magic) \
    (magic == MH_CIGAM_64 || \
     magic == MH_CIGAM || \
     magic == FAT_CIGAM_64 || \
     magic == FAT_CIGAM)

#define is_macho_fat(magic) \
    (magic == FAT_MAGIC_64 || \
     magic == FAT_CIGAM_64 || \
     magic == FAT_MAGIC || \
     magic == FAT_CIGAM)

#define is_macho_32bit(magic) \
    (magic == MH_MAGIC || \
     magic == MH_CIGAM)

#define is_dylib_lc(cmd) \
    (cmd == LC_LOAD_DYLIB || \
     cmd == LC_LOAD_WEAK_DYLIB || \
     cmd == LC_REEXPORT_DYLIB || \
     cmd == LC_LOAD_UPWARD_DYLIB)

typedef struct {
    uint8_t *file_data;
    uint32_t file_size;
    uint32_t slice_offset;
    uint32_t slice_size;
    struct mach_header_64 *hdr;
} macho_mapping_t;

extern mach_port_t IOServiceGetMatchingService(mach_port_t, CFDictionaryRef);
extern mach_port_t IORegistryEntryFromPath(mach_port_t, const char[512]);
extern kern_return_t IORegistryEntryGetProperty(mach_port_t, const char[512], const char[4096], uint32_t *);
extern int IORegistryEntrySetCFProperties(mach_port_t entry, CFTypeRef properties);
extern int IORegistryEntryCreateCFProperties(mach_port_t, CFMutableDictionaryRef *, void *, int);
extern int IOServiceOpen(mach_port_t, mach_port_t, uint32_t, mach_port_t *);
extern int IOConnectCallStructMethod(mach_port_t, uint32_t, void *, size_t, void *, size_t *);
extern CFMutableDictionaryRef IOServiceMatching(const char *name);
extern void *IOSurfaceCreate(CFDictionaryRef properties);
extern void *IOSurfaceGetBaseAddress(void *surface);
extern kern_return_t IOObjectRelease(mach_port_t);
extern CFDictionaryRef _CFCopySystemVersionDictionary(void);
extern int proc_listpids(uint32_t type, uint32_t typeinfo, void *buffer, int buffersize);
extern int proc_name(int pid, void * buffer, uint32_t buffersize);

uint64_t find_proc_for_pid(pid_t pid);
uint64_t find_task_for_pid(pid_t pid);
pid_t find_pid_for_name(const char *name);
uint64_t find_ipc_port(mach_port_t port);
uint64_t vnode_for_fd(int fd);
uint64_t vnode_for_path(const char *path);
uint64_t get_mac_slot(pid_t pid, int slot);
void set_mac_slot(pid_t pid, int slot, uint64_t value);
uint64_t get_sandbox_profile(pid_t pid);
uint32_t get_task_flags(pid_t pid);
void set_task_flags(pid_t pid, int flags);
void add_task_flag(pid_t pid, int flag);
void remove_task_flag(pid_t pid, int flag);
uint32_t get_cs_flags(pid_t pid);
void set_cs_flags(pid_t pid, int flags);
void add_cs_flag(pid_t pid, int flag);
void remove_cs_flag(pid_t pid, int flag);
void set_proc_flags(pid_t pid, int flags);
void add_proc_flag(pid_t pid, int flag);
void remove_proc_flag(pid_t pid, int flag);
uint8_t *map_file(const char *path, uint32_t *size, bool write);
void run_with_cred(pid_t pid, void (^action)(void));
int extract_tar(FILE *input, const char *path);
char *resolve_dir_path(const char *path);
const char *bundle_path(const char *item);
int remove_at_path(const char *path);
int copy_file(const char *src, const char *dest);
int swap_namecache_vnode(const char *src, const char *dest);
int vnode_hide_path(const char *path);
int run_binary(char *path, char **args, char **env, bool wait, bool root);
int run_jbutil(char *cmd, char *arg, bool wait);
CFMutableDictionaryRef load_plist(const char *path);
int write_plist(const char *path, CFMutableDictionaryRef plist);

#endif /* utils_h */
