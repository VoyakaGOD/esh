#include <readline/history.h>

// use pointers to token list content
typedef struct command
{
    char **args;
    int is_out_append;
    int is_err_append;
    char *out_file;
    char *err_file;
    struct command *next;
    token_type_t separator;
} command_t;

typedef struct context
{
    int counter;
    int debug_mode;
    char *cwd;
    char *prompt;
    char *home;
    char *history_path;
} context_t;

typedef struct pipe_context
{
    int pipe_in;
    int fd_in;
    int pipe_out;
    int fd_out;
} pipe_context_t;

// builtin top half
int execute_if_builtin(command_t *cmd, context_t *context)
{
    if(strcmp(cmd->args[0], "exit") == 0)
    {
        write_history(context->history_path);
        printf("exit");
        exit(0);
    }

    if(strcmp(cmd->args[0], "edbg") == 0)
    {
        context->debug_mode = !context->debug_mode;
        return 1;
    }

    if(strcmp(cmd->args[0], "estory") == 0)
        return 2;

    return 0;
}

int execute_builtin_bottom_half(int id, command_t *cmd, context_t *context)
{
    switch (id)
    {
    case 1:
        printf("debug_mode: %s\n", context->debug_mode ? "enabled" : "disabled");
        break;
    case 2:
        HIST_ENTRY **list = history_list();
        if (!list)
        {
            fprintf(stderr, "Can't load estory\n");
            return 1;
        }
        for (int i = 0; list[i]; i++)
            printf("%d %s\n", i + history_base, list[i]->line);
    default:
        break;
    }

    return 0;
}

void change_io_fd(command_t *cmd, pipe_context_t *pipeline)
{
    if(pipeline->pipe_in)
        dup2(pipeline->fd_in, STDIN_FILENO);

    if(cmd->out_file)
    {
        int fd = open(
            cmd->out_file,
            O_WRONLY | O_CREAT | (cmd->is_out_append ? O_APPEND : O_TRUNC),
            0644
        );
        if (fd < 0)
        {
            perror("Can't open file output for redirection");
            exit(-6);
        }

        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    else if(pipeline->pipe_out)
    {
        dup2(pipeline->fd_out, STDOUT_FILENO);
    }

    if(cmd->err_file)
    {
        int fd = open(
            cmd->err_file,
            O_WRONLY | O_CREAT | (cmd->is_err_append ? O_APPEND : O_TRUNC),
            0644
        );
        if (fd < 0)
        {
            perror("Can't open file for error redirection");
            exit(-7);
        }

        dup2(fd, STDERR_FILENO);
        close(fd);
    }
}

// exectue single command
pid_t execute_command(command_t *cmd, context_t *context, pipe_context_t *pipeline)
{
    int builtin_id = execute_if_builtin(cmd, context);

    pid_t pid = fork();
    if(pid < 0)
    {
        perror("fork failed");
        return pid;
    }

    if(pid == 0)
    {
        change_io_fd(cmd, pipeline);
        if(builtin_id)
            exit(execute_builtin_bottom_half(builtin_id, cmd, context));
        else
            execvp(cmd->args[0], cmd->args);
        perror("exec failed");
        exit(-255);
    }

    if(pipeline->pipe_in)
        close(pipeline->fd_in);
    if(pipeline->pipe_out)
        close(pipeline->fd_out);

    return pid;
}

int execute_pipeline(command_t **head, context_t *context)
{
    int status = 0;
    command_t *cmd = *head;
    pipe_context_t pipeline;
    pipeline.pipe_in = 0;
    pipeline.pipe_out = 0;

    if(cmd->separator != T_PIPE)
    {
        waitpid(execute_command(cmd, context, &pipeline), &status, 0);
        return WEXITSTATUS(status);
    }

    int prev_fd = STDIN_FILENO;
    pid_t pids[ESH_MAX_PIPELINE_LEN];
    int pid_count = 0;

    while(1)
    {
        int is_not_last = cmd->next && (cmd->separator == T_PIPE);
        int pipe_fd[2];
        if(is_not_last && (pipe(pipe_fd) != 0))
        {
            perror("Can't create pipe");
            return 1;
        }

        pipeline.pipe_in = (pid_count > 0);
        pipeline.fd_in = prev_fd;
        pipeline.pipe_out = is_not_last;
        pipeline.fd_out = pipe_fd[1];
        pid_t pid = execute_command(cmd, context, &pipeline);
        if(pid < 0)
            return pid;

        prev_fd = pipe_fd[0];
        pids[pid_count++] = pid;

        if(!is_not_last)
            break;

        cmd = cmd->next;
    }

    *head = cmd;

    for (int i = 0; i < pid_count; i++)
        waitpid(pids[i], &status, 0);

    return WEXITSTATUS(status);
}

void skip_pipeline(command_t **head)
{
    command_t *cmd = *head;
    while(cmd->next && (cmd->separator == T_PIPE))
        cmd = cmd->next;
    *head = cmd;
}

void execute_sequence(command_t *head, context_t *context)
{
    int code = 0;
    int skip = 0;

    while(head)
    {
        if(skip)
            skip_pipeline(&head);
        else
            code = execute_pipeline(&head, context);
        if(context->debug_mode)
            printf("return code: %d, skip: %d\n", code, skip);
        skip = 0;

        // save code
        if((head->separator == T_AND) && code)
            skip = 1;
        if((head->separator == T_OR) && (!code))
            skip = 1;

        if((head->separator != T_SEMI) && (!head->next))
            fprintf(stderr, "Useless separator at the end\n");
        head = head->next;
    }
}
