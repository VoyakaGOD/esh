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

int is_separator_token(token_type_t type)
{
    return (type == T_PIPE) ||
           (type == T_AND ) ||
           (type == T_OR  ) ||
           (type == T_SEMI);
}

void release_commands(command_t *head)
{
    while(head)
    {
        command_t *next = head->next;

        free(head->args);
        free(head);

        head = next;
    }
}

command_t *new_command(token_t *head, int count, token_type_t separator)
{
    int i = 0;
    int j = 0;
    command_t *cmd = malloc(sizeof(command_t));
    if (!cmd)
    {
        perror("Can't allocate memory for command");
        exit(-2);
    }

    cmd->args = malloc((count + 1) * sizeof(char *));
    if (!cmd->args)
    {
        perror("Can't allocate memory for command args");
        exit(-3);
    }

    cmd->is_out_append = 0;
    cmd->is_err_append = 0;
    cmd->out_file = NULL;
    cmd->err_file = NULL;
    cmd->next = NULL;
    cmd->separator = separator;

    while(i < count)
    {
        if((head->type == T_REDIR_OUT_1) || (head->type == T_REDIR_OUT_2))
        {
            cmd->is_out_append = (head->type == T_REDIR_OUT_2);
            if(!head->next)
            {
                perror("No file name for output redirection");
                free(cmd->args);
                free(cmd);
                return NULL;
            }

            head = head->next;

            if(head->type != T_WORD)
            {
                perror("Bad redirection target type");
                free(cmd->args);
                free(cmd);
                return NULL;
            }

            cmd->out_file = head->value;
            i += 2;
        }
        else if((head->type == T_REDIR_ERR_1) || (head->type == T_REDIR_ERR_2))
        {
            cmd->is_err_append = (head->type == T_REDIR_ERR_2);
            if(!head->next)
            {
                perror("No file name for error redirection");
                free(cmd->args);
                free(cmd);
                return NULL;
            }

            head = head->next;

            if(head->type != T_WORD)
            {
                perror("Bad redirection target type");
                free(cmd->args);
                free(cmd);
                return NULL;
            }

            cmd->err_file = head->value;
            i += 2;
        }
        else
        {
            cmd->args[j++] = head->value;
            i++;
        }

        head = head->next;
    }

    cmd->args[j] = NULL;
    return cmd;
}

void add_command(command_t **head, command_t **tail, command_t *new)
{
    if(!*head)
        *head = new;
    else
        (*tail)->next = new;

    *tail = new;
}

command_t *parse_commands(token_t *head)
{
    command_t *cmd_head = NULL;
    command_t *cmd_tail = NULL;
    token_t *tk = head;

    int i = 0;
    token_type_t separator = T_SEMI;

    while(tk)
    {
        head = tk;
        i = 0;

        while(!is_separator_token(tk->type))
        {
            i++;
            tk = tk->next;
            if(!tk)
                break;
        }

        if(tk)
        {
            separator = tk->type;
            tk = tk->next;
        }
        else
        {
            separator = T_SEMI;
        }

        if(i > 0)
        {
            command_t *cmd = new_command(head, i, separator);
            if(!cmd)
            {
                release_commands(cmd_head);
                return NULL;
            }
            add_command(&cmd_head, &cmd_tail, cmd);
        }
    }

    return cmd_head;
}

void print_command_list(command_t *head)
{
    command_t *cmd = head;
    int num = 0;

    while(head)
    {
        for (int i = 0; head->args[i]; i++)
            printf("%s ", head->args[i]);

        if (head->out_file)
            printf("[%s %s] ", head->is_out_append ? ">>" : ">", head->out_file);

        if (head->err_file)
            printf("[%s %s]", head->is_err_append ? "!>>" : "!>", head->err_file);

        switch (head->separator)
        {
            case T_PIPE: printf("|\n");         break;
            case T_AND:  printf("&&\n");        break;
            case T_OR:   printf("||\n");        break;
            case T_SEMI: printf(";\n");         break;
            default:     printf("(none)\n");    break;
        }

        head = head->next;
    }
}
