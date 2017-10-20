#ifndef TRAMPOLINE_TRAMPOLINE_CPP
#define TRAMPOLINE_TRAMPOLINE_CPP

#include <sys/mman.h>
#include <iostream>
#include <cassert>
#include <set>

//getiing args types
template<typename ... Args>
struct args_data;
template<>
struct args_data<> {
    static const int COUNT = 0;
    static const int SSE_COUNT = 0;
};
template<typename U, typename ... TAIL>
struct args_data<U, TAIL...> {
    static const int COUNT = args_data<TAIL...>::COUNT + 1;
    static const int SSE_COUNT = args_data<TAIL...>::SSE_COUNT;
};
template<typename ... TAIL>
struct args_data<double, TAIL...> {
    static const int COUNT = args_data<TAIL...>::COUNT;
    static const int SSE_COUNT = args_data<TAIL...>::SSE_COUNT + 1;
};
template<typename ... TAIL>
struct args_data<float, TAIL...> {
    static const int COUNT = args_data<TAIL...>::COUNT;
    static const int SSE_COUNT = args_data<TAIL...>::SSE_COUNT + 1;
};

//keeps available memory
struct page_keeper {
private:
    static bool initialized;
    static std::set<void *> pages;

    size_t const PAGE_AMOUNT = 16;
    size_t const PAGE_SIZE = 4096;

    void allocate_pages() {
        /* Map addresses starting near ADDR and extending for LEN bytes.  from
   OFFSET into the file FD describes according to PROT and FLAGS.  If ADDR
   is nonzero, it is the desired mapping address.  If the MAP_FIXED bit is
   set in FLAGS, the mapping will be at ADDR exactly (which must be
   page-aligned); otherwise the system chooses a convenient nearby address.
   The return value is the actual mapping address chosen or MAP_FAILED
   for errors (in which case `errno' is set).  A successful `mmap' call
   deallocates any previous mapping for the affected region.
    extern void *mmap (void *__addr, size_t __len, int __prot,
                       int __flags, int __fd, __off_t __offset) __THROW;
                    */
        void *place = mmap(nullptr, PAGE_AMOUNT * PAGE_SIZE, PROT_EXEC | PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (place == MAP_FAILED)
            return;

        for (size_t i = 0; i < PAGE_AMOUNT; i++)
            pages.insert(place + PAGE_SIZE * i);

        initialized = true;
    }

protected:
    void return_page(void *page) {
        pages.insert(page);
    }

    void *get_page() {
        if (!initialized) {
            allocate_pages();
        }
        assert(!pages.empty());
        void *page = *pages.begin();
        pages.erase(pages.begin());
        return page;
    }
};
bool page_keeper::initialized = false;
std::set<void *> page_keeper::pages;


template<typename T>
struct trampoline;

template<typename T, typename ... Args>
struct trampoline<T(Args...)> : page_keeper {
private:
    /*variables*/
    void *func_obj;
    void (*deleter)(void *);
    T (*caller)(void *obj, Args ... args);
    void *code;

    /*useful functions*/
    template<typename F>
    static T do_call(void *obj, Args ... args) {
        return (*static_cast<F *>(obj))(std::forward<Args>(args)...);
    }

    template<typename F>
    static void do_dell(void *obj) {
        delete (static_cast<F *>(obj));
    }

    /*code instertion*/
    void organize_code(char *pcode, int int_count, int sse_count, void *func_ref, void *caller_ref) {
        //Аргументы функции вводятся в регистры в следующем порядке их следования. ;%rdi,%rsi,%rdx,%rcx,%r8 и %r9
        const std::string move_comands[6] = {//moving arg registers to right
                "\x48\x89\xfe", /*mov rsi rdi*/
                "\x48\x89\xf2", /*mov rdx rsi*/
                "\x48\x89\xd1", /*mov rcx rdx*/
                "\x49\x89\xc8", /*mov r8 rcx;*/
                "\x4d\x89\xc1", /*mov r9 r8;*/
                "\x41\x51" /*push %%r9;*/
        };

        trampoline::code_collector h = trampoline::code_collector(pcode);

        if (int_count < 6) {//it's enough to have registers- just move 1 to right from this call, adding func_ref for caller
            for (int i = int_count - 1; i >= 0; i--)
                h.s(move_comands[i]);
            //mov rdi func_ref
            h.i(0x48, 0xbf);h.s(func_ref);
            //mov rax caller_ref
            h.i(0x48, 0xb8);h.s(caller_ref);
            //jmp rax
            h.i(0xff, 0xe0);

        } else {//not enough registers - need to place args in stack
            int stack_size = 8/*BYTE_SIZE*/ * (int_count - 5/*already have in regs*/ +
                                               std::max(sse_count - 8/*already have in xmm*/, 0));

            //mov r10 [rsp] - to save in current stack position as a return adress
            h.i(0x4c, 0x8b, 0x14, 0x24);

            for (int i = 5; i >= 0; i--)/*move reg values*/
                h.s(move_comands[i]);

            ////----preparing stack to copy, instead 1 BYTE - it will be moved
            //mov rax, [rsp]
            h.i(0x48, 0x89, 0xe0);
            //add rax [stack_size]
            h.i(0x48, 0x05);h.s(stack_size);
            //add rsp 0x00000008
            h.i(0x48, 0x81, 0xc4);h.s(8/*BYTE*/);

            ////----creating cycles for stack shifting
            char *loop = h.get_pointer();
            //cmp rax rsp
            h.i(0x48, 0x39, 0xe0);
            //je someValue - aiming to end of looop
            h.i(0x74/*will be known after*/);

            char *aimingValuePlace = h.get_pointer();
            h.i(0x0);

            //add rsp 0x00000008
            h.i(0x48, 0x81, 0xc4);h.s(8);
            //mov rdi [rsp]
            h.i(0x48, 0x8b, 0x3c, 0x24);
            //mov [rsp - 8] rdi
            h.i(0x48, 0x89, 0x7c, 0x24, 0xf8);
            //jmp someValue - aiming to begin of loop
            h.i(0xeb/*will be known right now*/);

            int steps = loop - h.get_pointer() - 1;
            h.i(steps);//how many steps to the begin
            *aimingValuePlace = h.get_pointer() - aimingValuePlace - 1;//how many steps to end

            ////----calling func
            //mov [rsp] r10 = reminding position
            h.i(0x4c, 0x89, 0x14, 0x24);
            //sub rsp stack_size
            h.i(0x48, 0x81, 0xec);h.s(stack_size);
            //mov rdi function
            h.i(0x48, 0xbf);h.s(func_ref);
            //mov rax caller_ref
            h.i(0x48, 0xb8);h.s(caller_ref);
            //call rax
            h.i(0xff, 0xd0);

            ////----restoring stack after calling
            //pop r9
            h.i(0x41, 0x59);
            //mov r10 [rsp + stack_size]
            h.i(0x4c, 0x8b, 0x94, 0x24);
            h.s(stack_size - 8/*BYTE*/);
            //mov [rsp] r10
            h.i(0x4c, 0x89, 0x14, 0x24, 0xc3);
        }
    }

    struct code_collector {
    private:
        char *pcode;
    public:
        code_collector(char *p) : pcode(p) {}

        char *get_pointer() { return pcode; }

        void i(char i1) { *pcode++ = i1; }
        void i(char i1, char i2) { *pcode++ = i1;*pcode++ = i2; }
        void i(char i1, char i2, char i3) {*pcode++ = i1;*pcode++ = i2;*pcode++ = i3; }
        void i(char i1, char i2, char i3, char i4) {*pcode++ = i1;*pcode++ = i2;*pcode++ = i3;*pcode++ = i4; }
        void i(char i1, char i2, char i3, char i4, char i5) {*pcode++ = i1;*pcode++ = i2;*pcode++ = i3;*pcode++ = i4;*pcode++ = i5;}

        void s(std::string str) { for (size_t i = 0; i < str.size(); i++)*pcode++ = str[i];}
        void s(int32_t i1) { *(int32_t *) pcode = i1;pcode += 4;/*32/8*/ }
        void s(void *f_ref) { *(void **) pcode = f_ref;pcode += 8;/*64/8*/}
    };


public:
    template<typename F>
    trampoline(F func) : func_obj(new F(std::move(func))),
                         caller(&do_call<F>),
                         deleter(&do_dell<F>) {
        code = get_page();
        organize_code((char *) code, args_data<Args...>::COUNT, args_data<Args...>::SSE_COUNT, func_obj,
                      (void *) caller);
    };

    trampoline(const trampoline<T(Args...)> &other) : func_obj(other.func_obj),
                                                      caller(other.caller),
                                                      deleter(other.deleter),
                                                      code(get_page()) {
        organize_code((char *) code, args_data<Args...>::COUNT, args_data<Args...>::SSE_COUNT, func_obj, caller);
    }

    ~trampoline() {
        return_page(code);
        if (func_obj)
            deleter(func_obj);
    }

    T (*get() const )(Args ... args) {
        return (T(*)(Args ... args)) code;
    }

    template<typename R, typename ...MArgs>
    void swap(trampoline<R(MArgs...)> &x, trampoline<R(MArgs...)> &y) {
        std::swap(x.func_obj, y.func_obj);
        std::swap(x.code, y.code);
        std::swap(x.caller, y.caller);
        std::swap(x.deleter, y.deleter);
    };

    void swap(trampoline &other) {
        swap(*this, other);
    }

};

using namespace std;

struct object {
    int operator()(int c1, int c2, int c3, int c4, int c5) {
        cout << "in func_obj\n";
        return 23;
    }

    ~object() {
        cout << "object deleted\n";
    }
};


int main() {
    int b = 123;
    object fo;

    trampoline<int(int, int, int, int, int)> t(fo);
    auto pq = t.get();
    pq(1, 2, 3, 4, 5);
    b = 124;

    int res = pq(2, 3, 4, 5, 6);
    cout << res << "\n";
    cout << b << " "  << "\n";
    cout << "\n";
    trampoline<long long(int, int, float, int, int, int, int, int, double)>
            tr([&](int c1, int c2, float c3, int c4, int c5, int c6, int c7, int c8, double c9) {
                   cout << c1 << " " << c2 << " " << c3 << " " << c4 << " " << c5 << " " << c6 << " ";
                   cout << c7 << " " << c8 << " " << c9 << "\n";
                   return 0;
               }
    );
    auto p = tr.get();
    {
        int res = p(100, 200, 300.5, 400, 500, 600, 700, 800, 900.5);
        cout << res << "\n";
    }

    p(9, 8, 7, 6, 5, 4, 3, 2, 1);

    cout << "\n";

    trampoline<long long(int, int, int, int, int, int, int, int, int)>
            tr1([&](int c1, int c2, int c3, int c4, int c5, int c6, int c7, int c8, int c9) {
                    cout << c1 << " " << c2 << " " << c3 << " " << c4 << " " << c5 << " " << c6 << " ";
                    cout << c7 << " " << c8 << " " << c9 << "\n";
                    return 1;
                }
    );
    trampoline<long long(int, int, int, int, int, int, int, int, int)>
            tr2([&](int c1, int c2, int c3, int c4, int c5, int c6, int c7, int c8, int c9) {
                    cout << c1 << " " << c2 << " " << c3 << " " << c4 << " " << c5 << " " << c6 << " ";
                    cout << c7 << " " << c8 << " " << c9 << "\n";
                    return 2;
                }
    );
    trampoline<long long(int, int, int, int, int, int, int, int, int)>
            tr3([&](int c1, int c2, int c3, int c4, int c5, int c6, int c7, int c8, int c9) {
                    cout << c1 << " " << c2 << " " << c3 << " " << c4 << " " << c5 << " " << c6 << " ";
                    cout << c7 << " " << c8 << " " << c9 << "\n";
                    return 3;
                }
    );

    auto p1 = tr1.get();
    auto p2 = tr2.get();
    auto p3 = tr3.get();
    cout << p1(1, 2, 3, 4, 5, 6, 7, 8, 9) << " " << p2(2, 1, 2, 2, 1, 2, 2, 1, 1) << p3(455, 54, 453, 54, 55, 33, 2, 5, 6);
    return 0;
}

#endif //TRAMPOLINE_TRAMPOLINE_CPP
