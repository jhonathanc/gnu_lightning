#include <lightning.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
# include <windows.h>
#else
# include <sys/mman.h>
# ifndef MAP_ANONYMOUS
#  define MAP_ANONYMOUS MAP_ANON
# endif
#endif

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
            exit(1); \
        } \
    } while (0)

static jit_pointer_t
allocate_code(jit_word_t size)
{
    jit_pointer_t buffer;

#ifdef _WIN32
    buffer = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE,
                          PAGE_EXECUTE_READWRITE);
    CHECK(buffer != NULL);
#else
    buffer = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(buffer != MAP_FAILED);
#endif
    return (buffer);
}

static void
release_code(jit_pointer_t buffer, jit_word_t size)
{
#ifdef _WIN32
    CHECK(VirtualFree(buffer, 0, MEM_RELEASE) != 0);
#else
    CHECK(munmap(buffer, size) == 0);
#endif
}

static void
entries(void)
{
    jit_state_t *_jit, *functions;
    jit_node_t *first, *second, *back, *forward, *last, *jump, *call;
    jit_word_t (*entry)(void), (*function1)(void), (*function2)(void);
    jit_word_t (*backward)(void), (*forward_call)(void), (*caller)(void);
    jit_pointer_t code;
    int leading_jump;

    for (leading_jump = 0; leading_jump < 2; leading_jump++) {
        _jit = functions = jit_new_state();
        jump = leading_jump ? jit_jmpi() : NULL;
        first = jit_indirect();
        jit_live(JIT_V0);
        jit_prolog();
        jit_reti(11);
        jit_epilog();
        second = jit_indirect();
        jit_prolog();
        jit_reti(29);
        jit_epilog();
        back = jit_indirect();
        jit_prolog();
        jit_prepare();
        call = jit_finishi(NULL);
        jit_patch_at(call, first);
        jit_retval(JIT_V0);
        call = jit_movi(JIT_R0, 0);
        jit_patch_at(call, second);
        jit_prepare();
        jit_finishr(JIT_R0);
        jit_retval(JIT_R0);
        jit_addr(JIT_R0, JIT_R0, JIT_V0);
        jit_retr(JIT_R0);
        jit_epilog();
        forward = jit_indirect();
        jit_prolog();
        jit_prepare();
        call = jit_finishi(NULL);
        jit_retval(JIT_R0);
        jit_retr(JIT_R0);
        jit_epilog();
        last = jit_indirect();
        jit_prolog();
        jit_reti(55);
        jit_epilog();
        jit_patch_at(call, last);
        if (jump)
            jit_patch_at(jump, first);
        code = jit_emit();
        CHECK(code != NULL);
        entry = code;
        function1 = jit_address(first);
        function2 = jit_address(second);
        backward = jit_address(back);
        forward_call = jit_address(forward);
        jit_clear_state();
        CHECK((*entry)() == 11);
        CHECK((*function1)() == 11);
        CHECK((*function2)() == 29);
        CHECK((*backward)() == 40);
        CHECK((*forward_call)() == 55);

        _jit = jit_new_state();
        jit_prolog();
        jit_prepare();
        jit_finishi(function2);
        jit_retval(JIT_R0);
        jit_addi(JIT_R0, JIT_R0, 7);
        jit_retr(JIT_R0);
        jit_epilog();
        caller = jit_emit();
        CHECK(caller != NULL);
        jit_clear_state();
        CHECK((*caller)() == 36);
        jit_destroy_state();
        _jit = functions;
        jit_destroy_state();
    }
}

static void
positions(void)
{
    jit_state_t *_jit;
    jit_node_t *entry, *before_name, *before_note;
    jit_node_t *body, *after_name, *after_note;
    jit_pointer_t code, address, raw;
    jit_word_t (*function)(void);
#if defined(__powerpc__) && _CALL_AIXDESC
    volatile jit_word_t descriptor[3];
#endif

    _jit = jit_new_state();
    before_name = jit_name("leaf");
    before_note = jit_note(__FILE__, __LINE__);
    entry = jit_indirect();
    jit_prolog();
    body = jit_indirect();
    after_name = jit_name("body");
    after_note = jit_note(__FILE__, __LINE__);
    jit_reti(11);
    jit_epilog();
    function = jit_emit();
    CHECK(function != NULL);
    code = jit_get_code(NULL);
    address = jit_address(entry);
    raw = jit_address(body);
    CHECK(jit_address(before_name) == code);
    CHECK(jit_address(before_note) == code);
    CHECK(jit_address(after_name) == raw);
    CHECK(jit_address(after_note) == raw);
#if defined(__powerpc__) && _CALL_AIXDESC
    CHECK(raw == (jit_pointer_t)((jit_word_t *)address)[0]);
    CHECK(address != raw);
    descriptor[0] = (jit_word_t)raw;
    descriptor[1] = descriptor[2] = 0;
    function = (jit_pointer_t)descriptor;
#endif
    CHECK((*function)() == 11);
    jit_clear_state();
    jit_destroy_state();
}

static void
pool_and_retry(void)
{
    jit_state_t *_jit;
    jit_node_t *nodes[64];
    jit_word_t (*functions[64])(void);
    jit_pointer_t buffer, first_buffer = NULL, second_buffer = NULL;
    jit_word_t size, used = 0, short_size, i;
    int supplied;

    for (supplied = 0; supplied < 2; supplied++) {
        _jit = jit_new_state();
        for (i = 0; i < 64; i++) {
            nodes[i] = jit_indirect();
            jit_prolog();
            jit_reti(i);
            jit_epilog();
        }
        jit_realize();
        jit_get_code(&size);
        if (supplied) {
            first_buffer = allocate_code(size);
            second_buffer = allocate_code(size);
            CHECK(first_buffer != second_buffer);
            memset(first_buffer, 0xa5, size);
            short_size = used - 3 * sizeof(jit_word_t);
            jit_set_code(first_buffer, short_size);
            buffer = jit_emit();
            CHECK(buffer == NULL);
            for (i = short_size; i < size; i++)
                CHECK(((jit_uint8_t *)first_buffer)[i] == 0xa5);
            jit_set_code(second_buffer, size);
        }
        buffer = jit_emit();
        CHECK(buffer != NULL);
        buffer = jit_get_code(&used);
        if (supplied)
            CHECK(buffer == second_buffer);
        for (i = 0; i < 64; i++) {
            functions[i] = jit_address(nodes[i]);
#if defined(__powerpc__) && _CALL_AIXDESC
            CHECK((jit_uword_t)functions[i] % sizeof(jit_word_t) == 0);
            CHECK(((jit_word_t *)functions[i])[0] < (jit_word_t)functions[0]);
            CHECK((jit_word_t *)functions[i] ==
                  (jit_word_t *)functions[0] + i * 3);
#endif
        }
#if defined(__powerpc__) && _CALL_AIXDESC
        CHECK((jit_uint8_t *)functions[0] + 64 * 3 * sizeof(jit_word_t) ==
              (jit_uint8_t *)buffer + used);
#endif
        jit_clear_state();
        for (i = 0; i < 64; i++)
            CHECK((*functions[i])() == i);
        jit_destroy_state();
        if (supplied) {
            release_code(first_buffer, size);
            release_code(second_buffer, size);
        }
    }
}

#if defined(__powerpc__) && _CALL_AIXDESC
static void
different_registers(void)
{
    jit_state_t *_jit;
    jit_node_t *leaf, *getter_node, *continuation, *move;
    jit_pointer_t expected, actual, body;
    jit_pointer_t (*getter)(jit_pointer_t);
    jit_word_t (*function)(void);

    _jit = jit_new_state();
    leaf = jit_indirect();
    jit_prolog();
    jit_reti(11);
    jit_epilog();
    getter_node = jit_indirect();
    jit_prolog();
    jit_getarg(_R0, jit_arg());
    move = jit_movi(_R3, 0);
    jit_patch_at(move, leaf);
    jit_jmpr(_R0);
    continuation = jit_indirect();
    jit_retr(_R3);
    jit_epilog();
    actual = jit_emit();
    CHECK(actual != NULL);
    expected = jit_address(leaf);
    getter = jit_address(getter_node);
    body = jit_address(continuation);
    actual = (*getter)(body);
    CHECK(actual == expected);
    function = actual;
    CHECK((*function)() == 11);
    jit_clear_state();
    jit_destroy_state();
}

static void
long_jump(void)
{
    jit_state_t *_jit;
    jit_node_t *jump, *body;
    jit_pointer_t buffer, code;
    jit_word_t (*function)(void);
    jit_word_t capacity = 64 * 1024 * 1024;

    _jit = jit_new_state();
    jit_prolog();
    jump = jit_jmpi();
    jit_epilog();
    jit_prolog();
    jit_reti(99);
    jit_epilog();
    jit_prolog();
    body = jit_indirect();
    jit_reti(17);
    jit_epilog();
    jit_patch_at(jump, body);
    jit_realize();
    jit_set_data(NULL, 0, JIT_DISABLE_DATA | JIT_DISABLE_NOTE);
    buffer = allocate_code(capacity);
    jit_set_code(buffer, capacity);
    code = jit_emit();
    CHECK(code != NULL);
    function = code;
    CHECK((*function)() == 17);
    jit_clear_state();
    jit_destroy_state();
    release_code(buffer, capacity);
}
#endif

int
main(int argc, char *argv[])
{
    const char *test = argc > 1 ? argv[1] : "all";

    init_jit(argv[0]);
    if (!strcmp(test, "all") || !strcmp(test, "entries"))
        entries();
    if (!strcmp(test, "all") || !strcmp(test, "positions"))
        positions();
    if (!strcmp(test, "all") || !strcmp(test, "pool"))
        pool_and_retry();
#if defined(__powerpc__) && _CALL_AIXDESC
    if (!strcmp(test, "all") || !strcmp(test, "registers"))
        different_registers();
    if (!strcmp(test, "all") || !strcmp(test, "long-jump"))
        long_jump();
#endif
    finish_jit();
    return (0);
}
