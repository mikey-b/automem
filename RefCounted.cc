#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <cstdio>

#ifndef NDEBUG
#  if __GNUC__
#    define assert(c) if (!(c)) __builtin_trap()
#  elif _MSC_VER
#    define assert(c) if (!(c)) __debugbreak()
#  else
#    define assert(c) if (!(c)) *(volatile int *)0 = 0
#  endif
#else
#  if __clang__
#    define assert(c) __builtin_assume(c);
#  elif __GNUC__
#    define assert(c) [[assume(c)]];
#  elif _MSC_VER
#    define assert(c) __assume(c);
#  else
#    define assert(c)
#  endif
#endif

template<class T, class U>
concept Derived = std::is_base_of<U, T>::value;

namespace {
    class Allocator {
    public:
        [[gnu::malloc]] virtual void *allocate(size_t) = 0;
        virtual void dropEdge(void*) = 0;
    };

    class Mallocator: public Allocator {
    public:
        [[gnu::malloc]] void *allocate(size_t sz) override {
            assert(sz > 0);
            return calloc(sz, 1);
        }
        
        void dropEdge(void *ptr) override {
            assert(ptr != nullptr);
            free(ptr);
        }
    };

    template<Derived<Allocator> BaseAlloc>
    class RefCounted: public BaseAlloc {
        struct meta {
            int ref;
        };
    public:
        [[gnu::malloc]] void *allocate(std::size_t sz) override {
            assert(sz > 0);
            meta *n = static_cast<meta*>(BaseAlloc::allocate(sizeof(meta) + sz));
            if (n == nullptr) return nullptr;
            
            n->ref = 1;
            char *ptr = reinterpret_cast<char*>(n);
            ptr += sizeof(meta);
            return static_cast<void*>(ptr);
        }
        
        void dropEdge(void *ptr) override {
            assert(ptr != nullptr);
            char *headerpos = static_cast<char*>(ptr);
            headerpos -= sizeof(meta);
            meta *header = reinterpret_cast<meta*>(headerpos);
            
            if (--header->ref == 0) BaseAlloc::dropEdge(headerpos);
        }
    };

    struct A {

    };
}

int main() {
    RefCounted<Mallocator> alloc{};

    auto x = alloc.allocate(sizeof(A));

    printf("%p\n", x);

    // Still need to cleanup! use [[gnc::cleanup]], RAII or manual.
    alloc.dropEdge(x);
    return 0;
}
