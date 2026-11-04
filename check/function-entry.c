#include <lightning.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void
many_functions(void)
{
    jit_state_t		 *_jit;
    jit_node_t		 *nodes[64];
    jit_word_t		(*functions[64])(void);
    jit_word_t		 size, used = 0, i;
    jit_pointer_t	  buffer = NULL;
    int			  supplied;

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
	    buffer = malloc(size);
	    assert(buffer != NULL);
	    memset(buffer, 0xa5, size);
	    jit_set_code(buffer, used - 3 * sizeof(jit_word_t));
	    assert(jit_emit() == NULL);
	    for (i = used - 3 * sizeof(jit_word_t); i < size; i++)
		assert(((jit_uint8_t *)buffer)[i] == 0xa5);
	    jit_set_code(buffer, size);
	}
	assert(jit_emit() != NULL);
	jit_get_code(&used);
	for (i = 0; i < 64; i++) {
	    functions[i] = jit_address(nodes[i]);
#if defined(__powerpc__) && _CALL_AIXDESC
	    assert((jit_uword_t)functions[i] % sizeof(jit_word_t) == 0);
	    assert(((jit_word_t *)functions[i])[0] < (jit_word_t)functions[0]);
	    assert((jit_word_t *)functions[i] == (jit_word_t *)functions[0] + i * 3);
#endif
	}
#if defined(__powerpc__) && _CALL_AIXDESC
	assert((jit_uint8_t *)functions[0] + 64 * 3 * sizeof(jit_word_t) ==
	       (jit_uint8_t *)jit_get_code(NULL) + used);
#endif
	jit_clear_state();
	assert(jit_get_code(&size) != NULL && size == used);
	if (!supplied)
	    for (i = 0; i < 64; i++)
		assert((*functions[i])() == i);
	jit_destroy_state();
	free(buffer);
    }
}

int
main(int argc, char *argv[])
{
    jit_state_t		 *_jit, *entries;
    jit_node_t		 *first, *second, *third, *fourth, *fifth, *jump, *call;
    jit_pointer_t	  code, buffer;
    jit_word_t		  size, used;
    jit_word_t		(*entry)(void), (*function1)(void), (*function2)(void);
    jit_word_t		(*caller)(void), (*backward)(void), (*forward)(void);
    int			  leading_jump;

    init_jit(argv[0]);
    for (leading_jump = 0; leading_jump < 2; leading_jump++) {
	_jit = entries = jit_new_state();
	jump = leading_jump ? jit_jmpi() : NULL;
	first = jit_indirect();
	jit_live(JIT_V0);
	jit_prolog();
	call = jit_movi(JIT_R0, 0);
	jit_jmpr(JIT_R0);
	jit_patch(call);
	jit_reti(11);
	jit_epilog();
	second = jit_indirect();
	jit_prolog();
	jit_reti(29);
	jit_epilog();
	third = jit_indirect();
	jit_prolog();
	jit_prepare();
	jit_patch_at(jit_finishi(NULL), first);
	jit_retval(JIT_V0);
	jit_patch_at(jit_movi(JIT_R0, 0), second);
	jit_prepare();
	jit_finishr(JIT_R0);
	jit_retval(JIT_R0);
	jit_addr(JIT_R0, JIT_R0, JIT_V0);
	jit_retr(JIT_R0);
	jit_epilog();
	fourth = jit_indirect();
	jit_prolog();
	jit_prepare();
	call = jit_finishi(NULL);
	jit_retval(JIT_R0);
	jit_retr(JIT_R0);
	jit_epilog();
	fifth = jit_indirect();
	jit_prolog();
	jit_reti(55);
	jit_epilog();
	jit_patch_at(call, fifth);
	if (jump)
	    jit_patch_at(jump, first);
	code = jit_emit();
	assert(code != NULL);
	entry = code;
	function1 = jit_address(first);
	function2 = jit_address(second);
	backward = jit_address(third);
	forward = jit_address(fourth);
	buffer = jit_get_code(&used);
#if defined(__powerpc__) && _CALL_AIXDESC
	assert((jit_uword_t)function1 % sizeof(jit_word_t) == 0);
	assert(((jit_word_t *)function1)[0] < (jit_word_t)function1);
	assert(((jit_word_t *)function2)[0] < (jit_word_t)function1);
	assert((jit_word_t *)function2 == (jit_word_t *)function1 + 3);
#endif
	jit_clear_state();
	assert(jit_get_code(&size) == buffer && size == used);
	assert((*entry)() == 11);
	assert((*function1)() == 11);
	assert((*function2)() == 29);
	assert((*backward)() == 40);
	assert((*forward)() == 55);

	_jit = jit_new_state();
	jit_prolog();
	jit_prepare();
	jit_finishi(function2);
	jit_retval(JIT_R0);
	jit_addi(JIT_R0, JIT_R0, 7);
	jit_retr(JIT_R0);
	jit_epilog();
	caller = jit_emit();
	assert(caller != NULL);
	jit_clear_state();
	assert((*caller)() == 36);
	jit_destroy_state();
	_jit = entries;
	jit_destroy_state();
    }
    many_functions();
    finish_jit();
    return (0);
}
