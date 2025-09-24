#ifndef SCALE_H
#define SCALE_H

#include <string>

enum ScaleMode {
  SCALE_MODE_FACTOR,
  SCALE_MODE_EXPR,
  SCALE_MODE_AGC,
  SCALE_MODE_DIVERGE
};


struct Scale {
  std::string attr_name;
  void* logger;
  ScaleMode mode;
  float factor;
  float window_size;
  float dvg_a;
  float dvg_v;

  size_t grp_size;
  void *trc_data;
  size_t trc_len;
  as::DataFormat trc_fmt;
  float sinterval;
  float trc_min;
};

#ifdef __cplusplus
extern "C" {
#endif

void scale_init(const char* myid, const char* mod_cfg);
void scale_process(const char* myid);

#ifdef __cplusplus
}
#endif

#endif /* ifndef SCALE_H */
