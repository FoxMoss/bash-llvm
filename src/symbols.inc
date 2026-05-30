#define PROG_SYMBOL(symb) SYMBOL(bash_##symb)
#include "programfuncs.inc"
#undef PROG_SYMBOL

SYMBOL(str_to_float)
SYMBOL(str_to_len)
SYMBOL(int_len)
SYMBOL(int_to_str)
SYMBOL(strequals)

SYMBOL(create_variable_memory)
SYMBOL(store_variable_memory)
SYMBOL(store_args_variable_memory)
SYMBOL(get_variable_memory)
SYMBOL(free_variable_memory)
SYMBOL(push_output_stack)
SYMBOL(pop_output_stack)

