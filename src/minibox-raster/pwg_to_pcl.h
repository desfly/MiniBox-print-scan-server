#ifndef MINIBOX_PWG_TO_PCL_H
#define MINIBOX_PWG_TO_PCL_H
#include <stddef.h>
#include <stdint.h>

typedef int (*mb_pwg_write_fn)(void *ctx,const unsigned char *data,size_t len);

enum mb_pwg_phase {
    MB_PWG_MAGIC=0,
    MB_PWG_HEADER,
    MB_PWG_REPEAT,
    MB_PWG_CONTROL,
    MB_PWG_REPEAT_VALUE,
    MB_PWG_LITERAL
};

struct mb_pwg_pcl {
    enum mb_pwg_phase phase;
    unsigned char magic[4];
    size_t magic_used;
    unsigned char header[1796];
    size_t header_used;
    unsigned char *line;
    unsigned char *mono;
    size_t line_used;
    size_t line_cap;
    size_t mono_cap;
    unsigned width;
    unsigned height;
    unsigned xdpi;
    unsigned ydpi;
    unsigned bits_per_color;
    unsigned bits_per_pixel;
    unsigned bytes_per_line;
    unsigned color_order;
    unsigned color_space;
    unsigned num_colors;
    unsigned page_width_points;
    unsigned page_height_points;
    unsigned row;
    unsigned repeat_lines;
    unsigned color_value_bytes;
    unsigned token_units;
    unsigned token_bytes_left;
    unsigned char token_value[16];
    unsigned token_value_used;
    unsigned pages;
    int failed;
    int job_started;
};

void mb_pwg_pcl_init(struct mb_pwg_pcl *s);
void mb_pwg_pcl_reset(struct mb_pwg_pcl *s);
int mb_pwg_pcl_feed(struct mb_pwg_pcl *s,const unsigned char *data,size_t len,
                    mb_pwg_write_fn write_fn,void *write_ctx);
int mb_pwg_pcl_finish(struct mb_pwg_pcl *s,mb_pwg_write_fn write_fn,void *write_ctx);

#endif
