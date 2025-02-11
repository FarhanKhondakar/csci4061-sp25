#include <stdio.h>
#include <string.h>

#include "file_list.h"
#include "minitar.h"

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: %s -c|a|t|u|x -f ARCHIVE [FILE...]\n", argv[0]);
        return 0;
    }

    file_list_t files;
    file_list_init(&files);

    // TODO: Parse command-line arguments and invoke functions from 'minitar.h'
    // to execute archive operations

    const char *tar_archive_name = argv[3];

    if (strcmp(argv[1], "-c") == 0) {
        for (int i = 4; i < argc; i++) {
            if (file_list_add(&files, argv[i]) == -1) {
                perror("Failed to add file to list");
                file_list_clear(&files);
                return -1;
            }
        }

        if (create_archive(tar_archive_name, &files) == -1) {
            perror("Failed to create archive");
            file_list_clear(&files);
            return -1;
        }
    }

    

    file_list_clear(&files);
    return 0;
}
