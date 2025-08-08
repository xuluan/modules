#ifndef DEMOATTR_H
#define DEMOATTR_H

#include <string>
struct Demoattr {
  float fval;
  std::string attr_name;
  void* logger;
};

#ifdef __cplusplus
extern "C" {
#endif

void demoattr_init(const char* myid, const char* mod_cfg);
void demoattr_process(const char* myid);

#ifdef __cplusplus
}
#endif

#endif /* ifndef DEMOATTR_H */
