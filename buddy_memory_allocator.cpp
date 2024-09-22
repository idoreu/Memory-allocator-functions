#include <unistd.h>
#include <stddef.h>
#include <cstring>
#include <sys/mman.h>

const size_t MIN_BLOCK_SIZE = 128;
const size_t TREE_SIZE = (MIN_BLOCK_SIZE*1024);
const size_t NUM_OF_TREES = 32;
const size_t TOTAL_MEM_SIZE = (TREE_SIZE*NUM_OF_TREES);
const size_t MAX_ORDERS = 10;
const size_t TOO_BIG = TREE_SIZE;

struct MallocMetadata
{
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;

    MallocMetadata* buddy;
    MallocMetadata* nextFree;
    MallocMetadata* prevFree;
};
//
void initiate();
void initiateBlock(MallocMetadata* block, MallocMetadata* buddy, size_t size);
void insert_free_block(MallocMetadata* block);
size_t calc_block_rank(size_t size);
MallocMetadata* get_min_free_block(size_t size);
MallocMetadata* merge_buddies(MallocMetadata* block);
bool need_to_split(MallocMetadata* block, size_t size);
MallocMetadata* split_block(MallocMetadata* block, size_t size);
void detach_from_orderd_list(MallocMetadata* block);
void join_with_buddies(MallocMetadata* block);
//
void* oldsmalloc(size_t size);
void oldsfree(void* p);
//
MallocMetadata* block_arr = nullptr;
MallocMetadata* orderd_blocks[MAX_ORDERS];
MallocMetadata* largeBlocks = nullptr;
bool initiated_information = false;

size_t allocated_blocks = 0;
size_t freed_blocks = 0;
size_t allocated_bytes = 0;
size_t freed_bytes = 0;

void* smalloc(size_t size){
    if(size == 0){
        return nullptr;
    } else if(size > TOO_BIG){
        return oldsmalloc(size);
    }
    if(!initiated_information){
        initiate();
        if(!initiated_information){
            return nullptr;
        }
    }
    MallocMetadata* curr_block = get_min_free_block(size);
    if(curr_block == nullptr){
        return nullptr;
    }
    if(need_to_split(curr_block, size)){
        curr_block = split_block(curr_block, size);
        freed_blocks -=1;
    }
    curr_block->is_free = false;
    return (void*)(curr_block+1);
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
    if(p == nullptr){
        return;
    }
    MallocMetadata* allocated_data = (MallocMetadata*)p;
    if(!allocated_data->is_free){
        allocated_data->is_free = true;
        allocated_bytes -= allocated_data->size - sizeof(MallocMetadata);
        freed_blocks += 1;
        std::memset(p, 0 , allocated_data->size);
    }  
    if(allocated_data->size > TREE_SIZE){
        oldsfree(p);
        return;
    }
    freed_bytes += allocated_data->size;

    join_with_buddies(allocated_data);
    return;
}

void* srealloc(void* oldp, size_t size){
    if(size == 0){
        return nullptr;
    }
    if(oldp == nullptr){
        return smalloc(size);
    }
    if(size > TOO_BIG){
        //addresse when this is a mmap problem
    }
    MallocMetadata* oldBlock =  nullptr;
    oldBlock = (MallocMetadata*)oldp;
    oldBlock -= 1;
    void* block_adrr;
    if(size <= oldBlock->size){
        return oldp;
    } else {
        
        block_adrr = smalloc(size);
        if(block_adrr == nullptr){
            return nullptr;
        }
        std::memmove(block_adrr, oldp, oldBlock->size);
        sfree(oldp);
    }
    return block_adrr;
}

//
void initiateBlock(MallocMetadata* block, MallocMetadata* buddy, size_t size)
{
    if(block == nullptr){
        return;
    }
    block->size = size;
    block->is_free = true;
    //
    block->next = nullptr;
    block->prev = nullptr;
    block->buddy = buddy;
    block->nextFree = nullptr;
    block->prevFree = nullptr;
}

void initiate()
{
    MallocMetadata* temp = (MallocMetadata*)(sbrk(TOTAL_MEM_SIZE+ ((MAX_ORDERS+1)*sizeof(MallocMetadata*))));
    if(temp == (void*)(-1)){
        return;
    }
    allocated_blocks += NUM_OF_TREES;
    allocated_bytes += TOTAL_MEM_SIZE;
    allocated_bytes -= NUM_OF_TREES*sizeof(MallocMetadata);
    initiated_information = true;
    size_t size = MIN_BLOCK_SIZE;
    for(size_t i = 0; i <= MAX_ORDERS; ++i){
        orderd_blocks[i] = temp;
        initiateBlock(orderd_blocks[i], nullptr, size);
        size *= 2;
        temp += 1;
    }
    block_arr = temp;
    MallocMetadata* seeker = orderd_blocks[MAX_ORDERS];
    for(size_t n = 0; n < NUM_OF_TREES; ++n){
        initiateBlock(seeker, nullptr, TREE_SIZE);
        seeker->nextFree = (MallocMetadata*)((char*)temp + TREE_SIZE*n);
        seeker->prevFree = (n == 0) ? nullptr : (MallocMetadata*)((char*)temp + TREE_SIZE*(n-1));
        seeker->next = (n == NUM_OF_TREES-1) ? nullptr : (MallocMetadata*)((char*)temp + TREE_SIZE*(n+1));
        seeker->prev = (n == 0) ? nullptr : (MallocMetadata*)((char*)temp + TREE_SIZE*(n-1));
        seeker = seeker->nextFree;
    }
    // block_arr = temp;
}

size_t calc_block_rank(size_t size){
    size_t meta_size = sizeof(MallocMetadata);
    // if(TREE_SIZE/2 <= size && size <= TREE_SIZE){
    //     return MAX_ORDERS;
    // }
    if(size+meta_size <= MIN_BLOCK_SIZE){
        return 0;
    }
    size_t rank = 0;
    for(size_t k = 0; k < MAX_ORDERS; ++k){
        ++rank;
        size /= 2;
        if(size + meta_size <= MIN_BLOCK_SIZE){
            return rank;
        }
    }
    return rank;
}

MallocMetadata* get_min_free_block(size_t size)
{
    size_t block_rank = calc_block_rank(size);
    for(size_t i = block_rank; i <= MAX_ORDERS; ++i){
        if(orderd_blocks[i]->nextFree != nullptr){
            return orderd_blocks[i]->nextFree;
        }
    }
    return nullptr;
}

bool need_to_split(MallocMetadata* block, size_t size)
{
    return(size <= (block->size/2));
}

void detach_from_orderd_list(MallocMetadata* block)
{
    MallocMetadata* temp = nullptr;
    if(block->prevFree != nullptr){
        temp = block->prevFree;
        temp->nextFree = block->nextFree;
    }
    block->prevFree = nullptr;
    block->nextFree = nullptr;
}

void insert_free_block(MallocMetadata* block)
{
    if(block == nullptr){
        return;
    } else if(block->nextFree != nullptr && block->prevFree != nullptr){
        return;
    } else if(block->buddy == nullptr){
        return;
    }
    size_t rank = calc_block_rank(block->size);
    if(orderd_blocks[rank]->nextFree == nullptr){
        orderd_blocks[rank]->nextFree = block;
        block->prevFree = orderd_blocks[rank];
        return;
    }
    if(orderd_blocks[rank] == nullptr){
        if(0 <= rank && rank <= MAX_ORDERS){
            orderd_blocks[rank] = (block_arr - MAX_ORDERS + rank);
        }
    }
    MallocMetadata* seeker = orderd_blocks[rank];
    while(seeker->nextFree != nullptr){
        if((char*)(seeker->nextFree) <= (char*)(block)){
            seeker = seeker->nextFree;
        } else {
            break;
        }
    }
    if(seeker->nextFree == nullptr){
        seeker->nextFree = block;
        block->prevFree = seeker;
    } else {
        block->nextFree = seeker;
        block->prevFree = seeker->prevFree;
        seeker->prevFree = block;
    }

}

MallocMetadata* split_block(MallocMetadata* block, size_t size)
{
    detach_from_orderd_list(block);
    size_t block_rank = calc_block_rank(block->size);
    if(block_rank == 0){
        return block;
    }
    size_t size_rank = calc_block_rank(size);
    for(size_t i = 0; i < (block_rank - size_rank); ++i){
        allocated_blocks += 1;
        freed_blocks +=1;
        allocated_bytes -= block->size - sizeof(MallocMetadata);
        MallocMetadata* free_block = (MallocMetadata*)((char*)block + block->size/2);
        if(block->buddy == nullptr){
            initiateBlock(free_block, block, size/2);
        } else {
            initiateBlock(free_block, block->buddy, size/2);
        }
        free_block->next = block->next;
        block->next = free_block;
        free_block->prev = block;
        block->size = size/2;
        allocated_bytes += 2*(block->size);
        insert_free_block(free_block);
    }
    // allocated_bytes += block->size - sizeof(MallocMetadata);
    return block;
}

void join_with_buddies(MallocMetadata* block){
    if(block == nullptr){
        return;
    } else if(block->buddy == nullptr || block->size == TREE_SIZE){
        return;
    }
    if(block->is_free){
        detach_from_orderd_list(block);
        size_t rank = calc_block_rank(block->size);
        if(rank == MAX_ORDERS){
            insert_free_block(block);
            return;
        }
        MallocMetadata* seeker = orderd_blocks[rank];
        MallocMetadata* block_buddy = (block->buddy == nullptr) ? block : block->buddy;
        MallocMetadata* seeker_buddy = nullptr;
        while(seeker->nextFree != nullptr){
            seeker_buddy = (seeker->nextFree->buddy == nullptr) ? seeker->nextFree : seeker->nextFree->buddy;
            if((char*)(seeker_buddy) == (char*)(block_buddy)){
                seeker = seeker->nextFree;
                detach_from_orderd_list(seeker->nextFree);
                seeker = nullptr;
                block->size *= 2;
                allocated_blocks -=1;
                freed_blocks -= 1;
                break;
            }
        }
        if(seeker == nullptr){
            //succcess
            join_with_buddies(block);
        }
        insert_free_block(block);
    }
}

void* oldsmalloc(size_t size){
    if(size == 0){
        return nullptr;
    }
    MallocMetadata* curr_block = largeBlocks;
    if(curr_block != nullptr){
        if(curr_block->is_free && curr_block->size >= size){
            curr_block->is_free = false;
            return (void*)(curr_block +1);
        }
        while (curr_block->next != nullptr)
        {
            curr_block = curr_block->next;
            if(curr_block->is_free && curr_block->size >= size){
                curr_block->is_free = false;
                return (void*)(curr_block +1);
            }
        }
    }
    // meaning no free block was found
    size_t new_block_size = size + sizeof(MallocMetadata);
    MallocMetadata* new_block = (MallocMetadata*)(mmap(nullptr, new_block_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
    if(new_block == (void*)(-1)){
        return nullptr;
    }
    allocated_blocks +=1;
    allocated_bytes += size;
    new_block->size = size;
    new_block->is_free = false;
    new_block->next = nullptr;
    new_block->prev = nullptr;
    if(curr_block != nullptr){
        new_block->prev = curr_block;
        curr_block->next = new_block;
    } else {

        largeBlocks = new_block;
    }
    return (void*)(new_block +1);
}

void oldsfree(void* p){
    if(p == nullptr){
        return;
    }
    MallocMetadata* allocated_data = (MallocMetadata*)p;
    allocated_data -= 1; // This line ensures us we are in the meta data section
    if(!allocated_data->is_free){
        freed_blocks += 1;
        freed_bytes += allocated_data->size;
        allocated_data->is_free = true;
        size_t allocated_size = allocated_data->size + sizeof(MallocMetadata);
        munmap(p , allocated_size);
    }
}

//
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