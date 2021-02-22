#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <time.h>
#include <getopt.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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
	all 		= 1,
	not_sort 	= 2,
	long_format 	= 3,
};

static inline int 	no_hidden(const struct dirent *)
			    __attribute__ ((always_inline));
static void 		print_longformat(const char *, const char *);
static void 		print_entries_in_longfmt(struct dirent *[], int, const char *);
static int 		list_dir(const char *);
static int 		get_dir_vec(char *const [], int, char ***);
inline static void 	list_dirs(char *const [], int);

static unsigned 	options 	= 0U;
static char 		*prognam 	= NULL;

/**
 * Filter hidden entries, needed for scandir()
 */
static inline int
no_hidden(const struct dirent *ent){
	return !(ent->d_name[0] == '.');
}

/**
 * Prints a file in long format (<perms> <owner> <size> <path>)
 */
static void
print_longformat(const char *path, const char *display_name){
	static const char perms_table[8][4] = {
		[0] = "---",
		[1] = "--x",
		[2] = "-w-",
		[3] = "-wx",
		[4] = "r--",
		[5] = "r-x",
		[6] = "rw-",
		[7] = "rwx",
	};
	struct stat sbuf;
	struct passwd *pwd;
	uint8_t own_perms, grp_perms, oth_perms;
	uint32_t filetype;

	if (stat(path, &sbuf) < 0){
		warn("`stat` failed on %s", path);
		return;
	}
	
	pwd = getpwuid(sbuf.st_uid);
	if (!pwd){
		warn("`getpwuid` failed on %s", path);
		return;
	}
	
	own_perms = (sbuf.st_mode & 0700) >> 6;
	grp_perms = (sbuf.st_mode & 0070) >> 3;
	oth_perms = (sbuf.st_mode & 0007);

	filetype = (sbuf.st_mode & S_IFMT);
	switch(filetype){
	case S_IFSOCK:
		filetype = 's';
		break;
	case S_IFLNK:
		filetype = 'l';
		break;
	case S_IFREG:
		filetype = '-';
		break;
	case S_IFBLK:
		filetype = 'b';
		break;
	case S_IFDIR:
		filetype = 'd';
		break;
	case S_IFCHR:
		filetype = 'c';
		break;
	case S_IFIFO:
		filetype = 'p';
		break;
	default:
		filetype = 0;
		break;
	}

	printf("%c%s%s%s %s %*lu %s\n",
			filetype,
			perms_table[own_perms],
			perms_table[grp_perms],
			perms_table[oth_perms],
			pwd->pw_name,
			8,
			sbuf.st_size,
			display_name);
}

/**
 * Prints directory entries (the list of contents) in long format.
 *
 * @param namelist 	Array of directory entries.
 * @param dirnum 	Size of the array
 * @param dirpath 	Path to the directory
 */
static void
print_entries_in_longfmt(struct dirent *namelist[], int dirnum, const char *dirpath){
	assert(namelist != NULL);
	char *real_path, *display_name;
	size_t dirlen, tmp;
	int i;
	
	dirlen = strlen(dirpath);

	for (i = 0; i < dirnum; i++){

		display_name = namelist[i]->d_name;
		tmp = dirlen + strlen(display_name) + 2;

		real_path = malloc(tmp);
		if (!real_path)
			err(EX_OSERR, "`malloc` failed allocating %lu", tmp);

		snprintf(real_path, tmp, "%s/%s", dirpath, display_name);

		print_longformat(real_path, display_name);
		free(namelist[i]);
		free(real_path);
	}

	free(namelist);
}

/**
 * List a directory
 *
 * @return 	Number of directory entries in success, in error -1 is
 * 		returned and a warning is printed.
 */
static int
list_dir(const char *dirpath){
	int (*compar)(const struct dirent **, const struct dirent **);
	int (*filter)(const struct dirent *);
	int dirnum, i;

	struct dirent **namelist;

	filter = no_hidden;
	compar = alphasort;

	if (IS_OPT_SET(all))
		filter = NULL;

	if (IS_OPT_SET(not_sort))
		compar = NULL;

	namelist = NULL;
	dirnum = scandir(dirpath, &namelist, filter, compar);

	if (dirnum == -1)
		err(EX_OSERR, "Cannot scan directory %s", dirpath);

	if (IS_OPT_SET(long_format)){
		print_entries_in_longfmt(namelist, dirnum, dirpath);
	} else {
		for (i = 0; i < dirnum; i++){
			printf("%s\n", namelist[i]->d_name);
			free(namelist[i]);
		}
		free(namelist);
	}

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
	int i, j, dirnum;
	size_t tmp;
	char **dv;

	dv = calloc(argc - 1, sizeof(char *));

	for (i = 1, dirnum = 0; i < argc; ++i){
		if (*argv[i] != '-'){
			dv[dirnum] = strdup(argv[i]);
			++dirnum;
		}
	}

	/* remove trailing '/' */
	for (j = 0; j < dirnum; j++){
		tmp = strlen(dv[j]);

		if (dv[j][tmp - 1] == '/')
			dv[j][tmp - 1] = 0x0;
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
			"-f, --not-sort\n\tDo not sort, enables -a\n"
			"-h, --help\n\tDisplay this help and exit\n"
			"-l, --long\n\tUse long listing format\n",
			prognam);
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
		{"long", 0, NULL, 'l'},
		{"not-sort", 0, NULL, 'f'},
		{NULL,   0, NULL,  0},
	};
	char **dir_vec;
	int dirnum, i;
	int c;

	prognam = strdup(argv[0]);
	dir_vec = NULL;
	dirnum = get_dir_vec(argv, argc, &dir_vec);

	opterr = 0;
	while ((c = getopt_long(argc, argv, ":ahfl", opts, NULL)) > 0){
		switch(c){
		case 'a':
			SET_OPTION(all);
			break;
		case 'f':
			SET_OPTION(all);
			SET_OPTION(not_sort);
			break;
		case 'h':
			usage(stdout);
			exit(EXIT_SUCCESS);
			break;
		case 'l':
			SET_OPTION(long_format);
			break;
		default:
			errx(EX_USAGE, "Unknown option \"-%c\", use %s -h to "
					"see the list of valid options",
					optopt, prognam);
		}
	}

	list_dirs(dir_vec, dirnum);

	for (i = 0; i < dirnum; i++)
		free(dir_vec[i]);

	free((void*)dir_vec);
	free(prognam);

	return 0;
}
