#ifndef PARSER_H
#define PARSER_H

#define CMD_PREFIX "imu"
#define MAX_ARGS 12
#define MAX_KEY_LEN 32
#define MAX_VALUE_LEN 64
#define MAX_COMMAND_LEN 32

#include <Arduino.h>
#include <string.h>

typedef enum
{
    CMD_ARG_FLAG,
    CMD_ARG_kEY_VALUE,
    CMD_ARG_POSITIONAL
} cmd_arg_type_t;

typedef struct
{
    cmd_arg_type_t type;
    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];
} cmd_arg_t;

typedef struct
{
    char command[MAX_COMMAND_LEN];
    cmd_arg_t args[MAX_ARGS];
    int arg_count;
} parsed_command_t;

typedef enum
{
    CMD_PARSE_OK = 0,
    CMD_PARSE_NULL,
    CMD_PARSE_EMPTY,
    CMD_PARSE_BAD_PREFIX,
    CMD_PARSE_EMPTY_COMMAND,
    CMD_PARSE_TOO_MANY_ARGS,
    CMD_PARSE_ARG_TOO_LONG,
    CMD_PARSE_BAD_ARG
} cmd_parse_status_t;

class CommandParser
{
public:
    static cmd_parse_status_t parse(const char *input, parsed_command_t *output)
    {
        if (input == nullptr || output == nullptr)
            return CMD_PARSE_NULL;

        memset(output, 0, sizeof(parsed_command_t));
        const char *ptr = skip_spaces(input);

        if (*ptr == '\0')
            return CMD_PARSE_EMPTY;

        const char *token_end = find_token_end(ptr);
        size_t first_token_len = static_cast<size_t>(token_end - ptr);

        // check for the colon first
        const char *colon_char = static_cast<const char *>(memchr(ptr, ':', first_token_len));
        if (colon_char == nullptr)
            return CMD_PARSE_BAD_PREFIX;

        size_t prefix_len = static_cast<size_t>(colon_char - ptr);
        if (prefix_len != strlen(CMD_PREFIX) || strncmp(ptr, CMD_PREFIX, prefix_len) != 0)
            return CMD_PARSE_BAD_PREFIX;

        // command is after the ":"
        ptr = colon_char + 1;
        size_t command_len = static_cast<size_t>(token_end - ptr);
        if (command_len == 0)
            return CMD_PARSE_EMPTY_COMMAND;
        if (!copy_span(output->command, sizeof(output->command), ptr, command_len))
        {
            return CMD_PARSE_ARG_TOO_LONG;
        }

        ptr = token_end;

        // note potential infinate loop ...
        while (1)
        {
            ptr = skip_spaces(ptr);

            if (*ptr == '\0')
            {
                break;
            }

            token_end = find_token_end(ptr);
            size_t arg_len = (size_t)(token_end - ptr);

            if (output->arg_count >= MAX_ARGS)
            {
                return CMD_PARSE_TOO_MANY_ARGS;
            }

            cmd_parse_status_t status =
                parse_arg(ptr, arg_len, &output->args[output->arg_count]);

            if (status != CMD_PARSE_OK)
            {
                return status;
            }

            output->arg_count++;
            ptr = token_end;
        }

        return CMD_PARSE_OK;
    }

private:
    static const char *skip_spaces(const char *str)
    {
        while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n')
            str++;
        return str;
    }

    static const char *find_token_end(const char *str)
    {
        while (*str != '\0' &&
               *str != ' ' &&
               *str != '\t' &&
               *str != '\r' &&
               *str != '\n')
        {
            str++;
        }
        return str;
    }

    static bool copy_span(char *dst, size_t dst_size, const char *src, size_t len)
    {
        if (dst == nullptr || src == nullptr || dst_size == 0 || len >= dst_size)
        {
            return false;
        }

        if (len == 0)
        {
            dst[0] = '\0';
            return true;
        }

        memcpy(dst, src, len);
        dst[len] = '\0';
        return true;
    }

    static cmd_parse_status_t parse_arg(const char *arg_str, size_t len, cmd_arg_t *arg_out)
    {

        memset(arg_out, 0, sizeof(*arg_out));
        if (len >= 2 && arg_str[0] == '-' && arg_str[1] == '-')
        {
            const char *opt = arg_str + 2;
            size_t opt_len = len - 2;

            if (opt_len == 0)
                return CMD_PARSE_BAD_ARG;
            const char *equal_char = static_cast<const char *>(memchr(opt, '=', opt_len));
            if (equal_char != nullptr)
            {
                if ((size_t)(equal_char - opt) == 0 || size_t(opt_len - (equal_char - opt) - 1) == 0)
                    return CMD_PARSE_BAD_ARG;
                if (!copy_span(arg_out->key, sizeof(arg_out->key), opt, equal_char - opt))
                {
                    return CMD_PARSE_ARG_TOO_LONG;
                }
                if (!copy_span(arg_out->value, sizeof(arg_out->value), equal_char + 1, opt_len - (equal_char - opt) - 1))
                {
                    return CMD_PARSE_ARG_TOO_LONG;
                }
                arg_out->type = CMD_ARG_kEY_VALUE;
            }
            else
            {
                if (!copy_span(arg_out->key, sizeof(arg_out->key), opt, opt_len))
                {
                    return CMD_PARSE_ARG_TOO_LONG;
                }
                arg_out->type = CMD_ARG_FLAG;
            }
        }
        else
        {
            if (!copy_span(arg_out->value, sizeof(arg_out->value), arg_str, len))
            {
                return CMD_PARSE_ARG_TOO_LONG;
            }
            arg_out->type = CMD_ARG_POSITIONAL;
            arg_out->key[0] = '\0';
        }
        return CMD_PARSE_OK;
    };
};

#endif // PARSER_H