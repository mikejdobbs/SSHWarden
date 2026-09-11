# SSHWarden

`SSHWarden` is an `sshd(8)` forced-command wrapper that authorizes requested SSH commands against a file of POSIX extended regular expressions.

## Usage

### Validate regex file only

```sh
SSHWarden REGEX_FILE
```

- Validates `REGEX_FILE` when `SSH_ORIGINAL_COMMAND` is unset.
- If `SSH_ORIGINAL_COMMAND` is set (e.g., during an SSH session), validates the file and authorizes/executes the command if it matches a pattern.

### Test a command locally

```sh
SSHWarden REGEX_FILE TEST_COMMAND
```

- Validates `REGEX_FILE` and tests `TEST_COMMAND` without executing it.

## `authorized_keys` Integration

Example entry in `~/.ssh/authorized_keys`:

```text
command="/usr/local/bin/SSHWarden /etc/ssh/allowed_commands.regex",\
no-port-forwarding,no-X11-forwarding,no-agent-forwarding,no-pty \
ssh-ed25519 AAAA... user@host
```

## Regex File Format

- One POSIX extended regular expression per line.
- Blank lines are ignored.
- Lines whose first non-whitespace character is `#` are treated as comments.
- Each usable line is compiled as a POSIX extended regular expression.

## Exit Status

| Code | Meaning |
|------|---------|
| `0` | Regex file is valid and either:<br>- validation-only mode completed successfully;<br>- `TEST_COMMAND` matched a pattern; or<br>- `SSH_ORIGINAL_COMMAND` matched and was executed successfully. |
| `1` | Regex file is valid, but `TEST_COMMAND` or `SSH_ORIGINAL_COMMAND` did not match any pattern. |
| `2` | Invalid command line. |
| `3` | File, allocation, regex compilation, read, or exec error. |

## Building

This project uses CMake.

```sh
# Clone or unpack the source, then:
mkdir build && cd build
cmake ..
make
sudo make install
```

This produces and installs the `SSHWarden` binary (typically to `/usr/local/bin/SSHWarden`).

### Requirements

- A C compiler (e.g., `cc` or `gcc`)
- CMake 3.10 or newer
- POSIX environment with `getline()`, `regcomp()`, and related functions (standard on FreeBSD, Linux, macOS)
