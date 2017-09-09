#include <iostream>
#include <ctime>
#include <emmintrin.h>
#include <cstring>

void memcpy_by_elem(void const * from, void* to, size_t size){
    for (size_t i=0;i<size;i++){
        *((char *)(to) +i)=*((const char *) (from) + i);
    }
}

extern "C" void memcpy_asm_8_side(void const * from, void* to, size_t size);
void memcpy_asm_8_inline(void const * from, void* to, size_t size);
void memcpy_asm_16_inline(void const * from, void* to, size_t size);
void memcpy_asm_16_side(void const * from, void* to, size_t size);

void memcpy_asm_8_inline(void const * from, void* to, size_t size){
    size_t i = 0;
    for (; i < size; i += 8) {
        __asm__(
        //".intel_syntax noprefix\n\t"
        "movq\t(%0), %%rax\n\t"
        "movq\t %%rax, (%1)\n\t"
        :
        :"r"((const char *) from + i), "r"((char *) to+ i)
        :"%rax");

    }
    if (i<size)
        memcpy_by_elem((const char *)from+i,(char*)to+i,size-i);
}

void memcpy_asm_16_inline(void const * from, void* to, size_t size){
    size_t i=0;
    for (;i+16 < size; i += 16) {
        __m128i bigReg;
        __asm__(
        "movdqu\t(%1), %0\n\t"
        "movntdq\t%0, (%2)\n\t"
        :"=Yz"(bigReg)
        :"r"((const char *) from+i), "r"((char*) to + i)//x - любой регистр SSE, Yz - регистр xmm0
        );
    }
    if (i<size)
        memcpy_by_elem((const char *)from+i,(char*)to+i,size-i);
}

void check(void * from, void* to, size_t size){
    std::string str="OK\n";
    for (size_t i=0;i<size; i++){
        if (((char*)from)[i]!=((char*)to)[i]) {
            str="NeOK\n";
            std::cout<<i<<' '<<((char*)from)[i]<<' '<<((char*)to)[i]<<'\n';
            std::cout<<str;
            return;
        }
    }
    std::cout<<str;
}

void genHonest(void * from, void* to, size_t size){
    /*std::cout<<"genering...\n";
    for (size_t i=0;i<size; i++){
        ((char*)from)[i]=(char)(rand()*90 - 40);
        ((char*)to)[i]=(char)(rand()*90 -40);
    }
    std::cout<<"genered\n";*/
    for (size_t i=0;i<size; i++){
        ((char*)from)[i]='a';
        ((char*)to)[i]='b';
    }
}

int main() {
    for (int i=17;i<(1<<30)&&(i>0);i*=17) {
        char *from = new char[i];
        char *to = new char[i];
        time_t time;

        genHonest(from,to,(size_t)i);
        std::cout<<"\nChecking time with "<<i<<" values:\n";
        time= std::clock();
        memcpy_by_elem(from,to,(size_t)i);
        std::cout<<(clock()-time)<<" - usual memcpy\n";
        check(from,to,(size_t)i);

        genHonest(from,to,(size_t)i);
        time=std::clock();
        memcpy_asm_8_side(from,to,(size_t)i);
        std::cout<<(clock()-time)<<" - 8 byte memcpy side func\n";
        check(from,to,(size_t)i);

        genHonest(from,to,(size_t)i);
        time=std::clock();
        memcpy_asm_8_inline(from,to,(size_t)i);
        std::cout<<(clock()-time)<<" - 8 byte memcpy inline func\n";
        check(from,to,(size_t)i);

        genHonest(from,to,(size_t)i);
        to[4]='c';
        time=std::clock();
        memcpy_asm_16_side(from,to,(size_t)i);
        std::cout<<(clock()-time)<<" - 16 byte memcpy side func\n";
        check(from,to,(size_t)i);

        genHonest(from,to,(size_t)i);
        to[4]='c';
        time=std::clock();
        memcpy_asm_16_inline(from,to,(size_t)i);
        std::cout<<(clock()-time)<<" - 16 byte memcpy inline func\n";
        check(from,to,(size_t)i);

        genHonest(from,to,(size_t)i);
        to[4]='c';
        time=std::clock();
        std::memcpy(from,to,(size_t)i);
        std::cout<<(clock()-time)<<" - standard cpy\n";
        check(from,to,(size_t)i);

        delete[] from;
        delete[] to;
    }

    return 0;
}