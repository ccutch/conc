
#define NOB_IMPLEMENTATION
#include "nob.h"


int create_app_h_file(void);
int alphabetical_cmp(const void *a, const void *b);
void print_gen_info(Nob_String_Builder *sb);


int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);
    
    if (!nob_mkdir_if_not_exists("build")) return 1;

    // ignoring first argument (./nob)
    (void)nob_shift_args(&argc, &argv);

    char *command;
    if (argc < 1) command = "build";
    else command = nob_shift_args(&argc, &argv);

    if (strcmp(command, "build") == 0)
        return create_app_h_file();
 
    if (strcmp(command, "run") == 0) {
        if (argc < 1) {
            nob_log(NOB_ERROR, "run command requires a argument");
            return 1;
        }

        char *example = nob_shift_args(&argc, &argv);
        if (example == NULL) { 
            nob_log(NOB_ERROR, "run command requires an example");
            return 1;
        }

        char *input_path;
        if (strcmp(example, "app") == 0) {
            input_path = nob_temp_sprintf("./app.c");
        } else {
            input_path = nob_temp_sprintf("./examples/%s.c", example);

        }

        char *output_path = nob_temp_sprintf("./build/%s", example);

        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", output_path, input_path);
        if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;

        nob_cmd_append(&cmd, output_path);
        return nob_cmd_run_sync(cmd);
    }

    return 0;
}

int create_app_h_file(void)
{
    Nob_Fd output_fd = nob_fd_open_for_write("./app.h");
    if (output_fd == NOB_INVALID_FD) {
        return 1;
    }

    Nob_File_Paths files = {0};
    if (!nob_read_entire_dir("./source", &files)) {
        return 1;
    }

    qsort(files.items, files.count, sizeof(char*), alphabetical_cmp);

    Nob_String_Builder sb = {0};
    for (size_t i = 0; i < files.count; ++i) {
        if (nob_sv_end_with(nob_sv_from_cstr(files.items[i]), ".h")) {
            Nob_String_Builder file_content = {0};
            char *file_path = nob_temp_sprintf("./source/%s", files.items[i]);
            if (!nob_read_entire_file(file_path, &file_content)) return 1;
            nob_sb_append_buf(&sb, file_content.items, file_content.count);
            nob_sb_append_cstr(&sb, "\n");
            nob_sb_free(file_content);
        }
        if (i == 2) print_gen_info(&sb);
    }
    
    if (!nob_write_entire_file("./app.h", sb.items, sb.count)) return 1;

    nob_fd_close(output_fd);
    nob_da_free(sb);
    nob_da_free(files);
    return 0;
}

int alphabetical_cmp(const void *a, const void *b)
{
    return strcmp(*(char**)a, *(char**)b);
}

void print_gen_info(Nob_String_Builder *sb)
{
    nob_sb_append_cstr(sb, "\n////////////////////////////////////////////////////////\n");
    nob_sb_append_cstr(sb, "//                                                    //\n");
    nob_sb_append_cstr(sb, "//   ╔════════════════════════════════════════════╗   //\n");
    nob_sb_append_cstr(sb, "//   ║                                            ║   //\n");
    nob_sb_append_cstr(sb, "//   ║              >>> WARNING <<<               ║   //\n");
    nob_sb_append_cstr(sb, "//   ║                                            ║   //\n");
    nob_sb_append_cstr(sb, "//   ║   This file is automatically generated.    ║   //\n");
    nob_sb_append_cstr(sb, "//   ║                                            ║   //\n");
    nob_sb_append_cstr(sb, "//   ║   ═══════ Do not edit this file ════════   ║   //\n");
    nob_sb_append_cstr(sb, "//   ║                                            ║   //\n");
    nob_sb_append_cstr(sb, "//   ║   Instead edit files in source directory   ║   //\n");
    nob_sb_append_cstr(sb, "//   ║   and run `./nob` to rebuild this file.    ║   //\n");
    nob_sb_append_cstr(sb, "//   ║                                            ║   //\n");
    nob_sb_append_cstr(sb, "//   ║   The build system will concatinate all    ║   //\n");
    nob_sb_append_cstr(sb, "//   ║   header files into one `app.h` file.      ║   //\n");
    nob_sb_append_cstr(sb, "//   ║                                            ║   //\n");
    nob_sb_append_cstr(sb, "//   ╚════════════════════════════════════════════╝   //\n");
    nob_sb_append_cstr(sb, "//                                                    //\n");
    nob_sb_append_cstr(sb, "////////////////////////////////////////////////////////\n\n\n");
}