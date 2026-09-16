#ifndef memory_h
#define memory_h

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <mach-o/loader.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>

#define ARM_TTE_VALID               0x0000000000000001ull
#define ARM_TTE_TYPE_MASK           0x0000000000000002ull
#define ARM_TTE_TYPE_TABLE          0x0000000000000002ull
#define ARM_TTE_TYPE_BLOCK          0x0000000000000000ull
#define ARM_TTE_TABLE_MASK          0x0000fffffffff000ULL
#define ARM_TTE_PA_MASK             0x0000fffffffff000ull
#define ARM_TTE_TYPE_L3BLOCK        0x0000000000000002ull
#define PMAP_TT_L0_LEVEL            0x0
#define PMAP_TT_L1_LEVEL            0x1
#define PMAP_TT_L2_LEVEL            0x2
#define PMAP_TT_L3_LEVEL            0x3

#define ARM_16K_TT_L0_SIZE          0x0000800000000000ull
#define ARM_16K_TT_L0_OFFMASK       0x00007fffffffffffull
#define ARM_16K_TT_L0_SHIFT         47
#define ARM_16K_TT_L0_INDEX_MASK    0x0000800000000000ull
#define ARM_16K_TT_L1_SIZE          0x0000001000000000ull
#define ARM_16K_TT_L1_OFFMASK       0x0000000fffffffffull
#define ARM_16K_TT_L1_SHIFT         36
#define ARM_16K_TT_L1_INDEX_MASK    0x0000007000000000ull
#define ARM_16K_TT_L2_SIZE          0x0000000002000000ull
#define ARM_16K_TT_L2_OFFMASK       0x0000000001ffffffull
#define ARM_16K_TT_L2_SHIFT         25
#define ARM_16K_TT_L2_INDEX_MASK    0x0000000ffe000000ull
#define ARM_16K_TT_L3_SIZE          0x0000000000004000ull
#define ARM_16K_TT_L3_OFFMASK       0x0000000000003fffull
#define ARM_16K_TT_L3_SHIFT         14
#define ARM_16K_TT_L3_INDEX_MASK    0x0000000001ffc000ull

#define ARM_4K_TT_L0_SIZE           0x0000008000000000ULL
#define ARM_4K_TT_L0_OFFMASK        0x0000007fffffffffULL
#define ARM_4K_TT_L0_SHIFT          39
#define ARM_4K_TT_L0_INDEX_MASK     0x0000ff8000000000ULL
#define ARM_4K_TT_L1_SIZE           0x0000000040000000ULL
#define ARM_4K_TT_L1_OFFMASK        0x000000003fffffffULL
#define ARM_4K_TT_L1_SHIFT          30
#define ARM_4K_TT_L1_INDEX_MASK     0x0000003fc0000000ULL
#define ARM_4K_TT_L2_SIZE           0x0000000000200000ULL
#define ARM_4K_TT_L2_OFFMASK        0x00000000001fffffULL
#define ARM_4K_TT_L2_SHIFT          21
#define ARM_4K_TT_L2_INDEX_MASK     0x000000003fe00000ULL
#define ARM_4K_TT_L3_SIZE           0x0000000000001000ULL
#define ARM_4K_TT_L3_OFFMASK        0x0000000000000fffULL
#define ARM_4K_TT_L3_SHIFT          12
#define ARM_4K_TT_L3_INDEX_MASK     0x00000000001ff000ULL

typedef struct {
    uint64_t off_mask;
    uint64_t shift;
    uint64_t index_mask;
    uint64_t valid_mask;
    uint64_t type_mask;
    uint64_t type_block;
} tt_level_t;

extern kern_return_t mach_vm_read_overwrite(mach_port_t, mach_vm_address_t, mach_vm_size_t, mach_vm_address_t, mach_vm_size_t *);
extern kern_return_t mach_vm_allocate(mach_port_t, mach_vm_address_t *, mach_vm_size_t, int);
extern kern_return_t mach_vm_deallocate(mach_port_t, mach_vm_address_t, mach_vm_size_t);
extern kern_return_t mach_vm_write(mach_port_t, mach_vm_address_t, vm_offset_t, mach_msg_type_number_t);
extern kern_return_t mach_vm_protect(mach_port_t, mach_vm_address_t, mach_vm_size_t, boolean_t, vm_prot_t);
extern kern_return_t mach_vm_remap(mach_port_t, mach_vm_address_t *, mach_vm_size_t, mach_vm_offset_t, int, mach_port_t, mach_vm_address_t, boolean_t, vm_prot_t *, vm_prot_t *, vm_inherit_t);
extern kern_return_t mach_vm_wire(host_priv_t, mach_port_t, mach_vm_address_t, mach_vm_size_t, vm_prot_t);
extern kern_return_t mach_vm_region(mach_port_t, uint64_t *, uint64_t *, uint64_t, void *, uint64_t *, mach_port_t *);
extern kern_return_t mach_vm_map(vm_map_t, mach_vm_address_t *, mach_vm_size_t, mach_vm_offset_t, int, mem_entry_name_port_t, memory_object_offset_t, boolean_t, vm_prot_t, vm_prot_t, vm_inherit_t);
extern kern_return_t mach_vm_msync(vm_map_t, mach_vm_address_t, mach_vm_size_t, vm_sync_t);

void kread_buf(uint64_t addr, void *data, uint32_t size);
void kwrite_buf(uint64_t addr, void *data, uint32_t size);
uint8_t kread8(uint64_t addr);
uint16_t kread16(uint64_t addr);
uint32_t kread32(uint64_t addr);
uint64_t kread64(uint64_t addr);
void kwrite8(uint64_t addr, uint8_t value);
void kwrite16(uint64_t addr, uint16_t value);
void kwrite32(uint64_t addr, uint32_t value);
void kwrite64(uint64_t addr, uint64_t value);
uint64_t kalloc(uint32_t size);
uint64_t kalloc_wired(uint32_t size);
void kfree(uint64_t addr, uint32_t size);
void kzero(uint64_t addr, uint32_t size);
uint64_t phystokv(uint64_t pa);
uint64_t get_kva_pte(uint64_t va);
uint64_t kvtophys(uint64_t va);
uint64_t vtophys(uint64_t va);
uint64_t phys_map_page(uint64_t pa);
void phys_unmap_page(uint64_t va);

#endif /* memory_h */
