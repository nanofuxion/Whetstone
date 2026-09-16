#include "jailbreak.h"
#include "handoff.h"
#include "utils.h"

uint64_t find_proc_for_pid(pid_t pid) {
    if (pid < 0) return 0;
    if (pid == getpid()) return kinfo->self_proc_addr;
    if (pid == 0) return kinfo->kern_proc_addr;
    uint64_t current_task = kinfo->kern_task_addr;
    pid_t current_pid = -1;
    
    while (current_task != 0) {
        uint64_t current_proc = kread64(current_task + koffsetof(task, bsd_info));
        if (current_proc != 0) current_pid = kread32(current_proc + koffsetof(proc, pid));
        if (current_pid == pid) return current_proc;
        current_task = kread64(current_task + koffsetof(task, next));
        if (current_task == kinfo->kern_task_addr) break;
    }
    return 0;
}

uint64_t find_task_for_pid(pid_t pid) {
    if (pid < 0) return 0;
    if (pid == getpid()) return kinfo->self_task_addr;
    if (pid == 0) return kinfo->kern_task_addr;
    uint64_t current_task = kinfo->kern_task_addr;
    pid_t current_pid = -1;
    
    while (current_task != 0) {
        uint64_t current_proc = kread64(current_task + koffsetof(task, bsd_info));
        if (current_proc != 0) current_pid = kread32(current_proc + koffsetof(proc, pid));
        if (current_pid == pid) return current_task;
        current_task = kread64(current_task + koffsetof(task, next));
        if (current_task == kinfo->kern_task_addr) break;
    }
    return 0;
}

pid_t find_pid_for_name(const char *name) {
    int count = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0) + 100;
    if (count <= 0) return -1;

    pid_t *pids = calloc(1, sizeof(pid_t) * count);
    count = proc_listpids(PROC_ALL_PIDS, 0, pids, sizeof(pid_t) * count);
    if (count <= 0) {
        free(pids);
        return -1;
    }

    char *current_name = calloc(1, PROC_PIDPATHINFO_MAXSIZE+1);
    pid_t target_pid = -1;
    
    for (int i = 0; i < count; i++) {
        bzero(current_name, PROC_PIDPATHINFO_MAXSIZE+1);
        if (proc_name(pids[i], current_name, PROC_PIDPATHINFO_MAXSIZE) <= 0) continue;
        if (strncmp((const char *)current_name, name, PROC_PIDPATHINFO_MAXSIZE) == 0) {
            target_pid = pids[i];
            break;
        }
    }

    free(current_name);
    free(pids);
    return target_pid;
}

uint64_t find_ipc_port(mach_port_t port) {
    if (!MACH_PORT_VALID(port)) return 0;
    uint64_t itk_space = kread64(kinfo->self_task_addr + koffsetof(task, itk_space));
    uint64_t is_table = kread64(itk_space + koffsetof(ipc_space, is_table));
    return kread64(is_table + ((port >> 8) * 0x18));
}

uint64_t vnode_for_fd(int fd) {
    if (fd < 0) return 0;
    uint64_t p_fd = kread64(kinfo->self_proc_addr + koffsetof(proc, p_fd));
    uint64_t fd_ofiles = kread64(p_fd + koffsetof(filedesc, fd_ofiles));
    uint64_t fproc = kread64(fd_ofiles + (fd * 8));
    if (fproc == 0) return 0;
    
    uint64_t f_fglob = kread64(fproc + koffsetof(fileproc, f_fglob));
    return kread64(f_fglob + koffsetof(fileglob, fg_data));
}

uint64_t vnode_for_path(const char *path) {
    if (path == NULL) return 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    
    uint64_t vnode = vnode_for_fd(fd);
    close(fd);
    return vnode;
}

uint64_t get_mac_slot(pid_t pid, int slot) {
    uint64_t proc = 0;
    if (pid == 0) proc = kinfo->kern_proc_addr;
    else proc = find_proc_for_pid(pid);
    
    if (proc == 0) return 0;
    uint64_t ucred = kread64(proc + koffsetof(proc, ucred));
    uint64_t cr_label = kread64(ucred + koffsetof(ucred, cr_label));
    return kread64(cr_label + koffsetof(label, slots) + (slot*0x8));
}

void set_mac_slot(pid_t pid, int slot, uint64_t value) {
    uint64_t proc = find_proc_for_pid(pid);
    if (proc == 0) return;
    uint64_t ucred = kread64(proc + koffsetof(proc, ucred));
    uint64_t cr_label = kread64(ucred + koffsetof(ucred, cr_label));
    kwrite64(cr_label + koffsetof(label, slots) + (slot*0x8), value);
}

uint64_t get_sandbox_profile(pid_t pid) {
    uint64_t sandbox_slot = get_mac_slot(pid, 1);
    if (sandbox_slot == 0) return 1;
    
    uint64_t profile_ptr = kread64(sandbox_slot);
    if (profile_ptr == 0) return 2;
    return kread64(profile_ptr);
}

uint32_t get_task_flags(pid_t pid) {
    uint64_t task = find_task_for_pid(pid);
    if (task == 0) return 0;
    return kread32(task + koffsetof(task, t_flags));
}

void set_task_flags(pid_t pid, int flags) {
    uint64_t task = find_task_for_pid(pid);
    if (task == 0) return;
    kwrite32(task + koffsetof(task, t_flags), flags);
}

void add_task_flag(pid_t pid, int flag) {
    uint64_t task = find_task_for_pid(pid);
    if (task == 0) return;
    uint32_t t_flags = kread32(task + koffsetof(task, t_flags));
    kwrite32(task + koffsetof(task, t_flags), t_flags | flag);
}

void remove_task_flag(pid_t pid, int flag) {
    uint64_t task = find_task_for_pid(pid);
    if (task == 0) return;
    uint32_t t_flags = kread32(task + koffsetof(task, t_flags));
    kwrite32(task + koffsetof(task, t_flags), t_flags & ~flag);
}

uint32_t get_cs_flags(pid_t pid) {
    uint64_t proc = find_proc_for_pid(pid);
    if (proc == 0) return 0;
    return kread32(proc + koffsetof(proc, csflags));
}

void set_cs_flags(pid_t pid, int flags) {
    uint64_t proc = find_proc_for_pid(pid);
    if (proc == 0) return;
    kwrite32(proc + koffsetof(proc, csflags), flags);
}

void add_cs_flag(pid_t pid, int flag) {
    uint64_t proc = find_proc_for_pid(pid);
    if (proc == 0) return;
    uint32_t cs_flags = kread32(proc + koffsetof(proc, csflags));
    kwrite32(proc + koffsetof(proc, csflags), cs_flags | flag);
}

void remove_cs_flag(pid_t pid, int flag) {
    uint64_t proc = find_proc_for_pid(pid);
    if (proc == 0) return;
    uint32_t cs_flags = kread32(proc + koffsetof(proc, csflags));
    kwrite32(proc + koffsetof(proc, csflags), cs_flags & ~flag);
}

void set_proc_flags(pid_t pid, int flags) {
    uint64_t proc = find_proc_for_pid(pid);
    if (proc == 0) return;
    kwrite32(proc + koffsetof(proc, p_flag), flags);
}

void add_proc_flag(pid_t pid, int flag) {
    uint64_t proc = find_proc_for_pid(pid);
    if (proc == 0) return;
    uint32_t proc_flags = kread32(proc + koffsetof(proc, p_flag));
    kwrite32(proc + koffsetof(proc, p_flag), proc_flags | flag);
}

void remove_proc_flag(pid_t pid, int flag) {
    uint64_t proc = find_proc_for_pid(pid);
    if (proc == 0) return;
    uint32_t proc_flags = kread32(proc + koffsetof(proc, p_flag));
    kwrite32(proc + koffsetof(proc, p_flag), proc_flags & ~flag);
}

uint8_t *map_file(const char *path, uint32_t *size, bool write) {
    if (path == NULL) return NULL;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;

    size_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    uint32_t prot = PROT_READ | (write ? PROT_WRITE : PROT_NONE);
    void *data = mmap(NULL, file_size, prot, MAP_PRIVATE, fd, 0);
    close(fd);

    if (data == MAP_FAILED) return NULL;
    *size = (uint32_t)file_size;
    return (uint8_t *)data;
}

void run_with_cred(pid_t pid, void (^action)(void)) {
    uint64_t proc = find_proc_for_pid(pid);
    if (proc == 0) return;
    
    uint64_t target_ucred = kread64(proc + koffsetof(proc, ucred));
    uint64_t self_ucred = kread64(kinfo->self_proc_addr + koffsetof(proc, ucred));
    kwrite64(kinfo->self_proc_addr + koffsetof(proc, ucred), target_ucred);
    
    action();
    kwrite64(kinfo->self_proc_addr + koffsetof(proc, ucred), self_ucred);
}

static mode_t parse_mode(uint8_t *data) {
    char *end = NULL;
    return (mode_t)strtol((char *)data, &end, 8);
}

static int parse_octal(uint8_t *data, size_t size) {
    int octal = 0;
    while ((data[0] < '0' || data[0] > '7') && size > 0) {
        data++;
        size--;
    }
    
    while (data[0] >= '0' && data[0] <= '7' && size > 0) {
        octal *= 8;
        octal += data[0] - '0';
        data++;
        size--;
    }
    return octal;
}

static time_t parse_time(uint8_t *data) {
    size_t size = 12;
    int64_t time = 0;
    while ((data[0] < '0' || data[0] > '7') && size > 0) {
        data++;
        size--;
    }
    
    while (data[0] >= '0' && data[0] <= '7' && size > 0) {
        time *= 8;
        time += data[0] - '0';
        data++;
        size--;
    }
    return (time_t)time;
}

static int set_time(uint8_t *data, time_t mtime, bool is_link) {
    struct timeval val[2] = {0};
    val[0].tv_sec = mtime;
    val[0].tv_usec = (__darwin_suseconds_t)0;
    val[1].tv_sec = mtime;
    val[1].tv_usec = (__darwin_suseconds_t)0;

    if (is_link) return lutimes((char *)data, (void *)&val);
    return utimes((char *)data, (void *)&val);
}

static bool end_of_file(uint8_t *data) {
    for (int i = 511; i >= 0; i--) {
        if (data[i] != '\0') return false;
    }
    return true;
}

static bool verify_checksum(uint8_t *data) {
    int unaligned = 0;
    for (int i = 0; i < 512; i++) {
        if (i < 148 || i > 155) unaligned += data[i];
        else unaligned += 0x20;
    }
    return (unaligned == parse_octal(data + 148, 8));
}

static void create_directory(uint8_t *data, mode_t mode, uid_t uid, gid_t gid) {
    char *path = (char *)data;
    size_t path_size = strlen(path);
    if (path[path_size-1] == '/') path[path_size-1] = '\0';
    
    int rv = -1;
    if (access(path, F_OK) != 0) {
        rv = mkdir(path, mode);
        chown(path, uid, gid);
    }

    if (rv != 0) {
        char *ptr = strrchr(path, '/');
        if (ptr != NULL) {
            ptr[0] = '\0';
            create_directory(data, mode, uid, gid);
            ptr[0] = '/';
            
            if (access(path, F_OK) != 0) {
                mkdir(path, mode);
                chown(path, uid, gid);
            }
        }
    }
}

static FILE *create_file(uint8_t *data, mode_t mode, int owner, int group) {
    char *path = (char *)data;
    FILE *file = fopen(path, "wb+");
    
    if (file == NULL) {
        char *ptr = strrchr(path, '/');
        if (ptr != NULL) {
            ptr[0] = '\0';
            create_directory(data, mode, owner, group);
            ptr[0] = '/';
            file = fopen(path, "wb+");
        }
    }
    return file;
}

static int create_link(uint8_t *data, bool is_symlink) {
    char linkname[100] = {0};
    for (int i = 0; i < 100; i++) linkname[i] = data[157 + i];

    if (is_symlink) {
        if (symlink(linkname, (char *)data) != 0) return -1;

    } else {
        if (link(linkname, (char *)data) != 0) return -1;
    }
    set_time(data, parse_time(data + TAR_MTIME_OFF), 1);
    return 0;
}

int extract_tar(FILE *input, const char *path) {
    if (chdir(path) != 0) return -1;
    uint8_t data[512] = {0};
    FILE *file = NULL;
    size_t bytes_read = 0;
    int file_size = 0;
    char path_name[100] = {0};
    time_t mtime = 0;
    mode_t mode = 0;

    while (true) {
        bytes_read = fread(data, 1, 512, input);
        if (bytes_read < 512) return -1;
        if (end_of_file(data)) return 0;
        if (!verify_checksum(data)) return -1;
        
        file_size = parse_octal(data + 124, 12);
        char entry_type = (char)data[156];
        
        switch (entry_type) {
            case TAR_LNKTYPE: create_link(data, false); break;
            case TAR_SYMTYPE: create_link(data, true); break;
            case TAR_CHRTYPE: break;
            case TAR_BLKTYPE: break;
            case TAR_DIRTYPE:
                mode = parse_mode(data + TAR_OCTAL_OFF);
                create_directory(data, mode, TAR_UID, TAR_GID);
                file_size = 0;
                break;
            case TAR_FIFOTYPE: break;
            default:
                printf("%s\n", (char *)data);
                mtime = parse_time(data + TAR_MTIME_OFF);
                mode = parse_mode(data + TAR_OCTAL_OFF);
                for (int i = 0; i < 100; i++) path_name[i] = data[0 + i];
                file = create_file(data, mode, TAR_UID, TAR_GID);
                chown(path_name, TAR_UID, TAR_GID);
                chmod(path_name, mode);
                break;
        }
        
        while (file_size > 0) {
            bytes_read = fread(data, 1, 512, input);
            if (bytes_read < 512) return -1;
            if (file_size < 512) bytes_read = file_size;
            if (file != NULL) {
                if (fwrite(data, 1, bytes_read, file) != bytes_read) {
                    fclose(file);
                    file = NULL;
                }
            }
            file_size -= bytes_read;
        }
        
        if (file != NULL) {
            fclose(file);
            file = NULL;
        }
    }
    return 0;
}

char *resolve_dir_path(const char *path) {
    if (path == NULL || path[0] == '\0') return NULL;
    size_t path_len = strlen(path);
    if (path_len <= 1 || path_len > PATH_MAX) return NULL;

    char *copy = strdup(path);
    int i = (int)path_len;
    for (; copy[i] != '/' && i > 0; i--);
    
    if (i == 0) {
        free(copy);
        return NULL;
    }
    
    copy[i] = '\0';
    return copy;
}

const char *bundle_path(const char *item) {
    char exec_path[PATH_MAX] = {0};
    uint32_t size = PATH_MAX;
    _NSGetExecutablePath(exec_path, &size);
    
    char *exec_dir = resolve_dir_path(exec_path);
    if (exec_dir == NULL) return NULL;
    
    char path_buf[PATH_MAX] = {0};
    sprintf(path_buf, "%s/%s", exec_dir, item);
    
    free(exec_dir);
    return strdup(path_buf);
}

int remove_at_path(const char *path) {
    if (access(path, F_OK) != 0) return 0;
    struct stat st = {0};
    int rv = 0;

    if (lstat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            DIR *dir = opendir(path);
            if (dir == NULL) return -1;
            struct dirent *entry = NULL;
                
            while (rv == 0 && (entry = readdir(dir))) {
                char *item = (char *)entry->d_name;
                if (strcmp(item, ".") == 0) continue;
                if (strcmp(item, "..") == 0) continue;
                
                size_t path_len = strlen(path) + strlen(item) + 2;
                char *path_buf = calloc(1, path_len);
                if (path_buf == NULL) continue;
                
                bzero(&st, sizeof(struct stat));
                snprintf(path_buf, path_len, "%s/%s", path, item);
                    
                if (lstat(path_buf, &st) == 0) {
                    if (S_ISDIR(st.st_mode)) {
                        rv = remove_at_path(path_buf);
                    } else {
                        rv = unlink(path_buf);
                    }
                }
                free(path_buf);
            }
            
            closedir(dir);
            if (rv == 0) rv = rmdir(path);
            return rv;
        }

        rv = unlink(path);
        if (rv != 0 || access(path, F_OK) == 0) rv = remove(path);
        return rv;
    } else {
        if (unlink(path) == 0) return 0;
        if (remove(path) == 0) return 0;
        if (rmdir(path) == 0) return 0;
    }
    return -1;
}

int copy_file(const char *src, const char *dest) {
    int fd = open(src, O_RDONLY);
    if (fd == -1) return -1;
    
    if (access(dest, R_OK) == 0) {
        unlink(dest);
        sync();
    }
    
    size_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    void *data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (data == MAP_FAILED) return -1;
    
    __block FILE *output = fopen(dest, "wb");
    if (output == NULL) {
        run_with_cred(1, ^{output = fopen(dest, "wb");});
        if (output == NULL) {
            munmap(data, size);
            return -1;
        }
    }

    fwrite(data, size, 1, output);
    munmap(data, size);
    fclose(output);
    sync();
    return access(dest, F_OK);
}

int swap_namecache_vnode(const char *src, const char *dest) {
    int src_fd = -1;
    int dest_fd = -1;
    int status = -1;
    
    if ((src_fd = open(src, O_RDONLY)) < 0) goto done;
    if ((dest_fd = open(dest, O_RDONLY)) < 0) goto done;
    usleep(10000);
    sync();
    
    uint64_t src_vnode = vnode_for_fd(src_fd);
    if (src_vnode == 0) goto done;
    
    uint64_t dest_vnode = vnode_for_fd(dest_fd);
    if (dest_vnode == 0) goto done;

    uint64_t namecache = kread64(dest_vnode + koffsetof(vnode, namecache));
    if (namecache == 0) goto done;
    
    kwrite32(src_vnode + koffsetof(vnode, v_kusecount), kread32(src_vnode + koffsetof(vnode, v_kusecount)) + 10);
    kwrite32(src_vnode + koffsetof(vnode, v_usecount), kread32(src_vnode + koffsetof(vnode, v_usecount)) + 10);
    kwrite32(dest_vnode + koffsetof(vnode, v_kusecount), kread32(dest_vnode + koffsetof(vnode, v_kusecount)) + 10);
    kwrite32(dest_vnode + koffsetof(vnode, v_usecount), kread32(dest_vnode + koffsetof(vnode, v_usecount)) + 10);
    kwrite64(namecache + koffsetof(namecache, vnode), src_vnode);
    
    status = 0;
    usleep(10000);
    sync();
    
done:
    if (src_fd >= 0) close(src_fd);
    if (dest_fd >= 0) close(dest_fd);
    return status;
}

int vnode_hide_path(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    usleep(10000);
    sync();
    
    uint64_t vnode = vnode_for_fd(fd);
    if (vnode == 0) {
        close(fd);
        return -1;
    }
    
    kwrite32(vnode + koffsetof(vnode, v_kusecount), kread32(vnode + koffsetof(vnode, v_kusecount)) + 10);
    kwrite32(vnode + koffsetof(vnode, v_usecount), kread32(vnode + koffsetof(vnode, v_usecount)) + 10);
    kwrite32(vnode + koffsetof(vnode, v_flag), kread32(vnode + koffsetof(vnode, v_flag)) | VISSHADOW);
    usleep(10000);
    sync();
    
    close(fd);
    return 0;
}

int run_binary(char *path, char **args, char **env, bool wait, bool root) {
    pid_t pid = -1;
    int status = 0;
    
    int fds[2] = {-1, -1};
    pipe(fds);
    
    posix_spawn_file_actions_t file_actions = NULL;
    posix_spawn_file_actions_init(&file_actions);
    posix_spawn_file_actions_adddup2(&file_actions, fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&file_actions, fds[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&file_actions, fds[0]);

    posix_spawnattr_t attr = NULL;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_START_SUSPENDED);

    int err = posix_spawn(&pid, args[0], NULL, NULL, args, env);
    posix_spawnattr_destroy(&attr);
    close(fds[1]);

    if (err != 0 || pid == -1) {
        posix_spawn_file_actions_destroy(&file_actions);
        close(fds[0]);
        return -1;
    }
    
    uint64_t proc = find_proc_for_pid(pid);
    uint64_t ucred = kread64(proc + koffsetof(proc, ucred));

    kwrite32(proc + koffsetof(proc, p_svuid), 0);
    kwrite32(ucred + koffsetof(ucred, cr_svuid), 0);
    kwrite32(ucred + koffsetof(ucred, cr_uid), 0);
    kwrite32(proc + koffsetof(proc, p_svgid), 0);
    kwrite32(ucred + koffsetof(ucred, cr_svgid), 0);
    kwrite32(ucred + koffsetof(ucred, cr_groups), 0);
    
    remove_proc_flag(pid, P_SUGID);
    set_mac_slot(pid, 1, 0);
    add_task_flag(pid, TF_PLATFORM);
    add_cs_flag(pid, CS_PLATFORM_PATH|CS_DEBUGGED|CS_INVALID_ALLOWED);
    remove_cs_flag(pid, CS_KILL|CS_HARD|CS_RESTRICT);
    kill(pid, SIGCONT);
    
    if (wait) {
        char output[1024] = {0};
        ssize_t len = 0;
        while ((len = read(fds[0], output, sizeof(output)-1)) > 0) {
            output[len] = '\0';
            printf("%s", output);
        }
        
        posix_spawn_file_actions_destroy(&file_actions);
        close(fds[0]);
        
        do { if (waitpid(pid, &status, 0) == -1) return -1; }
        while (!WIFEXITED(status) && !WIFSIGNALED(status));
        return status;
    } else {
        posix_spawn_file_actions_destroy(&file_actions);
        close(fds[0]);
        return err;
    }
}

int run_jbutil(char *cmd, char *arg, bool wait) {
    char *args[] = {"/amethyst/jbutil", cmd, arg, NULL};
    char *env[] = {"DYLD_INSERT_LIBRARIES=/usr/lib/base_hook.dylib", NULL};
    return run_binary("/amethyst/jbutil", args, env, wait, true);
}

CFMutableDictionaryRef load_plist(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;
    
    size_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    
    void *data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (data == MAP_FAILED) return NULL;
    
    CFDataRef cf_data = CFDataCreateWithBytesNoCopy(NULL, data, size, NULL);
    CFMutableDictionaryRef plist = (CFMutableDictionaryRef)CFPropertyListCreateWithData(NULL, cf_data, kCFPropertyListMutableContainersAndLeaves, NULL, NULL);
    munmap(data, size);
    return plist;
}

int write_plist(const char *path, CFMutableDictionaryRef plist) {
    CFDataRef data = CFPropertyListCreateData(NULL, plist, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
    if (data == NULL) return -1;
    
    unlink(path);
    FILE *file = fopen(path, "wb+");
    if (file == NULL) {
        CFRelease(data);
        return -1;
    }
    
    fwrite(CFDataGetBytePtr(data), CFDataGetLength(data), 1, file);
    fflush(file);
    fclose(file);
    sync();
    return 0;
}
