#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <readline/readline.h>
#include <fcntl.h>

#include "lexer.c"
#include "command.c"
#include "parser.c"

#define ESH_HISTORY_LIMIT 1000
#define ESH_HISTORY_FILE "/.esh_history"
#define ESH_PROMPT_EXTRA_LEN 128

int main()
{
    char *input = NULL;
    token_t *tokens;
    command_t *sequence;

    context_t context;
    context.counter = 0;
    context.debug_mode = 0;
    context.home = getenv("HOME");
    if(!context.home)
    {
        fprintf(stderr, "You should have [HOME] env variable to use esh\n");
        exit(-5);
    }

    context.history_path = (char *)malloc(strlen(context.home) + strlen(ESH_HISTORY_FILE) + 1);
    sprintf(context.history_path, "%s/.esh_history", context.home);
    read_history(context.history_path);
    stifle_history(ESH_HISTORY_LIMIT);

    while(1)
    {
        context.cwd = getcwd(NULL, 0);
        if(!context.cwd)
            context.cwd = strdup("[cwd]");
        context.prompt = (char *)malloc(strlen(context.cwd) + ESH_PROMPT_EXTRA_LEN);
        if(!context.prompt)
            context.prompt = strdup("[prompt]");
        snprintf(
            context.prompt,
            strlen(context.cwd) + ESH_PROMPT_EXTRA_LEN,
            "esh(%d):%s>%s ",
            context.counter,
            context.cwd,
            context.debug_mode ? ">" : ""
        );

        input = readline(context.prompt);
        tokens = tokenize_input(input);
        expand_tilde(tokens);
        sequence = parse_commands(tokens);
        if(context.debug_mode)
        {
            print_token_list(tokens);
            print_command_list(sequence);
        }

        if(input[0])
            add_history(input);

        if (!sequence)
            continue;
        context.counter++;

        if(execute_if_builtin(sequence, &context))
            continue;
        int code = execute_command(sequence);
        if(context.debug_mode)
            printf("return code: %d\n", code);

        release_commands(sequence);
        release_tokens(tokens);
        free(input);
        free(context.cwd);
        free(context.prompt);
    }

    return 0;
}
