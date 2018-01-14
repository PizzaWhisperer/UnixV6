#include "unixv6fs.h"
#include "mount.h"
#include "inode.h"
#include "filev6.h"
#include "direntv6.h"
#include "error.h"
#include "sector.h"
#include "sha.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define NB_CMD 13               // Number of commands available.
#define UNUSED(x) (void)(x)     // Because some functions don't use the void parameter they receive.
#define MAX_INPUT_LENGTH 255
#define MAX_PARAM 3             // Max number of parameter the user can give.
#define SPACE " "
#define EMPTY_STRING ""
#define NB_START_IN_CHAR 48

//correcteur : globalement une bonne gestion des casting pour les appels de fonctions (1 point bonus)

/**
 * @brief check for a command if the number of given parameters is correct
 **/
int check_args(int cmd_nb, int argc);

/**
 * @brief splits input into arguments and fills in the given array
 * @param input to split
 * @param the array to fill
 **/
int tokenize_input(char* input, char** args);

const char * const SHELL_ERR_MESSAGES[] = {
    "", // no error
    "invalid command",
    "wrong number of arguments",
    "mount the FS before the operation",
    "cat on a directory is not defined"
};

enum shell_error_codes {
    ERR_INVALID_CMD = 1,
    ERR_ARGS,
    ERR_MOUNT,
    ERR_DIR_CAT
};

typedef int (*shell_fct)(const char** array);

/**
 * @brief display help
 */
int do_help(const char** array);
/**
 * @brief exit
 */
int do_exit(const char** array);
/**
 * @brief exit
 */
int do_quit(const char** array);
/**
 * @brief create a new filesystem
 */
int do_mkfs(const char** array);
/**
 * @brief mount the provided filesystem
 */
int do_mount(const char** array);
/**
 * @brief create a new directory
 */
int do_mkdir(const char** array);
/**
 * @brief list all directories and files contained in the currently mounted filesystem
 */
int do_lsall(const char** array);
/**
 * @brief add a new file
 */
int do_add(const char** array);
/**
 * @brief display the content of a file
 */
int do_cat(const char** array);
/**
 * @brief display information about the provided inode
 */
int do_istat(const char** array);
/**
 * @brief display the inode number of a file
 */
int do_inode(const char** array);
/**
 * @brief display the SHA of a file
 */
int do_sha(const char** array);
/**
 * @brief Print SuperBlock of the currently mounted filesystem
 */
int do_psb(const char** array);

struct unix_filesystem u;

struct shell_map {
    const char* name;   // Name of the command.
    shell_fct fct;      // Fonction doing the job.
    const char* help;   // Description of the command.
    size_t argc;        // Number of arguments for the command.
    const char* args;   // Description of the arguments of the command.
};

struct shell_map shell_cmds[NB_CMD] = {
    { "help", do_help, "display this help.", 0, ""},
    //correcteur : il aurait été malin d'utiliser le même pointeur de fonction pour les commandes help et exit (puisqu'elles sont fonctionnellement identiques) (-1)
    { "exit", do_exit, "exit shell.", 0, ""},
    { "quit", do_quit, "exit shell.", 0, ""},
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

    if(u.f != NULL) {
        //correcteur : vous devriez propager donner comme argument à exit la valeur de retour de unmountv6 (de cette façon on sait quand le programme termine si le unmount s'est vraiment bien executé) (-0.5)
        umountv6(&u);
    }

    exit(0);
    return 0;
}

int do_quit(const char** array)
{
    UNUSED(array);

    if(u.f != NULL) {
        umountv6(&u);
    }

    exit(0);
    return 0;
}

int do_help(const char** array)
{
    UNUSED(array);

    for(int i = 0; i < NB_CMD; ++i) {
        printf("-> %s %s: %s\n", shell_cmds[i].name, shell_cmds[i].args, shell_cmds[i].help);
    }
    fflush(stdout);

    return 0;
}

int do_mount(const char** array)
{
    if(u.f != NULL) {
        umountv6(&u);
    }

    return mountv6(array[0], &u);
}

int do_lsall(const char** array)
{
    UNUSED(array);

    if(u.f == NULL) {
        return ERR_MOUNT;
    }

    return direntv6_print_tree(&u, ROOT_INUMBER, EMPTY_STRING);
}

int do_psb(const char** array)
{
    UNUSED(array);

    if(u.f == NULL) {
        return ERR_MOUNT;
    }

    mountv6_print_superblock(&u);

    return 0;
}

int do_cat(const char** array)
{
    if(u.f == NULL) {
        return ERR_MOUNT;
    }

    int inr = direntv6_dirlookup(&u, ROOT_INUMBER, array[0]);
    if(inr < 0) {

        return inr;
    } else {
        //correcteur : à nouveau, il aurait été beaucoup plus judicieux d'uiltiser filev6_open et filev6_readblock pour pouvoir lire le contenu du file, secrtor_read et findsector y sont gérés correctement. Pensez à utilisez les fonctions qu'on vous a demandé d'implémenter pour des jobs pareils qui semblent très similaires (-1)
        struct inode inode;

        unsigned char buffer[SECTOR_SIZE + 1];
        buffer[0] = '\0';

        int err = inode_read(&u, (uint16_t)inr, &inode);
        if(err < 0) {

            return err;
        }

        if(inode.i_mode & IALLOC) {
            if(inode.i_mode & IFDIR) {

                return ERR_DIR_CAT;
            } else {

                for (int offset = 0; offset <= inode_getsize(&inode) / SECTOR_SIZE; ++offset) {
                    int sector = inode_findsector(&u, &inode, offset);
                    if(sector < 0) {

                        return sector;
                    }

                    memset(buffer, 0, SECTOR_SIZE);

                    int j = sector_read(u.f, (uint32_t) sector, buffer);
                    if(j < 0) {
                        return sector;
                    }
                    buffer[SECTOR_SIZE] = '\0';

                    printf("%s\n", buffer);
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
    if(u.f == NULL) {
        return ERR_MOUNT;
    }

    int inr = direntv6_dirlookup(&u, ROOT_INUMBER, array[0]);
    if(inr < 0) {

        return inr;
    } else {
        struct inode inode;

        int err = inode_read(&u, (uint16_t) inr, &inode);
        if(err< 0) {
            return err;
        }

        print_sha_inode(&u, inode, inr);
    }

    return 0;
}

int do_inode(const char** array)
{
    if(u.f == NULL) {
        return ERR_MOUNT;
    }

    int inr = direntv6_dirlookup(&u, ROOT_INUMBER, array[0]);
    if(inr < 0) {
        return inr;
    }

    printf("inode : %d\n", inr);
    fflush(stdout);

    return 0;
}

int do_istat(const char** array)
{
    //correcteur : il y a une fonction standard (atoi) pour faire ceci, vous n'avez pas l'assurance que cette macro vous donnera bel et bien le numéro correct, (selon le formattage ou le setlocale) (-0.5)
    int inr = *array[0] - NB_START_IN_CHAR;

    if(inr < 0) {
        return ERR_INODE_OUTOF_RANGE;
    }
    if(u.f == NULL) {
        return ERR_MOUNT;
    }

    struct inode inode;
    int err = inode_read(&u, (uint16_t) inr, &inode);
    if (err < 0) {
        return err;
    }

    inode_print(&inode);

    return 0;
}
int do_mkfs(const char** array)
{
    UNUSED(array);

    return 0;
}
int do_mkdir(const char** array)
{
    UNUSED(array);

    return 0;
}
int do_add(const char** array)
{
    UNUSED(array);

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
        printf(">>> ");
        fflush(stdout);

        char* input = calloc(1, MAX_INPUT_LENGTH + 1);

        input = fgets(input, MAX_INPUT_LENGTH + 1, stdin);
        if (input == NULL) {
            fprintf(stderr, "ERROR SHELL: %s\n", SHELL_ERR_MESSAGES[ERR_INVALID_CMD]);
        } else {

            char* endingEnter = strrchr(input, '\n');
            if (endingEnter != NULL) {
                *endingEnter = '\0';
            }

            char* cmdAndArgs[MAX_INPUT_LENGTH];
            memset(cmdAndArgs, 0, MAX_INPUT_LENGTH);

            // We split the input into args
            int feedbackTok = tokenize_input(input, cmdAndArgs);
            if (feedbackTok < 0) {
                //correcteur : tout les printf d'ERROR SHELL que vous avez là auraient pu être modularisés dans une fonction de traiutement de l'erreur (-1)
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

        free(input);
        input = NULL;
    }

    return 0;
}
