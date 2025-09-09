#ifndef MUTE_H
#define MUTE_H

#include <string>
#include "vector_operations.h"
#include "expression_parser.h"

struct Mute {

    void* logger;
    std::string compare_direction;
    int threshold_value;
    bool expr_enable;
    std::string threshold_expr;
    int tapering_window_size;

    // std::vector<std::string> used_variables;

    gexpr::ExpressionTree expression;

};


#ifdef __cplusplus
    extern "C" {
#endif

void mute_init(const char* myid, const char* mod_cfg);
void mute_process(const char* myid);

#ifdef __cplusplus
    }
#endif

#endif /* ifndef MUTE_H */
