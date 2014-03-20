#ifndef TREE_SITTER_PARSER_H_
#define TREE_SITTER_PARSER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ctype.h>
#include <stdio.h>
#include "tree_sitter/runtime.h"

/*
 *  Parsing DSL Macros
 *
 *  Generated parser use these macros. They prevent the code generator
 *  from having too much knowledge of the runtime types and functions.
 */

//#define TS_DEBUG_PARSE
//#define TS_DEBUG_LEX

#ifdef TS_DEBUG_LEX
#define DEBUG_LEX(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_LEX(...)
#endif

#ifdef TS_DEBUG_PARSE
#define DEBUG_PARSE(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_PARSE(...)
#endif

#define PARSE_TABLE() \
static size_t ts_symbol_count; \
static const state_id * ts_lex_states; \
static const ts_parse_action ** ts_parse_actions; \
static void ts_init_parse_table()

#define START_TABLE(num_states) \
ts_symbol_count = TS_SYMBOL_COUNT; \
static int done = 0; \
if (!done) { \
    static const ts_parse_action *parse_actions[num_states]; \
    static state_id lex_states[num_states]; \
    ts_parse_actions = parse_actions; \
    ts_lex_states = lex_states; \
    done = 1; \
    state_id state;

#define END_TABLE() }
#define END_STATE() }

#define STATE(state_val) \
state = state_val; \
if (1) { \
    static ts_parse_action actions_for_state_array[TS_SYMBOL_COUNT + 2]; \
    ts_parse_action *actions_for_state = actions_for_state_array + 2; \
    parse_actions[state] = actions_for_state;

#define SET_LEX_STATE(lex_state_val) \
lex_states[state] = lex_state_val

#define LEX_FN() \
static ts_tree * \
ts_lex(ts_lexer *lexer, state_id lex_state)

#define SYMBOL_NAMES \
static const char *ts_symbol_names[]

#define EXPORT_PARSER(constructor_name) \
ts_parser constructor_name() { \
    ts_init_parse_table(); \
    ts_parser result = { \
        .parse_fn = ts_parse, \
        .symbol_names = ts_symbol_names, \
        .data = ts_lr_parser_make(), \
        .free_fn = NULL \
    }; \
    return result; \
}

#define SHIFT(on_symbol, to_state_value) \
actions_for_state[on_symbol] = (ts_parse_action) { \
    .type = ts_parse_action_type_shift, \
    .data = { .to_state = to_state_value } \
};

#define REDUCE(on_symbol, symbol_val, child_count_val, collapse_flags_val) \
do { \
    static const int collapse_flags[child_count_val] = collapse_flags_val; \
    actions_for_state[on_symbol] = (ts_parse_action) { \
        .type = ts_parse_action_type_reduce, \
        .data = { .symbol = symbol_val, .child_count = child_count_val, .collapse_flags = collapse_flags } \
    }; \
} while(0);

#define ACCEPT_INPUT(on_symbol) \
actions_for_state[on_symbol] = (ts_parse_action) { \
    .type = ts_parse_action_type_accept, \
};

#define START_LEXER() \
ts_lexer_skip_whitespace(lexer); \
if (!ts_lexer_lookahead_char(lexer)) { \
    return ts_tree_make_leaf(ts_builtin_sym_end, 0, 0); \
} \
next_state:

#define LEX_STATE() \
lex_state

#define LOOKAHEAD_CHAR() \
ts_lexer_lookahead_char(lexer)

#define ADVANCE(state_index) \
{ \
    ts_lexer_advance(lexer); \
    lex_state = state_index; \
    goto next_state; \
}

#define ACCEPT_TOKEN(symbol) \
{ \
    DEBUG_LEX("token: %s \n", ts_symbol_names[symbol]); \
    return ts_lexer_build_node(lexer, symbol); \
}

#define LEX_ERROR() \
return ts_lexer_build_node(lexer, ts_builtin_sym_error);

#define LEX_PANIC() \
{ DEBUG_LEX("Lex error: unexpected state %d", LEX_STATE()); return NULL; }

#define COLLAPSE(...) __VA_ARGS__


/*
 *  Stack
 */
typedef int state_id;
typedef struct {
    size_t size;
    struct {
        ts_tree *node;
        state_id state;
    } *entries;
} ts_stack;

ts_stack ts_stack_make();
ts_tree * ts_stack_root(const ts_stack *stack);
ts_tree * ts_stack_reduce(ts_stack *stack, ts_symbol symbol, int immediate_child_count, const int *collapse_flags);
void ts_stack_shrink(ts_stack *stack, size_t new_size);
void ts_stack_push(ts_stack *stack, state_id state, ts_tree *node);
state_id ts_stack_top_state(const ts_stack *stack);
ts_tree * ts_stack_top_node(const ts_stack *stack);


/*
 *  Lexer
 */
typedef struct {
    ts_input input;
    const char *chunk;
    size_t chunk_start;
    size_t chunk_size;
    size_t position_in_chunk;
    size_t token_end_position;
    size_t token_start_position;
} ts_lexer;

static ts_lexer ts_lexer_make() {
    ts_lexer result = {
        .chunk = NULL,
        .chunk_start = 0,
        .chunk_size = 0,
        .position_in_chunk = 0,
        .token_start_position = 0,
        .token_end_position = 0,
    };
    return result;
}

static size_t ts_lexer_position(const ts_lexer *lexer) {
    return lexer->chunk_start + lexer->position_in_chunk;
}

static char ts_lexer_lookahead_char(const ts_lexer *lexer) {
    return lexer->chunk[lexer->position_in_chunk];
}

static void ts_lexer_advance(ts_lexer *lexer) {
    static const char empty_chunk[1] = "";
    if (lexer->position_in_chunk + 1 < lexer->chunk_size) {
        lexer->position_in_chunk++;
    } else {
        lexer->chunk_start += lexer->chunk_size;
        lexer->chunk = lexer->input.read_fn(lexer->input.data, &lexer->chunk_size);
        if (lexer->chunk_size == 0) {
            lexer->chunk = empty_chunk;
            lexer->chunk_size = 1;
        }
        lexer->position_in_chunk = 0;
    }
}

static ts_tree * ts_lexer_build_node(ts_lexer *lexer, ts_symbol symbol) {
    size_t current_position = ts_lexer_position(lexer);
    size_t size = current_position - lexer->token_start_position;
    size_t offset = lexer->token_start_position - lexer->token_end_position;
    lexer->token_end_position = current_position;
    return ts_tree_make_leaf(symbol, size, offset);
}

static void ts_lexer_skip_whitespace(ts_lexer *lexer) {
    while (isspace(ts_lexer_lookahead_char(lexer)))
        ts_lexer_advance(lexer);
    lexer->token_start_position = ts_lexer_position(lexer);
}

static const state_id ts_lex_state_error = -1;


/*
 *  Parse Table components
 */
typedef enum {
    ts_parse_action_type_error,
    ts_parse_action_type_shift,
    ts_parse_action_type_reduce,
    ts_parse_action_type_accept,
} ts_parse_action_type;

typedef struct {
    ts_parse_action_type type;
    union {
        state_id to_state;
        struct {
            ts_symbol symbol;
            int child_count;
            const int *collapse_flags;
        };
    } data;
} ts_parse_action;


/*
 *  Forward declarations
 *  The file including this header should use these macros to provide definitions.
 */
LEX_FN();
PARSE_TABLE();


/*
 *  Parser
 */
typedef struct {
    ts_lexer lexer;
    ts_stack stack;
    ts_tree *lookahead;
    ts_tree *next_lookahead;
} ts_lr_parser;

static ts_lr_parser * ts_lr_parser_make() {
    ts_lr_parser *result = malloc(sizeof(ts_lr_parser));
    result->lexer = ts_lexer_make();
    result->stack = ts_stack_make();
    return result;
}
    
static size_t ts_lr_parser_breakdown_stack(ts_lr_parser *parser, ts_input_edit *edit) {
    if (parser->stack.size == 0) return 0;
    
    ts_tree *node = ts_stack_top_node(&parser->stack);
    size_t left_position = 0;
    size_t right_position = node->offset + node->size;
    size_t child_count;
    ts_tree ** children = ts_tree_children(node, &child_count);

    while (right_position > edit->position || children) {
        parser->stack.size--;
        ts_tree *child;
        for (size_t i = 0; i < child_count; i++) {
            child = children[i];
            right_position = left_position + child->offset + child->size;
            ts_tree_retain(child);
            state_id parse_state = ts_parse_actions[ts_stack_top_state(&parser->stack)][child->symbol].data.to_state;
            ts_stack_push(&parser->stack, parse_state, child);
            if (right_position >= edit->position) break;
            left_position = right_position;
        }
        ts_tree_release(node);
        node = child;
        children = ts_tree_children(node, &child_count);
    }
    
    return right_position;
}

static void ts_lr_parser_initialize(ts_lr_parser *parser, ts_input input, ts_input_edit *edit) {
    if (!edit) ts_stack_shrink(&parser->stack, 0);
    parser->lookahead = NULL;
    parser->next_lookahead = NULL;

    size_t position = ts_lr_parser_breakdown_stack(parser, edit);
    input.seek_fn(input.data, position);

    parser->lexer = ts_lexer_make();
    parser->lexer.input = input;
    ts_lexer_advance(&parser->lexer);
}

static void ts_lr_parser_shift(ts_lr_parser *parser, state_id parse_state) {
    ts_stack_push(&parser->stack, parse_state, parser->lookahead);
    parser->lookahead = parser->next_lookahead;
    parser->next_lookahead = NULL;
}

static void ts_lr_parser_reduce(ts_lr_parser *parser, ts_symbol symbol, int immediate_child_count, const int *collapse_flags) {
    parser->next_lookahead = parser->lookahead;
    parser->lookahead = ts_stack_reduce(&parser->stack, symbol, immediate_child_count, collapse_flags);
}

static ts_symbol * ts_lr_parser_expected_symbols(ts_lr_parser *parser, size_t *count) {
    *count = 0;
    const ts_parse_action *actions = ts_parse_actions[ts_stack_top_state(&parser->stack)];
    for (size_t i = 0; i < ts_symbol_count; i++)
        if (actions[i].type != ts_parse_action_type_error)
            ++(*count);

    size_t n = 0;
    ts_symbol *result = malloc(*count * sizeof(*result));
    for (size_t i = 0; i < ts_symbol_count; i++)
        if (actions[i].type != ts_parse_action_type_error)
            result[n++] = i;

    return result;
}

static int ts_lr_parser_handle_error(ts_lr_parser *parser) {
    size_t count = 0;
    ts_symbol *expected_symbols = ts_lr_parser_expected_symbols(parser, &count);
    ts_tree *error = ts_tree_make_error(ts_lexer_lookahead_char(&parser->lexer), count, expected_symbols, 0, 0);

    for (;;) {
        ts_tree_release(parser->lookahead);
        parser->lookahead = ts_lex(&parser->lexer, ts_lex_state_error);
        if (parser->lookahead->symbol == ts_builtin_sym_end) {
            parser->stack.entries[0].node = error;
            return 0;
        }

        /*
         *  Unwind the stack, looking for a state in which this token
         *  may appear after an error.
         */
        for (long i = parser->stack.size - 1; i >= 0; i--) {
            state_id stack_state = parser->stack.entries[i].state;
            ts_parse_action action_on_error = ts_parse_actions[stack_state][ts_builtin_sym_error];
            if (action_on_error.type == ts_parse_action_type_shift) {
                state_id state_after_error = action_on_error.data.to_state;
                if (ts_parse_actions[state_after_error][parser->lookahead->symbol].type != ts_parse_action_type_error) {
                    ts_stack_shrink(&parser->stack, i + 1);
                    ts_stack_push(&parser->stack, state_after_error, error);
                    return 1;
                }
            }
        }
    }
}

static const ts_tree * ts_parse(void *data, ts_input input, ts_input_edit *edit) {
    int done = 0;
    ts_lr_parser *parser = (ts_lr_parser *)data;
    ts_lr_parser_initialize(parser, input, edit);
    while (!done) {
        state_id state = ts_stack_top_state(&parser->stack);
        if (!parser->lookahead)
            parser->lookahead = ts_lex(&parser->lexer, ts_lex_states[state]);
        ts_parse_action action = ts_parse_actions[state][parser->lookahead->symbol];
        switch (action.type) {
            case ts_parse_action_type_shift:
                ts_lr_parser_shift(parser, action.data.to_state);
                break;
            case ts_parse_action_type_reduce:
                ts_lr_parser_reduce(parser, action.data.symbol, action.data.child_count, action.data.collapse_flags);
                break;
            case ts_parse_action_type_accept:
                done = 1;
                break;
            case ts_parse_action_type_error:
                done = !ts_lr_parser_handle_error(parser);
                break;
        }
    }
    return ts_stack_root(&parser->stack);
}

#ifdef __cplusplus
}
#endif

#endif  // TREE_SITTER_PARSER_H_
