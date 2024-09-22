#include <stddef.h>
#include <unistd.h>

void* smalloc(size_t size){
    size_t too_big = 100000000;
    if(too_big < size){
        return NULL;
    }
    if(size == 0){
        return NULL;
    }
    void* curr_break = sbrk(size);
    if(curr_break == (void*)(-1)){
        return NULL;
    } else{
        return curr_break;
    }
}