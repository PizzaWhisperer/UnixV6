#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "unixv6fs.h"
#include "mount.h"
#include "inode.h"
#include "filev6.h"
#include "direntv6.h"
#include "error.h"
#include "sector.h"
#include "sha.h"

#define NB_CMD (13)                 // Number of commands available.
#define UNUSED(x) (void)(x)         // Because some functions don't use the void parameter they receive.
#define MAX_INPUT_LENGTH (255)
#define MAX_PARAM (3)               // Max number of parameter the user can give.
#define SPACE " "
#define SPACE_CHAR ' '
#define EMPTY_STRING ""
#define SHELL_PROMPT ">>> "
#define STRING_END '\0'
#define ENTER '\n'
#define NB_START_IN_CHAR (48)
#define BASE (10)

/**
 * @brief the unix filesystem
 **/
struct unix_filesystem u;

/**
 * @brief typedef for a function pointer
 **/
typedef int (*shell_fct)(const char** array);

/**
 * @brief structure for a shell command
 **/
struct shell_map {
    const char* name;   // Name of the command.
    shell_fct fct;      // Fonction doing the job.
    const char* help;   // Description of the command.
    size_t argc;        // Number of arguments for the command.
    const char* args;   // Description of the arguments of the command.
};

/**
 * @brief array of error messages
 **/
const char* const SHELL_ERR_MESSAGES[] = {
    "", // no error
    "invalid command",
    "wrong number of arguments",
    "mount the FS before the operation",
    "cat on a directory is not defined"
};

/**
 * @brief enumeration for the possible errors
 **/
enum shell_error_codes {
    ERR_INVALID_CMD = 1,
    ERR_ARGS,
    ERR_MOUNT,
    ERR_DIR_CAT
};

/// ====================================================================
/// =PROTOTYPES=========================================================
/// ====================================================================

/**
 * @brief check for a command if the number of given parameters is correct
 * @return 0 on succes, > 0 SHELL error, < 0 on FS error
 **/
int check_args(int cmd_nb, int argc);

/**
 * @brief splits input into arguments and fills in the given array
 * @param input to split
 * @param the array to fill
 * @return the number of arguments
 **/
int tokenize_input(char* input, char** args);

/**
 * @brief display help
 * @return 0 on succes, > 0 SHELL error, < 0 on FS error
 */
int do_help(const char** array);
/**
 * @brief exit
 * @return 0 on succes, > 0 SHELL error, < 0 on FS error
 */
int do_exit(const char** array);
/**
 * @brief exit
 * @return 0 on succes, > 0 SHELL error, < 0 on FS error
 */
//int do_quit(const char** array);
/**
 * @brief create a new filesystem
 * @return 0 on succes, > 0 SHELL error, < 0 on FS error
 */
int do_mkfs(const char** array);
/**
 * @brief mount the provided filesystem
 * @return 0 on succes, > 0 SHELL error, < 0 on FS error
 */
int do_mount(const char** array);
/**
 * @brief create a new directory
 * @return 0 on succes, > 0 SHELL error, < 0 on FS error
 */
int do_mkdir(const char** array);
/**
 * @brief list all directories and files contained in the currently mounted filesystem
 * @return 0 on succes, > 0 SHELL error, < 0 on FS error
 */
int do_lsall(const char** array);
/**
 * @brief add a new file
 * @return 0 on succes, > 0 SHELL error, < 0 on FS error
 */
int do_add(const char** array);
/**
 * @brief display the content of a file
 * @return 0 on succes, > 0 SHELL error, < 0 on FS error
 */
int do_cat(const char** array);
/**
 * @brief display information about the provided inode
 * @return 0 on succes, > 0 SHELL error, < 0 on FS error
 */
int do_istat(const char** array);
/**
 * @brief display the inode number of a file
 * @return 0 on succes, > 0 SHELL error, < 0 on FS error
 */
int do_inode(const char** array);
/**
 * @brief display the SHA of a file
 * @return 0 on succes, > 0 SHELL error, < 0 on FS error
 */
int do_sha(const char** array);
/**
 * @brief Print SuperBlock of the currently mounted filesystem
 * @return 0 on succes, > 0 SHELL error, < 0 on FS error
 */
int do_psb(const char** array);

/// ====================================================================
/// ====================================================================
/// ====================================================================

/**
 * @brief array of all the shell commands
 **/
struct shell_map shell_cmds[NB_CMD] = {
    { "help", do_help, "display this help.", 0, ""},
    { "exit", do_exit, "exit shell.", 0, ""},
    { "quit", do_exit, "exit shell.", 0, ""},
    { "mkfs", do_mkfs, "create a new filesystem.", 3, "<diskname> <#inodes> <#blocks>"},
    { "mount", do_mount, "mount the provided filesystem.", 1, "<diskname>"},
    { "mkdir", do_mkdir, "create a new directory.", 1, "<dirname>"},
    { "lsall", do_lsall, "list all directories and files contained in the currently mounted filesystem.", 0, ""},
    { "add", do_add, "add a new file.", 2, "<src-fullpath> <dst>"},
    { "cat", do_cat, "display the content of a file.", 1, "<pathname>"},
    { "istat", do_istat, "display information about the provided inode.", 1, "<inode_nr>"},
    { "inode", do_inode, "display the inode number of a file.", 1, "<pathname>"},
    { "sha", do_sha, "display the SHA of a file.", 1, "<pathname>"},
    { "psb", do_psb, "Print SuperBlock of the currently mounted filesystem.", 0, ""}
};

/// ====================================================================
/// =DEFINITIONS========================================================
/// ====================================================================

int check_args(int cmd_nb, int argc)
{
    size_t nb_args = shell_cmds[cmd_nb].argc;

    if (argc != (int) nb_args) {
        return ERR_ARGS;
    }

    return 0;
}

int do_exit(const char** array)
{
    UNUSED(array);

    if (u.f != NULL) {
        int err = umountv6(&u);
        if (!err) {
            exit(0);
        } else {
            exit(1);
        }
    }

    return 0;
}

/*
int do_quit(const char** array)
{
    UNUSED(array);

    return do_exit(array);
}
*/

int do_help(const char** array)
{
    UNUSED(array);

    for (int i = 0; i < NB_CMD; ++i) {
        printf("-> %s %s: %s\n", shell_cmds[i].name, shell_cmds[i].args, shell_cmds[i].help);
    }
    fflush(stdout);

    return 0;
}

int do_mount(const char** array)
{
    M_REQUIRE_NON_NULL(array);

    if (u.f != NULL) {
        umountv6(&u);
    }

    return mountv6(array[0], &u);
}

int do_lsall(const char** array)
{
    UNUSED(array);

    if (u.f == NULL) {
        return ERR_MOUNT;
    }

    return direntv6_print_tree(&u, ROOT_INUMBER, EMPTY_STRING);
}

int do_psb(const char** array)
{
    UNUSED(array);

    if (u.f == NULL) {
        return ERR_MOUNT;
    }

    mountv6_print_superblock(&u);

    return 0;
}

int do_cat(const char** array)
{
    M_REQUIRE_NON_NULL(array);

    if (u.f == NULL) {
        return ERR_MOUNT;
    }

    int inr = direntv6_dirlookup(&u, ROOT_INUMBER, array[0]);
    if (inr < 0) {

        return inr;
    } else {

        struct inode inode;

        unsigned char buffer[SECTOR_SIZE + 1];
        buffer[0] = '\0';

        int err = inode_read(&u, (uint16_t)inr, &inode);
        if (err < 0) {
            return err;
        }

        if (inode.i_mode & IALLOC) {
            if (inode.i_mode & IFDIR) {

                return ERR_DIR_CAT;
            } else {

                for (int offset = 0; offset < (inode_getsize(&inode) - 1)/ SECTOR_SIZE + 1; ++offset) {
                    int sector = inode_findsector(&u, &inode, offset);
                    if (sector < 0) {
                        return sector;
                    }
                    memset(buffer, 0, SECTOR_SIZE);


                    int j = sector_read(u.f, (uint32_t) sector, buffer);
                    if (j < 0) {
                        return j;
                    }

                    buffer[SECTOR_SIZE] = '\0';
                    printf("%s", buffer);
                }

                printf("\n");
                fflush(stdout);
            }
        }
    }

    return 0;
}

int do_sha(const char** array)
{
    M_REQUIRE_NON_NULL(array);

    if (u.f == NULL) {
        return ERR_MOUNT;
    }

    int inr = direntv6_dirlookup(&u, ROOT_INUMBER, array[0]);
    if (inr < 0) {
        return inr;
    } else {
        struct inode inode;

        int err = inode_read(&u, (uint16_t) inr, &inode);
        if (err < 0) {
            return err;
        }

        print_sha_inode(&u, inode, inr);
    }

    return 0;
}

int do_inode(const char** array)
{
    M_REQUIRE_NON_NULL(array);

    if (u.f == NULL) {
        return ERR_MOUNT;
    }

    int inr = direntv6_dirlookup(&u, ROOT_INUMBER, array[0]);
    if (inr < 0) {
        return inr;
    }

    printf("inode : %d\n", inr);
    fflush(stdout);

    return 0;
}

int do_istat(const char** array)
{
    M_REQUIRE_NON_NULL(array);

    if (u.f == NULL) {
        return ERR_MOUNT;
    }

    long int inr = strtol(array[0], NULL, BASE);
    if (inr < 0) {
        return ERR_INODE_OUTOF_RANGE;
    } else if (inr == LONG_MAX) {
        return ERR_ARGS;
    }

    struct inode inode;
    memset(&inode, 0, sizeof(struct inode));
    int err = inode_read(&u, (uint16_t) inr, &inode);
    if (err < 0) {
        return err;
    }

    inode_print(&inode);

    return 0;
}

int do_mkfs(const char** array)
{
    M_REQUIRE_NON_NULL(array);

    long int inrs = strtol(array[1], NULL, BASE);
    long int blocks = strtol(array[2], NULL, BASE);
    if (inrs < 0 || blocks < 0 || inrs == LONG_MAX || blocks == LONG_MAX) {
        return ERR_ARGS;
    }

    return mountv6_mkfs(array[0], (uint16_t) blocks, (uint16_t) inrs);
}

int do_mkdir(const char** array)
{
    M_REQUIRE_NON_NULL(array);

    int feedback = direntv6_create(&u, array[0], IFDIR | IALLOC);
    if (feedback < 0) {
        return feedback;
    }

    return 0;
}

int do_add(const char** array)
{
    M_REQUIRE_NON_NULL(array);

    while (*array[0] == PATH_TOKEN) {
        array[0] += 1;
    }

    FILE* file = fopen(array[0], "r");
    if (file == NULL) {
        return ERR_IO;
    }

    int feedback = fseek(file, 0L, SEEK_END);
    if (feedback != 0) {
        fclose(file);
        file = NULL;

        return ERR_IO;
    }

    long sizeOfFile = ftell(file);
    if (sizeOfFile < 0) {
        fclose(file);
        file = NULL;

        return ERR_IO;
    }
    rewind(file);

    unsigned char data[sizeOfFile];
    memset(data, 0, (size_t) sizeOfFile);

    if (fread(data, sizeof(char), (size_t) sizeOfFile, file) != (size_t) sizeOfFile) {
        return ERR_IO;
    }

    int inr = direntv6_create(&u, array[1], IALLOC);
    if (inr < 0) {
        return inr;
    }

    struct filev6 fileV6;
    memset(&fileV6, 0, sizeof(struct filev6));
    feedback = filev6_open(&u, (uint16_t) inr, &fileV6);
    if (feedback != 0) {
        fclose(file);
        file = NULL;

        return feedback;
    }

    feedback = filev6_writebytes(&u, &fileV6, data, (int) sizeOfFile);
    if (feedback != 0) {
        fclose(file);
        file = NULL;

        return feedback;
    }

    fclose(file);
    file = NULL;

    return 0;
}

int tokenize_input(char* input, char** args)
{
    M_REQUIRE_NON_NULL(input);
    M_REQUIRE_NON_NULL(args);

    char* inputCpy = input;
    int i = 0;

    while ((args[i] = strtok(inputCpy, SPACE)) != NULL) {
        if (i > MAX_PARAM + 1) {
            return -1;
        }

        i += 1;
        inputCpy = NULL;
    }

    return i;
}

/**
 * @brief read the user input and call the adequate method
 */
int main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    while (!feof(stdin) && !ferror(stdin)) {
        printf(SHELL_PROMPT);
        fflush(stdout);

        char input[MAX_INPUT_LENGTH + 1];
        memset(input, 0, MAX_INPUT_LENGTH + 1);

        if (fgets(input, MAX_INPUT_LENGTH + 1, stdin) == NULL || strlen(input) < 2) {
            fprintf(stderr, "ERROR SHELL: %s\n", SHELL_ERR_MESSAGES[ERR_INVALID_CMD]);
        } else {

            char* ending = strrchr(input, ENTER);
            if (ending != NULL) {
                *ending = STRING_END;
            }
            ending = strrchr(input, PATH_TOKEN);
            if (ending != NULL && *(ending + 1) == STRING_END && *(ending - 1) != SPACE_CHAR) {
                *ending = STRING_END;
            }
            ending = NULL;

            char* cmdAndArgs[MAX_INPUT_LENGTH];
            memset(cmdAndArgs, 0, MAX_INPUT_LENGTH);

            // We split the input into args
            int feedbackTok = tokenize_input(input, cmdAndArgs);
            if (feedbackTok < 0) {
                fprintf(stderr, "ERROR SHELL: %s\n", SHELL_ERR_MESSAGES[ERR_ARGS]);
            } else {

                int cmd_nb = -1;
                for (int i = 0; i < NB_CMD; ++i) {
                    if (strncmp(cmdAndArgs[0], shell_cmds[i].name, strlen(shell_cmds[i].name)) == 0) {
                        cmd_nb = i;

                        // We check if we have the correct number of args to call the method
                        int feedback = check_args(cmd_nb, feedbackTok - 1);
                        if (feedback) {
                            fprintf(stderr, "ERROR SHELL: %s\n", SHELL_ERR_MESSAGES[feedback]);
                        } else {

                            const char* args[MAX_INPUT_LENGTH + 1];
                            memset(args, 0, MAX_INPUT_LENGTH + 1);
                            for (size_t j = 1; j < MAX_PARAM + 1; ++j) {
                                args[j - 1] = cmdAndArgs[j];
                            }

                            feedback = (shell_cmds[cmd_nb].fct)(args);
                            if (feedback) { // Can be FS error or SHELL error
                                if (feedback < 0) {
                                    fprintf(stderr, "ERROR FS: %s\n", ERR_MESSAGES[feedback - ERR_FIRST]);
                                } else {
                                    fprintf(stderr, "ERROR SHELL: %s\n", SHELL_ERR_MESSAGES[feedback]);
                                }
                            }
                        }

                        i = NB_CMD + 1;
                    }
                }
                if (cmd_nb < 0) {
                    fprintf(stderr, "ERROR SHELL: %s\n", SHELL_ERR_MESSAGES[ERR_INVALID_CMD]);
                }
            }
        }
    }

    return 0;
}
