/*
 * gitree can be used to:
 * 1. Check git directory structure. (dir terminated with .git,
 *    including *.git and .git, and dir having objects and refs
 *    directories)
 * 2. Find dir which has .git directory.
 * 3. Find all files which are not in a bare Git tree.
 *
 * V1: Two problems left:
 * 1. When checking a .git tree directly, it will report false
 *    warning "name not terminated with .git".
 * 2. When checking a .git tree name not terminated with .git,
 *    it will report the files not in a git tree by mistake.
 *
 * V2: Redesign the core algorithm:
 * 1. Check valid git tree through key git files.
 * 2. For git tree, perform conformance check, give warnings when
 *    1) files break Git repo layout rule,
 *    2) git dirs name not terminated with .git,
 *    3) git dirs non-bare git tree,
 *    4) files not in a git tree.
 * 3. For non git tree, print all the files under it and then
 *    continue the check with its sub directories.
 */
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SUBDIRNO 4096
#define SUBFILENO 4096

static char git_files[][128] = {
	/* git files */
	"COMMIT_EDITMSG",
	"config",
	"description",
	"FETCH_HEAD",
	"HEAD",
	"index",
	"packed-refs",
	"ORIG_HEAD",
	"MERGE_HEAD",
	"MERGE_MODE",
	"MERGE_MSG",
	"MERGE_RR",
	"RENAMED-REF",
	"gitk.cache",
	/* git dirs */
	"hooks",
	"info",
	"logs",
	"objects",
	"rebase-apply",
	"refs",
	"branches",
	"remotes",
	"shallow",
	"rr-cache",

	/* getweb */
	"cloneurl",

	/* repo files */
	".repopickle_config",
	"clone.bundle",

	/* other files */
	"config.bak",
	"config_bak",
	"config~",
	"description~",
	"hooks_bk",
	"hooks.bak",
	"hooks-bak",
	"COMMIT_EDITMSG~",
	".gitignore",
	"pnt",
	"svn",
	"temp.patch",
};

static char exception_list[][128] = {
	"/git/android/.repo",
};

static int git_files_array_size
	= sizeof(git_files) / sizeof(git_files[0]);
static int exception_list_array_size
	= sizeof(exception_list) / sizeof(exception_list[0]);
static int sum_break_layout_rule, sum_dir_name_not_with_git,
	   sum_non_bare_git, sum_not_in_git;

static void usage(void)
{
	fprintf(stderr, "Usage: ./gitree pathname\n"
			"Perform conformance check, give warnings when\n"
			"1. files break Git repo layout rule\n"
			"2. git dirs name not terminated with .git\n"
			"3. git dirs non-bare git tree\n"
			"4. files not in a git tree\n");
	exit(-1);
}

static int in_exception_list(char *dirname)
{
	int i, str_len;

	for (i = 0; i < exception_list_array_size; i++) {
		str_len = strlen(exception_list[i]);
		if (!strncmp(dirname, exception_list[i], str_len))
			break;
	}
	if (i == exception_list_array_size)
		return 0;
	else
		return 1;
}

static void check_gitree(char *dirname)
{
	char *last_dir;
	int dir_name_with_git = 0;
	int dir_len;
	DIR *dirp;
	struct dirent *direntp;
	int i;

	last_dir = strrchr(dirname, '/');
	if (last_dir == NULL)
		last_dir = dirname;
	else
		last_dir++;

	dir_len = strlen(last_dir);

	if ((dir_len >= 4) && !strncmp(last_dir + dir_len - 4, ".git", 4))
		dir_name_with_git = 1;

	if ((dir_name_with_git == 1) && (dir_len == 4)) {
		if (!in_exception_list(dirname)) {
			sum_non_bare_git++;
			printf("WARNING: %s non-bare git tree\n", dirname);
		}
	}

	if (!dir_name_with_git) {
		sum_dir_name_not_with_git++;
		printf("WARNING: %s name not terminated with .git\n",
			dirname);
	}

	if ((dirp = opendir(dirname)) == NULL) {
		fprintf(stderr, "check_gitree: opendir failed\n");
		exit(-1);
	}

	while ((direntp = readdir(dirp)) != NULL) {
		if (!strcmp(direntp->d_name, "."))
			continue;
		else if (!strcmp(direntp->d_name, ".."))
			continue;
		for (i = 0; i < git_files_array_size; i++) {
			if (!strcmp(direntp->d_name, git_files[i]))
				break;
		}
		if (i != git_files_array_size) {
			continue;
		} else {
			sum_break_layout_rule++;
			printf("WARNING: %s/%s breaks Git repo layout rule\n",
				dirname, direntp->d_name);
		}
	}

	closedir(dirp);
}

static void gitree(char *dirname)
{
	/*
	 * Parameter order reveals the development history.
	 * The last one shows the latest feature development.
	 * DO NOT change the order of the parameters.
	 */
	DIR *dirp;
	struct dirent *direntp;
	char *subdir[SUBDIRNO];
	int i = 0, subdirn, str_len, dir_len, subdir_len;
	int has_dir_objects = 0, has_dir_refs = 0;
	int has_file_HEAD = 0;
	char *subfile[SUBFILENO];
	int j = 0, subfilen, subfile_len;

	if ((dirp = opendir(dirname)) == NULL) {
		fprintf(stderr, "gitree: opendir failed\n");
		exit(-1);
	}

	printf("Checking %s\n", dirname);

	dir_len = strlen(dirname);
	while ((direntp = readdir(dirp)) != NULL) {
		if (!strcmp(direntp->d_name, "."))
			continue;
		else if (!strcmp(direntp->d_name, ".."))
			continue;
		if (direntp->d_type == DT_DIR) {
			if (!strcmp(direntp->d_name, "objects"))
				has_dir_objects = 1;
			if (!strcmp(direntp->d_name, "refs"))
				has_dir_refs = 1;

			subdir_len = strlen(direntp->d_name);
			str_len = dir_len + 1 + subdir_len;
			subdir[i] = malloc(str_len + 1);
			strcpy(subdir[i], dirname);
			strcat(subdir[i], "/");
			strcat(subdir[i], direntp->d_name);
			subdir[i][str_len] = '\0';
			i++;
			if (i >= SUBDIRNO) {
				fprintf(stderr, "ERROR: gitree: reach max dir num\n");
				exit(-1);
			}
		} else if (direntp->d_type == DT_REG) {
			if (!strcmp(direntp->d_name, "HEAD"))
				has_file_HEAD = 1;

			subfile_len = strlen(direntp->d_name);
			subfile[j] = malloc(subfile_len + 1);
			strcpy(subfile[j], direntp->d_name);
			subfile[j][subfile_len] = '\0';
			j++;
			if (j >= SUBFILENO) {
				fprintf(stderr, "ERROR: gitree: reach max file num\n");
				exit(-1);
			}
		} else if (direntp->d_type == DT_UNKNOWN) {
			fprintf(stderr, "ERROR: gitree: unknown file type\n");
			exit(-2);
		}
	}

	closedir(dirp);

	subdirn = i;
	subfilen = j;

	if (has_dir_objects && has_dir_refs && has_file_HEAD) {
		for (i = 0; i < subdirn; i++)
			free(subdir[i]);
		for (j = 0; j < subfilen; j++)
			free(subfile[j]);
		check_gitree(dirname);
	} else {
		for (j = 0; j < subfilen; j++) {
			if (!in_exception_list(dirname)) {
				sum_not_in_git++;
				printf("WARNING: %s/%s not in a git tree\n",
					dirname, subfile[j]);
			}
			free(subfile[j]);
		}
		for (i = 0; i < subdirn; i++) {
			gitree(subdir[i]);
			free(subdir[i]);
		}
	}
}

int main(int argc, char *argv[])
{
	int dir_len;

	if (argc != 2)
		usage();

	dir_len = strlen(argv[1]);
	dir_len--;
	while (argv[1][dir_len] == '/') {
		argv[1][dir_len] = '\0';
		dir_len--;
	}

	gitree(argv[1]);

	printf("\nCheck Result:\n"
	       "%d files break Git repo layout rule\n"
	       "%d git dirs name not terminated with .git\n"
	       "%d git dirs non-bare git tree\n"
	       "%d files not in a git tree\n",
	       sum_break_layout_rule, sum_dir_name_not_with_git,
	       sum_non_bare_git, sum_not_in_git);

	return 0;
}
