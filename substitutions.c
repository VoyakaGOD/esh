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

void expand_variables(token_t *head)
{
    while(head)
    {
        if(head->type != T_WORD)
        {
            head = head->next;
            continue;
        }

        char *begin = strchr(head->value, '`');
        if(!begin)
        {
            head = head->next;
            continue;
        }

        char *end = strchr(begin + 1, '`');
        if(!end)
        {
            head = head->next;
            continue;
        }

        size_t prefix_len = begin - head->value;
        *end = '\0';
        char *value = getenv(begin + 1);
        if(!value)
            value = "";
        size_t value_len = strlen(value);
        char *buffer = (char *)malloc(prefix_len + value_len + strlen(end + 1) + 1);
        if(!buffer)
        {
            perror("Can't allocate memory for [ENV] substitution");
            exit(-8);
        }

        char *ptr = buffer;
        ptr = memcpy(ptr, head->value, prefix_len) + prefix_len;        
        ptr = strcpy(ptr, value) + value_len;
        strcpy(ptr, end + 1);

        free(head->value);
        head->value = buffer;
    }
}
