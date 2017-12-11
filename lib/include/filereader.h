#ifndef LKCHECKER_FILEREADER
#define LKCHECKER_FILEREADER

#ifdef __cplusplus
extern "C" {
#endif

struct lk_file;

struct lk_file* lk_file_open(const char* path);
void lk_file_close(struct lk_file *file);
int lk_file_is_valid(const struct lk_file const *file);

lk_result lk_file_read(struct lk_file *file, char *buffer, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif
