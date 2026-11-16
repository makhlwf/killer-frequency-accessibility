#include "music_array.h"
#include <array>
#include <cassert>
#include <iostream>
int main(){
    struct Array {void* klass;void* monitor;void* bounds;uintptr_t length;void* values[3];};
    int a=1,b=2,c=3;
    Array source{nullptr,nullptr,nullptr,3,{&a,&b,&c}};
    auto values=musicArray(&source);
    assert(values.size()==3&&values[0]==&a&&values[1]==&b&&values[2]==&c);
    source.length=0;assert(musicArray(&source).empty());
    source.length=50001;assert(musicArray(&source).empty());
    assert(musicArray(nullptr).empty());
    std::cout<<"Music metadata array extraction preserves every real object pointer.\n";
}
