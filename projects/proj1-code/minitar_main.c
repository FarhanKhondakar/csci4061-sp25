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

    const char *tar_archive_name = argv[3];

    // Creating A New Archive File With The Name Provided.
    if (strcmp(argv[1], "-c") == 0) {
        // Adding The Files To The List of Files.
        // Starting From The 4th Argument, as per our Usage.
        for (int i = 4; i < argc; i++) {
            if (file_list_add(&files, argv[i]) == -1) {
                perror("Failed to add file to list");
                file_list_clear(&files);
                return -1;
            }
        }

        // Creating The Archive.
        if (create_archive(tar_archive_name, &files) == -1) {
            perror("Failed to create archive");
            file_list_clear(&files);
            return -1;
        }
    }
    // Appending More Files To An Existing Archive.
    else if (strcmp(argv[1], "-a") == 0) {
        // Adding The Files To The List of Files.
        for (int i = 4; i < argc; i++) {
            if (file_list_add(&files, argv[i]) == -1) {
                perror("Failed to add file to list");
                file_list_clear(&files);
                return -1;
            }
        }

        // Appending The Files To The Archive.
        if (append_files_to_archive(tar_archive_name, &files) == -1) {
            perror("Failed to append files to archive");
            file_list_clear(&files);
            return -1;
        }
    }
    // Listing Out The Name Of Each File In The Archive.
    else if (strcmp(argv[1], "-t") == 0) {
        // Getting The List of Files In The Archive.
        if (get_archive_file_list(tar_archive_name, &files) == -1) {
            perror("Failed to get archive file list");
            file_list_clear(&files);
            return -1;
        }

        // Printing The List of Files.
        node_t *curr_file = files.head;
        while (curr_file != NULL) {
            printf("%s\n", curr_file->name);
            curr_file = curr_file->next;
        }
    }
    // Updating The Existing Archive.
    else if (strcmp(argv[1], "-u") == 0) {
        // Creating A List of Files To Update.
        file_list_t files_to_update;
        file_list_init(&files_to_update);

        // Creating A List of Files In The Archive.
        // This will be used for comparison.
        file_list_t files_in_archive;
        file_list_init(&files_in_archive);

        // Getting The List of Files In The Archive.
        if (get_archive_file_list(tar_archive_name, &files_in_archive) == -1) {
            perror("Failed to get archive file list");
            file_list_clear(&files_to_update);
            file_list_clear(&files_in_archive);
            return -1;
        }

        // Adding The Files To The List of Files To Update.
        // Starting From The 4th Argument, as per our Usage.
        // If The Files Do Not Exist In The Archive, We Print An Error.
        for (int i = 4; i < argc; i++) {
            if (file_list_contains(&files_in_archive, argv[i])) {
                if (file_list_add(&files_to_update, argv[i]) == -1) {
                    perror("Failed to add file to list");
                    file_list_clear(&files_to_update);
                    file_list_clear(&files_in_archive);
                    return -1;
                }
            } else {
                printf(
                    "Error: One or more of the specified files is not already present in "
                    "archive \n");
                file_list_clear(&files_to_update);
                file_list_clear(&files_in_archive);
                return -1;
            }
        }

        // Checking if the files to update are a subset of the files in the archive.
        // If they are, we append the files to the archive, essentially updating the archive.
        if (file_list_is_subset(&files_to_update, &files_in_archive)) {
            if (append_files_to_archive(tar_archive_name, &files_to_update) == -1) {
                perror("Failed to append files to archive");
                file_list_clear(&files_to_update);
                file_list_clear(&files_in_archive);
                return -1;
            }
        } else {
            printf(
                "Error: One or more of the specified files is not already present in archive \n");
            file_list_clear(&files_to_update);
            file_list_clear(&files_in_archive);
            return -1;
        }

        // Clearing The Lists.
        file_list_clear(&files_to_update);
        file_list_clear(&files_in_archive);
    }
    // Extracting The Files From The Archive.
    else if (strcmp(argv[1], "-x") == 0) {
        if (extract_files_from_archive(tar_archive_name) == -1) {
            perror("Failed to extract archive");
            file_list_clear(&files);
            return -1;
        }
    } else {
        printf("Usage: %s -c|a|t|u|x -f ARCHIVE [FILE...]\n", argv[0]);
        file_list_clear(&files);
        return -1;
    }

    file_list_clear(&files);
    return 0;
}
