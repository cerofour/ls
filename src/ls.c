#include <dirent.h>
#include <getopt.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sysexits.h>
#include <assert.h>
#include <err.h>

/* Turn on an option */
#define SET_OPTION(opt) \
	do { \
		options |= 1 << (opt - 1); \
	}while(0)

/* Check if an option is set */
#define IS_OPT_SET(opt) (options & 1 << (opt - 1))

/* Options */
enum {
	all = 1,
};

static void 		prettyprint_strvec(char *const [], int);
static inline int 	no_hidden(const struct dirent *)
			    __attribute__ ((always_inline));
static int 		list_dir(const char *);
static int 		get_dir_vec(char *const [], int, char ***);
inline static void 	list_dirs(char *const [], int);
static void 		free_dirvec(char *const [], int);

static unsigned options = 0U;
static char *prognam = NULL;

/**
 * Print a string vector in python style. Used only for debugging
 * purposes.
 */
static void
prettyprint_strvec(char *const strvec[], int vecsiz){
	int i;

	putchar('[');
	for (i = 0; i < vecsiz; i++){
		printf("\"%s\"", strvec[i]);
		if (i + 1 < vecsiz)
			printf(", ");
	}
	puts("]");
}

/**
 * Filter hidden entries, needed for scandir()
 */
static inline int
no_hidden(const struct dirent *ent){
	return !(ent->d_name[0] == '.');
}

/**
 * List a directory
 *
 * @return 	Number of directory entries in success, in error -1 is
 * 		returned and a warning is printed.
 */
static int
list_dir(const char *dirpath){
	int dirnum, i;
	int (*filter)(const struct dirent *);
	struct dirent **namelist;

	if (IS_OPT_SET(all)){
		filter = NULL;
	} else {
		filter = no_hidden;
	}

	namelist = NULL;
	dirnum = scandir(dirpath, &namelist, filter, alphasort);

	if (dirnum == -1)
		warn("warning: cannot scan directory %s", dirpath);

	for (i = 0; i < dirnum; i++){
		printf("%s\n", namelist[i]->d_name);
		free(namelist[i]);
	}

	free(namelist);

	return dirnum;
}

/**
 * Gets all valid (if there are) directory names in the argument vector.
 * A valid directory name is a NUL-terminated string that doesn't begin
 * with a '-' (dash).
 * If no directory names are found, the value of $PWD is stored in the
 * buffer.
 *
 * @param argv 		Argument vector.
 * @param argc 		Is the argument count.
 * @param dir_vec 	Buffer to store directory names. Both, the pointer
 *  			and directory names are dinamically allocated
 *  			and should be freed by the caller.
 * @return 		Number of directory names stored.
 */
static int
get_dir_vec(char *const argv[], int argc, char ***dir_vec){
	char **dv;
	int i, dirnum;

	dv = calloc(argc - 1, sizeof(char *));

	for (i = 1, dirnum = 0; i < argc; ++i){
		if (*argv[i] != '-'){
			dv[dirnum] = strdup(argv[i]);
			++dirnum;
		}
	}
	
	/* No directory names found in argv */
	if (!dirnum){
		dv = realloc(dv, 1UL);
		*dv = strdup(getenv("PWD"));
		++dirnum;
	}

	*dir_vec = dv;

	return dirnum;
}

/**
 * Lists all directories in dirv.
 */
inline static void
list_dirs(char *const dirv[], int dirnum){
	int i;

	for (i = 0; i < dirnum; i++)
		list_dir(dirv[i]);
}

static void
usage(FILE *stream){
	fprintf(stream, "Usage: %s [OPTIONS]... [FILE]...\n"
			"-a, --all\n\tDo not ignore entries starting with '.'\n"
			"-h, --help\n\tDisplay this help and exit\n",
			prognam);
}

static void
free_dirvec(char *const dirvec[], int dirnum){
	int i;
	for (i = 0; i < dirnum; i++)
		free(dirvec[i]);
	free((void*)dirvec);
}

/**
 * Directory listing. Similar to ls(1).
 *
 * TODO: Fix memory leaks.
 * 	 Add more options.
 */
int
main(int argc, char *argv[]){
	static const struct option opts[] = {
		{"all",  0, NULL, 'a'},
		{"help", 0, NULL, 'h'},
		{NULL,   0, NULL,  0},
	};
	char **dir_vec;
	int dirnum;
	int c;

	prognam = strdup(argv[0]);
	dir_vec = NULL;
	dirnum = get_dir_vec(argv, argc, &dir_vec);

	opterr = 0;
	while ((c = getopt_long(argc, argv, ":ah", opts, NULL)) > 0){
		switch(c){
		case 'a':
			SET_OPTION(all);
			break;
		case 'h':
			usage(stdout);
			exit(EXIT_SUCCESS);
		default:
			errx(EX_USAGE, "Unknown option, use %s -h to see"
					"the list of valid options", prognam);
		}
	}

	list_dirs(dir_vec, dirnum);
	free_dirvec(dir_vec, dirnum);

	return 0;
}
