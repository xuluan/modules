#ifndef ATTRCALC_H
#define ATTRCALC_H

#include <string>
#include "vector_operations.h"
#include "expression_parser.h"

struct Attrcalc {
    float fval;
    std::string name;
    std::string type;
    std::string action;
    std::string expr;
    ExpressionTree expression;
    AttrData attr_data;
    void* logger;
};

#ifdef __cplusplus
extern "C" {
#endif

void attrcalc_init(const char* myid, const char* mod_cfg);
void attrcalc_process(const char* myid);

#ifdef __cplusplus
}
#endif

#endif /* ifndef ATTRCALC_H */
