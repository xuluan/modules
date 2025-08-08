#pragma once

#include "vector_operations.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

struct ParseError {
    size_t position;
    std::string message;
    std::string context;
};

enum class NodeType {
    NUMBER,
    VARIABLE,
    BINARY_OP,
    UNARY_OP
};

struct ExpressionNode {
    NodeType type;
    
    // For NUMBER nodes
    double value;
    
    // For VARIABLE nodes
    std::string variable_name;
    
    // For operation nodes
    AttributeOp operation;
    std::shared_ptr<ExpressionNode> left;
    std::shared_ptr<ExpressionNode> right;
    
    ExpressionNode(NodeType t) : type(t), value(0.0) {}
};

using ExpressionTree = std::shared_ptr<ExpressionNode>;

class ExpressionParser {
private:
    std::string expr;
    size_t pos;
    std::vector<std::string> var_list;
    std::vector<std::string> used_variables;
    std::vector<ParseError> errors;
    
    // Tokenization
    struct Token {
        enum Type {
            NUMBER, VARIABLE, OPERATOR, LEFT_PAREN, RIGHT_PAREN, END
        };
        Type type;
        std::string value;
        size_t position;
    };
    
    std::vector<Token> tokens;
    size_t token_pos;
    
    void tokenize();
    bool is_valid_variable_char(char c, bool first_char = false);
    bool is_operator(const std::string& str, AttributeOp& op);
    void add_error(const std::string& message, size_t position = SIZE_MAX);
    
    // Parsing with operator precedence
    ExpressionTree parse_expression();
    ExpressionTree parse_term();
    ExpressionTree parse_factor();
    ExpressionTree parse_unary();
    ExpressionTree parse_primary();
    
    Token current_token();
    Token advance_token();
    bool expect_token(Token::Type type, const std::string& message = "");
    
    bool validate_variable(const std::string& var_name);
    
public:
    ExpressionParser();
    bool parse(const std::string& expression, const std::vector<std::string>& variables, ExpressionTree& result);
    std::string get_errors() const;
    void print_errors() const;
    const std::vector<std::string>& get_used_variables() const;
};

class ExpressionEvaluator {
private:
    std::vector<ParseError> errors;
    
    void add_error(const std::string& message);
    bool evaluate_node(const ExpressionNode* node, 
                      const std::map<std::string, AttrData>& variables,
                      AttrData* temp_storage,
                      AttrData*& result_ref);
    
public:
    ExpressionEvaluator();
    bool evaluate(const ExpressionTree& expression,
                 const std::map<std::string, AttrData>& variables,
                 AttrData* result);
    std::string get_errors() const;

    void print_errors() const;
};

// Main API functions
bool parse_expression(const std::string& expr, 
                     const std::vector<std::string>& var_list,
                     ExpressionTree& result);

bool evaluate_expression(const ExpressionTree& expression,
                        const std::map<std::string, AttrData>& variables,
                        AttrData* result);