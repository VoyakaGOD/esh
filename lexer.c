#define ESH_MAX_WORD_LEN 1024

typedef enum
{
    T_WORD,
    T_PIPE,
    T_AND,
    T_OR,
    T_SEMI,
    T_REDIR_OUT_1,
    T_REDIR_ERR_1,
    T_REDIR_OUT_2,
    T_REDIR_ERR_2
} token_type_t;

typedef struct token
{
    token_type_t type;
    char *value;
    struct token *next;
} token_t;

token_t *new_token(token_type_t type, char *value)
{
    token_t *tk = malloc(sizeof(token_t));
    if (!tk)
    {
        perror("Can't allocate memory for token");
        exit(-1);
    }

    tk->type = type;
    tk->value = value ? strdup(value) : NULL;
    tk->next = NULL;

    return tk;
}

void add_token(token_t **head, token_t **tail, token_t *new)
{
    if(!*head)
        *head = new;
    else
        (*tail)->next = new;

    *tail = new;
}

void release_tokens(token_t *head)
{
    while(head)
    {
        token_t *next = head->next;
        free(head->value);
        free(head);
        head = next;
    }
}

void add_word_to_token_list(token_t **head, token_t **tail, char *word, int *j)
{
    if(*j > 0)
    {
        word[*j] = '\0';
        add_token(head, tail, new_token(T_WORD, word));
    }
    *j = 0;
}

token_t *tokenize_input(char *input)
{
    token_t *head = NULL;
    token_t *tail = NULL;

    char word[ESH_MAX_WORD_LEN + 1];
    int i = 0;  // cmd ptr
    int j = 0;  // word ptr

    while(1)
    {
        if((input[i] == ' ') || (input[i] == '\t') || (!input[i]))
        {
            add_word_to_token_list(&head, &tail, word, &j);
            if(input[i])
            {
                i++;
                continue;
            }
            else
            {
                break;
            }
        }

        if(input[i] == '|')
        {
            add_word_to_token_list(&head, &tail, word, &j);
            if(input[i + 1] == '|')
            {
                add_token(&head, &tail, new_token(T_OR, NULL));
                i += 2;
            }
            else
            {
                add_token(&head, &tail, new_token(T_PIPE, NULL));
                i++;
            }
            continue;
        }

        if((input[i] == '&') && (input[i + 1] == '&'))
        {
            add_word_to_token_list(&head, &tail, word, &j);
            add_token(&head, &tail, new_token(T_AND, NULL));
            i += 2;
            continue;
        }

        if(input[i] == ';')
        {
            add_word_to_token_list(&head, &tail, word, &j);
            add_token(&head, &tail, new_token(T_SEMI, NULL));
            i++;
            continue;
        }

        if(input[i] == '>')
        {
            add_word_to_token_list(&head, &tail, word, &j);
            if (input[i + 1] == '>')
            {
                add_token(&head, &tail, new_token(T_REDIR_OUT_2, NULL));
                i += 2;
            }
            else
            {
                add_token(&head, &tail, new_token(T_REDIR_OUT_1, NULL));
                i++;
            }
            continue;
        }

        if((input[i] == '!') && (input[i + 1] == '>'))
        {
            add_word_to_token_list(&head, &tail, word, &j);
            if (input[i + 2] == '>')
            {
                add_token(&head, &tail, new_token(T_REDIR_ERR_2, NULL));
                i += 3;
            }
            else
            {
                add_token(&head, &tail, new_token(T_REDIR_ERR_1, NULL));
                i += 2;
            }
            continue;
        }

        if(input[i] == '"')
        {
            i++;
            while((j < ESH_MAX_WORD_LEN) && input[i] && (input[i] != '"'))
                word[j++] = input[i++];
            if(input[i] == '\0')
            {
                fprintf(stderr, "You should close quote\n");
                release_tokens(head);
                return NULL;
            }
            if(j >= ESH_MAX_WORD_LEN)
            {
                fprintf(stderr, "Argument is too big\n");
                release_tokens(head);
                return NULL;
            }
            i++;
            continue;
        }

        if(j >= ESH_MAX_WORD_LEN)
        {
            fprintf(stderr, "Argument is too big\n");
            release_tokens(head);
            return NULL;
        }
        word[j++] = input[i++];
    }

    return head;
}

void print_token_list(token_t *head)
{
    while(head)
    {
        printf("{Type: ");
        switch (head->type)
        {
            case T_WORD:         printf("T_WORD        "); break;
            case T_PIPE:         printf("T_PIPE        "); break;
            case T_AND:          printf("T_AND         "); break;
            case T_OR:           printf("T_OR          "); break;
            case T_SEMI:         printf("T_SEMI        "); break;
            case T_REDIR_OUT_1:  printf("T_REDIR_OUT_1 "); break;
            case T_REDIR_ERR_1:  printf("T_REDIR_ERR_1 "); break;
            case T_REDIR_OUT_2:  printf("T_REDIR_OUT_2 "); break;
            case T_REDIR_ERR_2:  printf("T_REDIR_ERR_2 "); break;
            default:             printf("UNKNOWN       "); break;
        }

        if(head->value)
            printf("| Value: '%s'}", head->value);
        else
            printf("| Value: NULL}");

        head = head->next;
        printf("\n");
    }
}

void expand_tilde(token_t *head)
{
    char *home = getenv("HOME");
    if (!home)
        return;

    while(head)
    {
        if((head->type != T_WORD) || (head->value[0] != '~'))
        {
            head = head->next;
            continue;
        }

        if(head->value[1] == '\0')
        {
            free(head->value);
            head->value = strdup(home);
        }
        else if(head->value[1] == '/')
        {
            char *buffer = (char *)malloc(strlen(home) + strlen(head->value));
            if(!buffer)
            {
                perror("Can't allocate memory for [HOME] substitution");
                exit(-4);
            }
            sprintf(buffer, "%s%s", home, head->value + 1);
            free(head->value);
            head->value = buffer;
        }

        head = head->next;
    }
}
