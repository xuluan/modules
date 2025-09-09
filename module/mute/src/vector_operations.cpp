#include "vector_operations.h"
#include <functional>
#include <unordered_map>
#include <array>

namespace gexpr{


using BinaryOpFunc = std::function<double(double, double)>;
using UnaryOpFunc = std::function<double(double)>;

struct Operation {
    bool is_binary;
    const char* name;
    BinaryOpFunc binary_func;
    UnaryOpFunc unary_func;
    
    Operation(const char* n, BinaryOpFunc f) 
        : is_binary(true), name(n), binary_func(f) {}
    
    Operation(const char* n, UnaryOpFunc f) 
        : is_binary(false), name(n), unary_func(f) {}
};

static std::unordered_map<AttributeOp, Operation> operations = {
    {OP_ADD, Operation("+", [](double a, double b) { return a + b; })},
    {OP_SUB, Operation("-", [](double a, double b) { return a - b; })},
    {OP_MUL, Operation("*", [](double a, double b) { return a * b; })},
    {OP_DIV, Operation("/", [](double a, double b) { return b != 0.0 ? a / b : 0.0; })},
    {OP_POW, Operation("pow", [](double a, double b) { return std::pow(a, b); })},
    
    {OP_SIN, Operation("sin", [](double a) { return std::sin(a); })},
    {OP_COS, Operation("cos", [](double a) { return std::cos(a); })},
    {OP_TAN, Operation("tan", [](double a) { return std::tan(a); })},
    {OP_LOG, Operation("log", [](double a) { return a > 0.0 ? std::log(a) : 0.0; })},
    {OP_SQRT, Operation("sqrt", [](double a) { return a >= 0.0 ? std::sqrt(a) : 0.0; })},
    {OP_ABS, Operation("abs", [](double a) { return std::abs(a); })},
    {OP_EXP, Operation("exp", [](double a) { return std::exp(a); })}
};

const OperationInfo* get_operation_info(AttributeOp op) {
    auto it = operations.find(op);
    if (it != operations.end()) {
        static OperationInfo info;
        info.is_binary = it->second.is_binary;
        info.name = it->second.name;
        return &info;
    }
    return nullptr;
}

struct TypeKey {
    as::DataFormat result_type;
    as::DataFormat first_type;
    as::DataFormat second_type;
    
    bool operator==(const TypeKey& other) const {
        return result_type == other.result_type && 
               first_type == other.first_type && 
               second_type == other.second_type;
    }
};

struct TypeKeyHasher {
    size_t operator()(const TypeKey& key) const {
        return (static_cast<size_t>(key.result_type) << 16) |
               (static_cast<size_t>(key.first_type) << 8) |
               static_cast<size_t>(key.second_type);
    }
};

using BinaryComputeFunc = bool(*)(const Operation&, AttrData*, const AttrData*, const AttrData*);
using UnaryComputeFunc = bool(*)(const Operation&, AttrData*, const AttrData*);

template<typename RT, typename FT, typename ST>
bool compute_binary_typed(const Operation& op, AttrData* result, const AttrData* first, const AttrData* second) {
    const FT* first_data = static_cast<const FT*>(first->data);
    const ST* second_data = static_cast<const ST*>(second->data);
    RT* result_data = static_cast<RT*>(result->data);
    
    //todo  assert length
    for (size_t i = 0; i < result->length; ++i) {
        double a = static_cast<double>(first_data[i]);
        double b = static_cast<double>(second_data[i]);
        double res = op.binary_func(a, b);
        result_data[i] = safe_cast<RT>(res);
    }
    return true;
}

template<typename RT, typename FT>
bool compute_unary_typed(const Operation& op, AttrData* result, const AttrData* first) {
    const FT* first_data = static_cast<const FT*>(first->data);
    RT* result_data = static_cast<RT*>(result->data);
    
    //todo  assert length    
    for (size_t i = 0; i < result->length; ++i) {
        double a = static_cast<double>(first_data[i]);
        double res = op.unary_func(a);
        result_data[i] = safe_cast<RT>(res);
    }
    return true;
}

static std::unordered_map<TypeKey, BinaryComputeFunc, TypeKeyHasher> binary_dispatch_table;
static std::unordered_map<TypeKey, UnaryComputeFunc, TypeKeyHasher> unary_dispatch_table;

template<typename RT, typename FT, typename ST>
void register_binary_type() {
    TypeKey key = {TypeToFormat<RT>(), TypeToFormat<FT>(), TypeToFormat<ST>()};
    binary_dispatch_table[key] = compute_binary_typed<RT, FT, ST>;
}

template<typename RT, typename FT>
void register_unary_type() {
    TypeKey key = {TypeToFormat<RT>(), TypeToFormat<FT>(), as::DataFormat::FORMAT_U8}; // dummy for second_type
    unary_dispatch_table[key] = compute_unary_typed<RT, FT>;
}

class TypeDispatchInitializer {
public:
    TypeDispatchInitializer() {
        register_binary_type<double, int8_t, int8_t>();
        register_binary_type<double, int8_t, int16_t>();
        register_binary_type<double, int8_t, int32_t>();
        register_binary_type<double, int8_t, int64_t>();
        register_binary_type<double, int8_t, float>();
        register_binary_type<double, int8_t, double>();

        register_binary_type<double, int16_t, int8_t>();
        register_binary_type<double, int16_t, int16_t>();
        register_binary_type<double, int16_t, int32_t>();
        register_binary_type<double, int16_t, int64_t>();
        register_binary_type<double, int16_t, float>();
        register_binary_type<double, int16_t, double>();

        register_binary_type<double, int32_t, int8_t>();
        register_binary_type<double, int32_t, int16_t>();
        register_binary_type<double, int32_t, int32_t>();
        register_binary_type<double, int32_t, int64_t>();
        register_binary_type<double, int32_t, float>();
        register_binary_type<double, int32_t, double>();

        register_binary_type<double, int64_t, int8_t>();
        register_binary_type<double, int64_t, int16_t>();
        register_binary_type<double, int64_t, int32_t>();
        register_binary_type<double, int64_t, int64_t>();
        register_binary_type<double, int64_t, float>();
        register_binary_type<double, int64_t, double>();

        register_binary_type<double, float, int8_t>();
        register_binary_type<double, float, int16_t>();
        register_binary_type<double, float, int32_t>();
        register_binary_type<double, float, int64_t>();
        register_binary_type<double, float, float>();
        register_binary_type<double, float, double>();

        register_binary_type<double, double, int8_t>();
        register_binary_type<double, double, int16_t>();
        register_binary_type<double, double, int32_t>();
        register_binary_type<double, double, int64_t>();
        register_binary_type<double, double, float>();
        register_binary_type<double, double, double>();

        register_unary_type<double, float>();
        register_unary_type<double, double>();
        register_unary_type<double, int32_t>();
        register_unary_type<double, int64_t>();
        register_unary_type<double, int16_t>();
        register_unary_type<double, int8_t>();
    }
};

static TypeDispatchInitializer initializer;

bool dispatch_operation(const Operation& op, AttrData* result, const AttrData* first, const AttrData* second) {
    TypeKey key = {result->type, first->type, second ? second->type : as::DataFormat::FORMAT_U8};
    
    if (op.is_binary) {
        auto it = binary_dispatch_table.find(key);
        if (it != binary_dispatch_table.end()) {
            return it->second(op, result, first, second);
        }
    } else {
        auto it = unary_dispatch_table.find(key);
        if (it != unary_dispatch_table.end()) {
            return it->second(op, result, first);
        }
    }
    return false;
}

bool vector_compute(AttributeOp operation, AttrData* result, const AttrData* first, const AttrData* second) {
    if (!result || !first || !result->data || !first->data) {
        return false;
    }
    
    auto it = operations.find(operation);
    if (it == operations.end()) {
        return false;
    }
    
    const Operation& op = it->second;
    
    if (op.is_binary && (!second || !second->data)) {
        return false;
    }
    
    return dispatch_operation(op, result, first, second);
}

template<typename T>
bool convert_from_r64_typed(AttrData* dst, const AttrData* src) {
    const double* src_data = static_cast<const double*>(src->data);
    T* dst_data = static_cast<T*>(dst->data);
    
    //todo  assert length 
    dst->length = src->length;
    
    for (size_t i = 0; i < src->length; ++i) {
        dst_data[i] = safe_cast<T>(src_data[i]);
    }
    
    return true;
}

bool convert_vector(AttrData* dst, const AttrData* src) {
    // Validate input parameters
    if (!dst || !src || !dst->data || !src->data) {
        return false;
    }
    
    // Source must be as::DataFormat::FORMAT_R64
    if (src->type != as::DataFormat::FORMAT_R64) {
        return false;
    }
    
    // Dispatch based on destination type
    switch (dst->type) {
        case as::DataFormat::FORMAT_U8:
            return convert_from_r64_typed<int8_t>(dst, src);
        case as::DataFormat::FORMAT_U16:
            return convert_from_r64_typed<int16_t>(dst, src);
        case as::DataFormat::FORMAT_U32:
            return convert_from_r64_typed<int32_t>(dst, src);
        case as::DataFormat::FORMAT_U64:
            return convert_from_r64_typed<int64_t>(dst, src);
        case as::DataFormat::FORMAT_R32:
            return convert_from_r64_typed<float>(dst, src);
        case as::DataFormat::FORMAT_R64:
            return convert_from_r64_typed<double>(dst, src);
        default:
            return false;
    }
}

}
