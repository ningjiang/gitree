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
 */
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SUBDIRNO 4096

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

static char git_tree_nonbare[][128] = {
	/* Allowed nonbare git trees */
	"manifests",
	"repo",
};

static int git_files_array_size
	= sizeof(git_files) / sizeof(git_files[0]);
static int git_tree_nonbare_array_size
	= sizeof(git_tree_nonbare) / sizeof(git_tree_nonbare[0]);
static int sum, sum2;
static int stage1, stage2, stage3;

static void usage(void)
{
	fprintf(stderr, "Usage:\n"
			"./gitree -1 pathname\n"
			"./gitree -2 pathname\n"
			"./gitree -3 pathname\n"
			"1 - Perform Git repository layout conformance check\n"
			"2 - Search for all non-bare Git trees\n"
			"3 - Find all files not in a bare Git tree\n"
			"Please perform the check in 1, 2, 3 order\n");
	exit(-1);
}

static void check_gitree(char *dirname)
{
	DIR *dirp;
	struct dirent *direntp;
	int i;

	if (!stage1)
		return ;

	printf("Checking %s\n", dirname);

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
			sum++;
			printf("WARNING: %s breaks Git repo layout rule\n",
				direntp->d_name);
		}
	}

	closedir(dirp);
}

static int in_nonbare_exp_list(char *dirname)
{
	int i;
	char *last_dir;

	last_dir = strrchr(dirname, '/');
	last_dir++;

	for (i = 0; i < git_tree_nonbare_array_size; i++) {
		if (!strcmp(last_dir, git_tree_nonbare[i]))
			break;
	}
	if (i == git_tree_nonbare_array_size)
		return 0;
	else
		return 1;
}

static void check_gitree_nonbare(char *dirname)
{
	if (!stage2)
		return ;

	if (!in_nonbare_exp_list(dirname))
		printf("%s\n", dirname);
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
	int git[SUBDIRNO], git_bare[SUBDIRNO];
	int has_dir_objects = 0, has_dir_refs = 0, skip_subtree_check = 0;

	if ((dirp = opendir(dirname)) == NULL) {
		fprintf(stderr, "gitree: opendir failed\n");
		exit(-1);
	}

	dir_len = strlen(dirname);
	while ((direntp = readdir(dirp)) != NULL) {
		if (!strcmp(direntp->d_name, "."))
			continue;
		else if (!strcmp(direntp->d_name, ".."))
			continue;
		if (direntp->d_type == DT_DIR) {
			subdir_len = strlen(direntp->d_name);
			str_len = dir_len + 1 + subdir_len;
			subdir[i] = malloc(str_len + 1);
			strcpy(subdir[i], dirname);
			strcat(subdir[i], "/");
			strcat(subdir[i], direntp->d_name);
			subdir[i][str_len] = '\0';
			if (!strncmp(direntp->d_name + subdir_len - 4, ".git", 4)) {
				git[i] = 1;
				if (subdir_len == 4)
					git_bare[i] = 0;
				else
					git_bare[i] = 1;
			} else {
				git[i] = 0;
				git_bare[i] = 0;
			}
			if (!strcmp(direntp->d_name, "objects"))
				has_dir_objects = 1;
			if (!strcmp(direntp->d_name, "refs"))
				has_dir_refs = 1;
			i++;
			if (i >= SUBDIRNO) {
				fprintf(stderr, "ERROR: gitree: reach max dir num\n");
				exit(-1);
			}
		} else if (direntp->d_type == DT_REG) {
			if (stage3 && !in_nonbare_exp_list(dirname))
				printf("%s/%s\n", dirname, direntp->d_name);
		} else if (direntp->d_type == DT_UNKNOWN) {
			fprintf(stderr, "ERROR: gitree: unknown file type\n");
			exit(-2);
		}
	}

	closedir(dirp);

	if (has_dir_objects && has_dir_refs) {
		check_gitree(dirname);
		if (stage1) {
			printf("WARNING: %s name not terminated with .git\n",
				dirname);
			sum2++;
		}
		skip_subtree_check = 1;
	}

	subdirn = i;
	for (i = 0; i < subdirn; i++) {
		if (!skip_subtree_check) {
			if (git[i] == 1) {
				check_gitree(subdir[i]);
				if (git_bare[i] == 0)
					check_gitree_nonbare(dirname);
			} else {
				if (!stage3 || !in_nonbare_exp_list(dirname))
					gitree(subdir[i]);
			}
		}
		free(subdir[i]);
	}
}

int main(int argc, char *argv[])
{
	if (argc != 3)
		usage();

	if (!strncmp(argv[1], "-1", 2))
		stage1 = 1;
	else if (!strncmp(argv[1], "-2", 2))
		stage2 = 1;
	else if (!strncmp(argv[1], "-3", 2))
		stage3 = 1;
	else
		usage();

	gitree(argv[2]);

	if (stage1)
		printf("\nCheck Result:\n"
		       "%d files break Git repo layout rule\n"
		       "%d dirs name not terminated with .git\n",
		       sum, sum2);

	return 0;
}
