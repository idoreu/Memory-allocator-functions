#include <unistd.h>
#include <stddef.h>
#include <cstring>

const size_t TOO_BIG = 100000000;

struct MallocMetadata
{
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};

MallocMetadata* block_arr = NULL;
size_t allocated_blocks = 0;
size_t freed_blocks = 0;
size_t allocated_bytes = 0;
size_t freed_bytes = 0;

void earase_statistics();
void earase_freed_blocks(MallocMetadata* last_block);

void* smalloc(size_t size){
    if(size == 0 || size > TOO_BIG){
        return NULL;
    }
    MallocMetadata* curr_block = block_arr;
    if(curr_block != NULL){
        if(curr_block->is_free && curr_block->size >= size){
            curr_block->is_free = false;
            freed_blocks -=1;
            freed_bytes -= curr_block->size;
            return (void*)(curr_block +1);
        }
        while (curr_block->next != NULL)
        {
            curr_block = curr_block->next;
            if(curr_block->is_free && curr_block->size >= size){
                freed_blocks -=1;
                freed_bytes -= curr_block->size;
                curr_block->is_free = false;
                return (void*)(curr_block +1);
            }
        }
    }
    // meaning no free block was found
    size_t new_block_size = size + sizeof(MallocMetadata);
    MallocMetadata* new_block = (MallocMetadata*)(sbrk(new_block_size));
    if(new_block == (void*)(-1)){
        return NULL;
    }
    if(curr_block == NULL){
        earase_statistics();
    }
    allocated_blocks +=1;
    allocated_bytes += size;
    new_block->size = size;
    new_block->is_free = false;
    new_block->next = NULL;
    new_block->prev = NULL;
    if(curr_block != NULL){
        new_block->prev = curr_block;
        curr_block->next = new_block;
    } else {
        block_arr = new_block;
    }
    return (void*)(new_block +1);
}

void* scalloc(size_t num, size_t size){
    size_t arr_size = num*size;
    void* block_adrr = smalloc(arr_size);
    if(block_adrr != NULL){
        std::memset(block_adrr, 0 , arr_size);
        return block_adrr;
    }
    return NULL;
}


void sfree(void* p){
    if(p == NULL){
        return;
    }
    MallocMetadata* allocated_data = (MallocMetadata*)p;
    allocated_data -= 1; // This line ensures us we are in the meta data section
    if(!allocated_data->is_free){
        freed_blocks += 1;
        freed_bytes += allocated_data->size;
        allocated_data->is_free = true;
        std::memset(p, 0 , allocated_data->size);
    }
}

void earase_statistics(){
    allocated_blocks = 0;
    freed_blocks = 0;
    allocated_bytes = 0;
    freed_bytes = 0;
}

void earase_freed_blocks(MallocMetadata* last_block){
    if(last_block != NULL){
        while(last_block->prev != NULL){
            if(last_block->is_free){
                size_t size = last_block->size + sizeof(MallocMetadata);
                last_block = last_block->prev;
                sbrk(-size);
            }
        }
        if(last_block->is_free){
            size_t size = last_block->size + sizeof(MallocMetadata);
            block_arr = NULL;
            sbrk(-size);
        }
    }
}

void* srealloc(void* oldp, size_t size){
    MallocMetadata* oldBlock =  NULL;
    if(size == 0 || size > TOO_BIG){
        return NULL;
    }
    if(oldp == NULL){
        return smalloc(size);
    }
    oldBlock = (MallocMetadata*)oldp;
    oldBlock -= 1;
    void* block_adrr;
    if(size <= oldBlock->size){
        return oldp;
    } else {
        block_adrr = smalloc(size);
        if(block_adrr == NULL){
            return NULL;
        }
        std::memmove(block_adrr, oldp, oldBlock->size);
        sfree(oldp);
    }
    return block_adrr;
}

size_t _num_free_blocks() {
    return freed_blocks;
}

size_t _num_free_bytes() {
    return freed_bytes; 
}

size_t _num_allocated_blocks() {
    return allocated_blocks;
}


size_t _num_allocated_bytes() {
    return allocated_bytes;
}

size_t _size_meta_data(){
    size_t size = sizeof(MallocMetadata);
    return size;
}

size_t _num_meta_data_bytes(){
    size_t num = _num_allocated_blocks();
    return num*_size_meta_data();
}