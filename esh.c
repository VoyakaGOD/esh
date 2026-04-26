#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <readline/readline.h>
#include <fcntl.h>

#define ESH_HISTORY_LIMIT 1000
#define ESH_HISTORY_FILE ".esh_history"
#define ESH_PROMPT_EXTRA_LEN 128
#define ESH_MAX_PIPELINE_LEN 64

#include "lexer.c"
#include "command.c"
#include "parser.c"

int main()
{
    char *input = NULL;
    token_t *tokens;
    command_t *sequence;

    context_t context;
    context.counter = 0;
    context.debug_mode = 0;
    context.cwd = NULL;
    context.prompt = NULL;
    context.home = getenv("HOME");
    if(!context.home)
    {
        fprintf(stderr, "You should have [HOME] env variable to use esh\n");
        exit(-5);
    }

    context.history_path = (char *)malloc(strlen(context.home) + strlen(ESH_HISTORY_FILE) + 2);
    sprintf(context.history_path, "%s/%s", context.home, ESH_HISTORY_FILE);
    read_history(context.history_path);
    stifle_history(ESH_HISTORY_LIMIT);

    while(1)
    {
        free(context.cwd);
        context.cwd = getcwd(NULL, 0);
        if(!context.cwd)
            context.cwd = strdup("[cwd]");
        free(context.prompt);
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
        if(!input)
        {
            printf("exit\n");
            input = "exit";
        }

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

        execute_sequence(sequence, &context);
        context.counter++;

        release_commands(sequence);
        release_tokens(tokens);
        free(input);
    }

    return 0;
}
