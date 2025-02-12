#include "minitar.h"

#include <fcntl.h>
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

#define NUM_TRAILING_BLOCKS 2
#define MAX_MSG_LEN 128
#define BLOCK_SIZE 512
#define OCTAL_BASE 8

// Constants for tar compatibility information
#define MAGIC "ustar"

// Constants to represent different file types
// We'll only use regular files in this project
#define REGTYPE '0'
#define DIRTYPE '5'

/*
 * Helper function to compute the checksum of a tar header block
 * Performs a simple sum over all bytes in the header in accordance with POSIX
 * standard for tar file structure.
 */
void compute_checksum(tar_header *header) {
    // Have to initially set header's checksum to "all blanks"
    memset(header->chksum, ' ', 8);
    unsigned sum = 0;
    char *bytes = (char *) header;
    for (int i = 0; i < sizeof(tar_header); i++) {
        sum += bytes[i];
    }
    snprintf(header->chksum, 8, "%07o", sum);
}

/*
 * Populates a tar header block pointed to by 'header' with metadata about
 * the file identified by 'file_name'.
 * Returns 0 on success or -1 if an error occurs
 */
int fill_tar_header(tar_header *header, const char *file_name) {
    memset(header, 0, sizeof(tar_header));
    char err_msg[MAX_MSG_LEN];
    struct stat stat_buf;
    // stat is a system call to inspect file metadata
    if (stat(file_name, &stat_buf) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to stat file %s", file_name);
        perror(err_msg);
        return -1;
    }

    strncpy(header->name, file_name, 100);    // Name of the file, null-terminated string
    snprintf(header->mode, 8, "%07o",
             stat_buf.st_mode & 07777);    // Permissions for file, 0-padded octal

    snprintf(header->uid, 8, "%07o", stat_buf.st_uid);    // Owner ID of the file, 0-padded octal
    struct passwd *pwd = getpwuid(stat_buf.st_uid);       // Look up name corresponding to owner ID
    if (pwd == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up owner name of file %s", file_name);
        perror(err_msg);
        return -1;
    }
    strncpy(header->uname, pwd->pw_name, 32);    // Owner name of the file, null-terminated string

    snprintf(header->gid, 8, "%07o", stat_buf.st_gid);    // Group ID of the file, 0-padded octal
    struct group *grp = getgrgid(stat_buf.st_gid);        // Look up name corresponding to group ID
    if (grp == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up group name of file %s", file_name);
        perror(err_msg);
        return -1;
    }
    strncpy(header->gname, grp->gr_name, 32);    // Group name of the file, null-terminated string

    snprintf(header->size, 12, "%011o",
             (unsigned) stat_buf.st_size);    // File size, 0-padded octal
    snprintf(header->mtime, 12, "%011o",
             (unsigned) stat_buf.st_mtime);    // Modification time, 0-padded octal
    header->typeflag = REGTYPE;                // File type, always regular file in this project
    strncpy(header->magic, MAGIC, 6);          // Special, standardized sequence of bytes
    memcpy(header->version, "00", 2);          // A bit weird, sidesteps null termination
    snprintf(header->devmajor, 8, "%07o",
             major(stat_buf.st_dev));    // Major device number, 0-padded octal
    snprintf(header->devminor, 8, "%07o",
             minor(stat_buf.st_dev));    // Minor device number, 0-padded octal

    compute_checksum(header);
    return 0;
}

/*
 * Removes 'nbytes' bytes from the file identified by 'file_name'
 * Returns 0 upon success, -1 upon error
 * Note: This function uses lower-level I/O syscalls (not stdio), which we'll learn about later
 */
int remove_trailing_bytes(const char *file_name, size_t nbytes) {
    char err_msg[MAX_MSG_LEN];

    struct stat stat_buf;
    if (stat(file_name, &stat_buf) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to stat file %s", file_name);
        perror(err_msg);
        return -1;
    }

    off_t file_size = stat_buf.st_size;
    if (nbytes > file_size) {
        file_size = 0;
    } else {
        file_size -= nbytes;
    }

    if (truncate(file_name, file_size) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to truncate file %s", file_name);
        perror(err_msg);
        return -1;
    }
    return 0;
}

// Helper Functions

// Close File Function
void close_file(FILE *file, const char *msg) {
    if (fclose(file) != 0) {
        perror(msg);
    }
}

// Convert Octal String To Size_t
// This is used to calculate the size of the file, ensuring that we have the correct size.
int convert_octal_to_size_t(const char *octal_string, size_t *size) {
    char *end_pointer;

    // We Use strtoull to convert the Octal String to an Unsigned Long Long.
    // Furthermore, we use the end_pointer to check if the conversion was successful.
    *size = strtoull(octal_string, &end_pointer, OCTAL_BASE);
    if (*end_pointer != '\0') {
        perror("Failed to convert octal string to size_t");
        return -1;
    }

    return 0;
}

// Writes The Padding (of BLOCK_SIZE) To The Archive.
int write_file_padding(FILE *tar_archive, size_t padding) {
    char padding_buffer[BLOCK_SIZE] = {0};

    // Write The Padding To The Archive.
    // Else If The Padding Is Not Written, Return An Error.
    if (fwrite(padding_buffer, 1, padding, tar_archive) != padding) {
        perror("Failed to write to tar archive");
        return -1;
    }

    return 0;
}

// Writes The Footer (Two Empty Blocks) To The Archive.
int write_footer(FILE *tar_archive) {
    // Add Two Empty Blocks (Footer) To The End Of The Archive.
    char empty_footer[BLOCK_SIZE * NUM_TRAILING_BLOCKS] = {0};

    // Write The Empty Footer To The Archive.
    // If The Empty Footer Is Not Written To Exactly 1024 Bytes, Return An Error.
    if (fwrite(empty_footer, 1, BLOCK_SIZE * NUM_TRAILING_BLOCKS, tar_archive) !=
        BLOCK_SIZE * NUM_TRAILING_BLOCKS) {
        perror("Failed to write to tar archive");
        return -1;
    }

    return 0;
}

// Writes The File Contents To The Archive.
int write_file_contents(FILE *tar_archive, FILE *input_file) {
    char buffer[BLOCK_SIZE] = {0};
    size_t bytes_fetched;

    // Read The File Contents In Blocks Of 512 Bytes.
    // If Bytes Fetched is 0, We Have Reached The End Of The File.
    while ((bytes_fetched = fread(buffer, 1, BLOCK_SIZE, input_file)) > 0) {
        // Write The File Contents To The Archive.
        size_t bytes_written = fwrite(buffer, 1, bytes_fetched, tar_archive);

        // If The Bytes Written Is Not Equal To The Bytes Fetched, Return An Error.
        if (bytes_written != bytes_fetched) {
            perror("Failed to write to tar archive");
            return -1;
        }

        // If The File Contents is Less Than 512 Bytes, We need to add padding.
        // This ensures that the file contents are written in blocks of 512 bytes (following the
        // tar format).
        if (bytes_fetched < BLOCK_SIZE) {
            size_t padding = BLOCK_SIZE - bytes_fetched;

            // Write The Padding To The Archive.
            if (write_file_padding(tar_archive, padding) != 0) {
                return -1;
            }
        }
    }

    // Check If The fread Function Failed To Read The File.
    // If The Bytes Fetched Is 0 And ferror Returns An Error, Return An Error.
    if (bytes_fetched == 0 && ferror(input_file) != 0) {
        perror("Failed to read file");
        return -1;
    }

    return 0;
}

int create_archive(const char *archive_name, const file_list_t *files) {
    // Create A New Tar Archive with Write Binary Permissions.
    FILE *tar_archive = fopen(archive_name, "wb");
    if (tar_archive == NULL) {
        perror("Failed to create tar archive");
        return -1;
    }

    // Setting Current File
    node_t *curr_file = files->head;

    // Iterate Through The Files To Be Added To The Archive.
    while (curr_file != NULL) {
        tar_header archive_header;

        // Filling The Header
        // Handle Errors If The Header Is Not Filled.
        int header_status = fill_tar_header(&archive_header, curr_file->name);
        if (header_status == -1) {
            perror("Failed to fill tar header");
            close_file(tar_archive, "Failed to close tar archive");
            return -1;
        }

        // Write The Header To The Archive.
        // If The Header Is Not Written, Return An Error.
        size_t write_to_header = fwrite(&archive_header, sizeof(tar_header), 1, tar_archive);
        if (write_to_header != 1) {
            perror("Failed to write to tar archive");
            close_file(tar_archive, "Failed to close tar archive");
            return -1;
        }

        // Open The Current File To Be Added To The Archive.
        // Read Only Permisisons.
        FILE *input_file = fopen(curr_file->name, "r");
        if (input_file == NULL) {
            perror("Failed to open file");
            close_file(tar_archive, "Failed to close tar archive");
            return -1;
        }

        // Write/Copy The File Contents To The Archive.
        if (write_file_contents(tar_archive, input_file) != 0) {
            close_file(tar_archive, "Failed to close tar archive");
            close_file(input_file, "Failed to close file");
            return -1;
        }

        // Close The Current File.
        if (fclose(input_file) != 0) {
            perror("Failed to close file");
            close_file(tar_archive, "Failed to close tar archive");
            return -1;
        }

        // Move To The Next File.
        curr_file = curr_file->next;
    }

    // Write The Footer (Using The Padding Helper)
    if (write_footer(tar_archive) != 0) {
        close_file(tar_archive, "Failed to close tar archive");
        return -1;
    }

    // Close The Tar Archive.
    if (fclose(tar_archive) != 0) {
        perror("Failed to close tar archive");
        return -1;
    }

    return 0;
}

int append_files_to_archive(const char *archive_name, const file_list_t *files) {
    // Opening The Existing Tar Archive With Read/Write Permissions.
    // Note: R+B Opens an existing file, and will return NULL if the file does not exist.
    FILE *tar_archive = fopen(archive_name, "r+b");
    if (tar_archive == NULL) {
        perror("Failed to open tar archive");
        return -1;
    }

    // Removing The Trailing Bytes / Footer From The Archive.
    if (remove_trailing_bytes(archive_name, BLOCK_SIZE * NUM_TRAILING_BLOCKS) != 0) {
        perror("Failed to remove trailing bytes from archive");
        close_file(tar_archive, "Failed to close tar archive");
        return -1;
    }

    // Move The File Pointer To The End Of The Archive.
    if (fseek(tar_archive, 0, SEEK_END) != 0) {
        perror("Failed to move file pointer to the end of the archive");
        close_file(tar_archive, "Failed to close tar archive");
        return -1;
    }

    // Setting Current File
    node_t *curr_file = files->head;

    // Iterate Through The Files To Be Added To The Archive.
    while (curr_file != NULL) {
        tar_header archive_header;

        // Filling The Header
        // Handle Errors If The Header Is Not Filled.
        int header_status = fill_tar_header(&archive_header, curr_file->name);
        if (header_status == -1) {
            perror("Failed to fill tar header");
            close_file(tar_archive, "Failed to close tar archive");
            return -1;
        }

        // Write The Header To The Archive.
        // If The Header Is Not Written, Return An Error.
        size_t write_to_header = fwrite(&archive_header, sizeof(tar_header), 1, tar_archive);
        if (write_to_header != 1) {
            perror("Failed to write to tar archive");
            close_file(tar_archive, "Failed to close tar archive");
            return -1;
        }

        // Open The Current File To Be Added To The Archive.
        // Read Only Permisisons.
        FILE *input_file = fopen(curr_file->name, "r");
        if (input_file == NULL) {
            perror("Failed to open file");
            close_file(tar_archive, "Failed to close tar archive");
            return -1;
        }

        // Write/Copy The File Contents To The Archive.
        if (write_file_contents(tar_archive, input_file) != 0) {
            close_file(tar_archive, "Failed to close tar archive");
            close_file(input_file, "Failed to close file");
            return -1;
        }

        // Close The Current File.
        if (fclose(input_file) != 0) {
            perror("Failed to close file");
            if (fclose(tar_archive) != 0) {
                perror("Failed to close tar archive");
            }
            return -1;
        }

        // Move To The Next File.
        curr_file = curr_file->next;
    }

    // Write The Footer
    if (write_footer(tar_archive) != 0) {
        close_file(tar_archive, "Failed to close tar archive");
        return -1;
    }

    // Close The Tar Archive.
    if (fclose(tar_archive) != 0) {
        perror("Failed to close tar archive");
        return -1;
    }

    return 0;
}

int get_archive_file_list(const char *archive_name, file_list_t *files) {
    // Opening The Existing Tar Archive With Read Permissions.
    FILE *tar_archive = fopen(archive_name, "rb");
    if (tar_archive == NULL) {
        perror("Failed to open tar archive");
        return -1;
    }

    tar_header archive_header;
    size_t bytes_fetched;

    // Read The Archive Header.
    while ((bytes_fetched = fread(&archive_header, sizeof(archive_header), 1, tar_archive)) == 1) {
        // If The Name of The File is Empty, We Have Reached The End of The Archive.
        if (archive_header.name[0] == '\0') {
            break;
        }

        // Add The File Name To The List of Files.
        if (file_list_add(files, archive_header.name) == -1) {
            perror("Failed to add file to list");
            close_file(tar_archive, "Failed to close tar archive");
            return -1;
        }

        // Convert The Octal String To A Size_t.
        // This is used to calculate the size of the file.
        size_t file_size;
        if (convert_octal_to_size_t(archive_header.size, &file_size) != 0) {
            close_file(tar_archive, "Failed to close tar archive");
            return -1;
        }

        // Caluclating the Spacing/Padding needed to move to the next file/block.
        // If the file size is a multiple of 512, we don't need any padding, else
        // we need to add padding to reach the next block.
        size_t spacing;
        if (file_size % BLOCK_SIZE == 0) {
            spacing = 0;
        } else {
            spacing = BLOCK_SIZE - (file_size % BLOCK_SIZE);
        }

        // Move The File Pointer To The Next File.
        if (fseek(tar_archive, file_size + spacing, SEEK_CUR) != 0) {
            perror("Failed to seek in tar archive");
            close_file(tar_archive, "Failed to close tar archive");
            return -1;
        }
    }

    // Close The Tar Archive.
    if (fclose(tar_archive) != 0) {
        perror("Failed to close tar archive");
        return -1;
    }

    return 0;
}

int extract_files_from_archive(const char *archive_name) {
    // Opening The Existing Tar Archive With Read Permissions.
    FILE *tar_archive = fopen(archive_name, "rb");
    if (tar_archive == NULL) {
        perror("Failed to open tar archive");
        return -1;
    }

    tar_header archive_header;
    size_t bytes_fetched;

    while ((bytes_fetched = fread(&archive_header, sizeof(archive_header), 1, tar_archive)) == 1) {
        // If The Name of The File is Empty, We Have Reached The End of The Archive.
        if (archive_header.name[0] == '\0') {
            break;
        }

        // Convert The Octal String To A Size_t.
        // This is used to calculate the size of the file.
        size_t file_size;
        if (convert_octal_to_size_t(archive_header.size, &file_size) != 0) {
            close_file(tar_archive, "Failed to close tar archive");
            return -1;
        }

        // Open The File To Be Extracted From The Archive.
        FILE *output_file = fopen(archive_header.name, "wb");
        if (output_file == NULL) {
            perror("Failed to open file");
            close_file(tar_archive, "Failed to close tar archive");
            return -1;
        }

        // Read The File Contents From The Archive.
        char buffer[BLOCK_SIZE] = {0};
        size_t bytes_remaining = file_size;

        // Read The File Contents In Blocks Of 512 Bytes.
        // If Bytes Fetched is 0, We Have Reached The End Of The File.
        while (bytes_remaining > 0) {
            size_t bytes_to_fetch;
            if (bytes_remaining < BLOCK_SIZE) {
                bytes_to_fetch = bytes_remaining;
            } else {
                bytes_to_fetch = BLOCK_SIZE;
            }

            // Read The File Contents From The Archive/Buffer.
            size_t bytes_fetched = fread(buffer, 1, bytes_to_fetch, tar_archive);
            if (bytes_fetched != bytes_to_fetch) {
                perror("Failed to read from tar archive");
                close_file(tar_archive, "Failed to close tar archive");
                close_file(output_file, "Failed to close file");
                return -1;
            }

            // Write The File Contents To The Output File.
            size_t bytes_written = fwrite(buffer, 1, bytes_fetched, output_file);
            if (bytes_written != bytes_fetched) {
                perror("Failed to write to file");
                close_file(tar_archive, "Failed to close tar archive");
                close_file(output_file, "Failed to close file");
                return -1;
            }

            // Update The Bytes Remaining.
            bytes_remaining -= bytes_fetched;
        }

        // Handle Padding If The File Size is Not a Multiple of 512,
        // we need to add padding to reach the next block.
        if (file_size % BLOCK_SIZE != 0) {
            size_t padding = BLOCK_SIZE - (file_size % BLOCK_SIZE);
            if (fseek(tar_archive, padding, SEEK_CUR) != 0) {
                perror("Failed to seek in tar archive");
                close_file(tar_archive, "Failed to close tar archive");
                close_file(output_file, "Failed to close file");
                return -1;
            }
        }

        // Close The Output File.
        if (fclose(output_file) != 0) {
            perror("Failed to close file");
            close_file(tar_archive, "Failed to close tar archive");
            return -1;
        }
    }

    // Close The Tar Archive.
    if (fclose(tar_archive) != 0) {
        perror("Failed to close tar archive");
        return -1;
    }

    return 0;
}
