#ifndef ATTRLIST_H
#define ATTRLIST_H



struct Attrlist {

    void* logger;
};

#ifdef __cplusplus
    extern "C" {
#endif

void attrlist_init(const char* myid, const char* mod_cfg);
void attrlist_process(const char* myid);

#ifdef __cplusplus
    }
#endif

#endif /* ifndef ATTRLIST_H */
