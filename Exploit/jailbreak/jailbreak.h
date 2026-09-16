#ifndef jailbreak_h
#define jailbreak_h

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <mach/mach.h>
#include "memory.h"

#define JB_FLAG_NONE                0x00000000
#define JB_FLAG_EXPLOIT_HEMLOCK     0x00000001
#define JB_FLAG_EXPLOIT_TRIGON      0x00000002
#define JB_FLAG_RESTORE_ROOTFS      0x00000004
#define JB_FLAG_ENABLE_TWEAKS       0x00000008
#define JB_FLAG_INSTALL_SILEO       0x00000010
#define JB_FLAG_INSTALL_ZEBRA       0x00000020
#define JB_FLAG_INSTALL_LIBSWIFT    0x00000040

#define koffsetof(struct, entry) kinfo->offsets.struct.entry

typedef struct {
    uint64_t self_task_addr;
    uint64_t self_proc_addr;
    uint64_t self_port_addr;
    uint64_t kern_task_addr;
    uint64_t kern_proc_addr;
    uint64_t host_port_addr;
    uint64_t realhost_addr;
    uint64_t ipc_space_kernel;
    uint64_t kernel_base;
    uint64_t kernel_slide;
    uint32_t page_size;
    mach_port_t host_priv;
    uint64_t cpu_ttep;
    mach_port_t tfp0;
    uint64_t self_ttep;
    bool tnsv2_supported;
    uint32_t version[3];
    
    struct {
        bool kpp;
        bool ktrr;
        bool pan;
        bool pac;
        bool ppl;
    } protections;
    
    struct {
        struct {
            uint32_t bsd_info;
            uint32_t vm_map;
            uint32_t next;
            uint32_t itk_space;
            uint32_t t_flags;
        } task;
        
        struct {
            uint32_t pid;
            uint32_t p_fd;
            uint32_t p_flag;
            uint32_t p_svuid;
            uint32_t p_svgid;
            uint32_t ucred;
            uint32_t csflags;
            uint32_t p_textvp;
        } proc;
        
        struct {
            uint32_t cr_uid;
            uint32_t cr_ruid;
            uint32_t cr_svuid;
            uint32_t cr_groups;
            uint32_t cr_rgid;
            uint32_t cr_svgid;
            uint32_t cr_label;
        } ucred;
        
        struct {
            uint32_t slots;
        } label;
        
        struct {
            uint32_t fd_ofiles;
        } filedesc;
        
        struct {
            uint32_t f_fglob;
        } fileproc;
        
        struct {
            uint32_t fg_data;
        } fileglob;
        
        struct {
            uint32_t ip_bits;
            uint32_t ip_receiver;
            uint32_t ip_kobject;
        } ipc_port;
        
        struct {
            uint32_t is_table_size;
            uint32_t is_table;
        } ipc_space;
        
        struct {
            uint32_t namecache;
            uint32_t v_flag;
            uint32_t v_kusecount;
            uint32_t v_usecount;
            uint32_t v_name;
            uint32_t v_parent;
            uint32_t v_mount;
            uint32_t v_data;
            uint32_t v_data_flag;
            uint32_t specinfo;
            uint32_t ubcinfo;
        } vnode;

        struct {
            uint32_t flags;
        } specinfo;

        struct {
            uint32_t cs_blob;
        } ubc_info;
        
        struct {
            uint32_t vnode;
        } namecache;
        
        struct {
            uint32_t mnt_next;
            uint32_t vnodelist;
            uint32_t flag;
            uint32_t devvp;
        } mount;
        
        struct {
            int pmap;
        } vm_map;
        
        struct {
            int ttep;
            int cs_enforced;
        } pmap;

        struct {
            int trust;
        } pmap_cs_code_directory;
    } offsets;
    
    struct {
        uint64_t dynamic_trustcache;
        uint64_t static_trustcache;
        uint64_t ptov_table;
        uint64_t gVirtBase;
        uint64_t gPhysBase;
        uint64_t gPhysSize;
        uint64_t pplrw_entry;
        uint64_t pplrw_mapping_va;
        uint64_t pplrw_mapping_pa;
    } patches;
} kinfo_t;

typedef enum {
    JB_ERROR_UNKNOWN = -1,
    JB_ERROR_SUCCESS,
    JB_ERROR_EXPLOIT,
    JB_ERROR_PERMISSION,
    JB_ERROR_PATCHFINDER,
    JB_ERROR_TRUSTCACHE,
    JB_ERROR_PPL,
    JB_ERROR_REMOUNT,
    JB_ERROR_RESTORE,
    JB_ERROR_OTA,
    JB_ERROR_REMOUNT_REBOOT,
    JB_ERROR_RESTORE_REBOOT,
    JB_ERROR_HANDOFF,
    JB_ERROR_DYLD,
    JB_ERROR_BOOTSTRAP,
    JB_ERROR_SILEO,
    JB_ERROR_ZEBRA,
    JB_ERROR_LIBSWIFT,
    JB_ERROR_UNSUPPORTED_INSTALL
} jb_error_t;

extern kinfo_t *kinfo;
extern void ProgressLog(float progress, const char *fmt, ...);
void FadeDisplay(void);
extern void ShowAlert(const char *title, const char *msg, const char *btn, void (^action)(void));
extern uint32_t get_install_options(void);
extern uint64_t trigon_cache_get_u64(char *key);
extern int trigon_cache_set_u64(char *key, uint64_t value);
extern bool trigon_cache_get_bool(char *key);
extern int trigon_cache_set_bool(char *key, bool value);
extern void trigon_cache_sync(void);

jb_error_t run_jailbreak(uint32_t flags, char *generator);

#endif /* jailbreak_h */
