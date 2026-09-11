/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Michael J. Dobbs
 *
 *
 * An sshd forced-command wrapper that authorizes requested SSH commands
 * against a  regular expression in REGEX_FILE.
 *
 * Usage:
 *
 *     regexmatch REGEX_FILE
 *         Validate REGEX_FILE if SSH_ORIGINAL_COMMAND is unset.  If
 *         SSH_ORIGINAL_COMMAND is set, authorize and execute that command.
 *
 *     regexmatch REGEX_FILE TEST_COMMAND
 *         Validate REGEX_FILE and test TEST_COMMAND without executing it.
 *
 * Usage in authorized_keys:
 *
 *     command="/usr/local/bin/regexmatch /etc/ssh/allowed_commands.regex",\
 *     no-port-forwarding,no-X11-forwarding,no-agent-forwarding,no-pty \
 *     ssh-ed25519 AAAA... user@host
 *
 * Blank lines and lines whose first non-whitespace character is '#' are
 * ignored.  Each usable line is compiled as a POSIX extended regular
 * expression.
 *
 * Exit status:
 *
 *     0  Regex file is valid and either:
 *          - validation-only mode completed successfully;
 *          - TEST_COMMAND matched a pattern; or
 *          - SSH_ORIGINAL_COMMAND matched and was executed successfully.
 *     1  Regex file is valid, but TEST_COMMAND or SSH_ORIGINAL_COMMAND did
 *        not match any pattern.
 *     2  Invalid command line.
 *     3  File, allocation, regex compilation, read, or exec error.
 */

#include <ctype.h>
#include <errno.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define EXIT_MATCH    0
#define EXIT_NO_MATCH    1
#define EXIT_USAGE    2
#define EXIT_ERROR    3

enum mode {
    MODE_VALIDATE,
    MODE_TEST,
    MODE_SSH
};

static void
trim_line(char *line)
{
    char *start;
    char *end;

    start = line;
    while (isspace((unsigned char)*start))
        start++;

    if (start != line)
        memmove(line, start, strlen(start) + 1);

    end = line + strlen(line);
    while (end > line && isspace((unsigned char)end[-1]))
        *--end = '\0';
    
}

static void
usage(const char *progname)
{
    fprintf(stderr,
        "Usage: %s REGEX_FILE [TEST_COMMAND]\n"
        "\n"
        "Without TEST_COMMAND:\n"
        "  - validates REGEX_FILE when SSH_ORIGINAL_COMMAND is unset;\n"
        "  - otherwise validates and authorizes SSH_ORIGINAL_COMMAND.\n"
        "\n"
        "With TEST_COMMAND:\n"
        "  - validates REGEX_FILE and tests TEST_COMMAND without executing it.\n",
        progname);
}

int
main(int argc, char *argv[])
{
    const char *command;
    const char *filename;
    FILE *fp;
    char *line;
    size_t line_capacity;
    ssize_t line_length;
    unsigned long line_number;
    enum mode mode;
    int saw_pattern;
    int result;

    if (argc != 2 && argc != 3) {
        usage(argv[0]);
        return (EXIT_USAGE);
    }

    filename = argv[1];

    /*
     * An explicit second argument selects safe local test mode.  This mode
     * never executes a command, even if SSH_ORIGINAL_COMMAND is set.
     */
    if (argc == 3) {
        command = argv[2];
        mode = MODE_TEST;
    } else {
        command = getenv("SSH_ORIGINAL_COMMAND");

        /*
         * No SSH_ORIGINAL_COMMAND means this invocation is being used
         * to validate the regex file rather than process an SSH request.
         */
        
        if (command == NULL || command[0] == '\0')
            mode = MODE_VALIDATE;
        else
            mode = MODE_SSH;
    }

    fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Cannot open regex file '%s': %s\n",
            filename, strerror(errno));
        return (EXIT_ERROR);
    }

    line = NULL;
    line_capacity = 0;
    line_number = 0;
    saw_pattern = 0;
    result = EXIT_NO_MATCH;

    while ((line_length = getline(&line, &line_capacity, fp)) != -1) {
        regex_t regex;
        char errorbuf[256];
        int rc;

        line_number++;
        trim_line(line);
        line_length = strlen(line);

        if (line[0] == '\0' || line[0] == '#')
            continue;

        saw_pattern = 1;
        
        /* check for missing ^ or $ */
        
        if (mode != MODE_SSH) {
            if (line[0] != '^') {
                fprintf(stderr,
                    "%s:%lu: warning: regex does not start with '^': '%s'\n",
                    filename, line_number, line);
            }
            
            if (line_length == 0 || line[line_length - 1] != '$') {
                fprintf(stderr,
                    "%s:%lu: warning: regex does not end with '$': '%s'\n",
                    filename, line_number, line);
            }
        }

        rc = regcomp(&regex, line, REG_EXTENDED);
        if (rc != 0) {
            (void)regerror(rc, &regex, errorbuf, sizeof(errorbuf));
            fprintf(stderr, "%s:%lu: invalid regex '%s': %s\n",
                filename, line_number, line, errorbuf);
            result = EXIT_ERROR;
            break;
        }

        /*
         * Validation-only mode must compile every usable regex.  It
         * deliberately does not call regexec(), because no command is
         * being tested.
         */
        
        if (mode == MODE_VALIDATE) {
            regfree(&regex);
            continue;
        }

        rc = regexec(&regex, command, 0, NULL, 0);
        if (rc != 0 && rc != REG_NOMATCH) {
            (void)regerror(rc, &regex, errorbuf, sizeof(errorbuf));
            regfree(&regex);
            fprintf(stderr, "%s:%lu: regex execution error: %s\n",
                filename, line_number, errorbuf);
            result = EXIT_ERROR;
            break;
        }

        regfree(&regex);

        if (rc == 0) {
            result = EXIT_MATCH;

            /*
             * Test mode only needs to establish that one pattern
             * matches.  SSH mode also needs only one match before
             * executing the requested command.
             */
            
            break;
        }
    }

    if (ferror(fp)) {
        fprintf(stderr, "Error reading regex file '%s'\n", filename);
        result = EXIT_ERROR;
    }

    if (!saw_pattern && result != EXIT_ERROR) {
        fprintf(stderr, "Regex file '%s' contains no usable patterns\n",
            filename);
        result = EXIT_ERROR;
    }

    free(line);
    (void)fclose(fp);

    if (result == EXIT_ERROR)
        return (EXIT_ERROR);

    if (mode == MODE_VALIDATE) {
        /*
         * Every usable pattern was compiled successfully.  No command
         * was supplied or executed in validation-only mode.
         */
        return (EXIT_MATCH);
    }

    if (result == EXIT_NO_MATCH) {
        if (mode == MODE_TEST) {
            fprintf(stderr,
                "Test command does not match any allowed pattern: '%s'\n",
                command);
        } else {
            fprintf(stderr,
                "Command rejected: '%s' does not match any allowed pattern.\n",
                command);
        }
        return (EXIT_NO_MATCH);
    }

    /*
     * A local test match is deliberately non-executing.
     */
    if (mode == MODE_TEST)
        return (EXIT_MATCH);

    /*
     * A remote SSH command matched.  Replace this process with the
     * requested command.  Regexes are the security policy and should be
     * anchored with ^ and $ to approve the complete command string.
     */
    execl("/bin/sh", "sh", "-c", command, (char *)NULL);

    fprintf(stderr, "Cannot execute command '%s': %s\n", command,
        strerror(errno));
    return (EXIT_ERROR);
}
