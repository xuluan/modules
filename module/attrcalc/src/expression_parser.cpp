#include "expression_parser.h"
#include <iostream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <cmath>
#include <cstring>

ExpressionParser::ExpressionParser() : pos(0), token_pos(0) {}

void ExpressionParser::add_error(const std::string& message, size_t position) {
    ParseError error;
    error.position = (position == SIZE_MAX) ? pos : position;
    error.message = message;
    
    // Extract context around the error position
    size_t start = (error.position >= 10) ? error.position - 10 : 0;
    size_t end = std::min(error.position + 10, expr.length());
    error.context = expr.substr(start, end - start);
    
    errors.push_back(error);
}

bool ExpressionParser::is_valid_variable_char(char c, bool first_char) {
    if (first_char) {
        return std::isalpha(c) || c == '_';
    }
    return std::isalnum(c) || c == '_' || c == '-';
}

bool ExpressionParser::is_operator(const std::string& str, AttributeOp& op) {
    if (str == "+") { op = OP_ADD; return true; }
    if (str == "-") { op = OP_SUB; return true; }
    if (str == "*") { op = OP_MUL; return true; }
    if (str == "/") { op = OP_DIV; return true; }
    if (str == "SIN") { op = OP_SIN; return true; }
    if (str == "COS") { op = OP_COS; return true; }
    if (str == "TAN") { op = OP_TAN; return true; }
    if (str == "LOG") { op = OP_LOG; return true; }
    if (str == "SQRT") { op = OP_SQRT; return true; }
    if (str == "ABS") { op = OP_ABS; return true; }
    if (str == "POW") { op = OP_POW; return true; }
    if (str == "EXP") { op = OP_EXP; return true; }
    return false;
}

void ExpressionParser::tokenize() {
    tokens.clear();
    pos = 0;
    
    while (pos < expr.length()) {
        char c = expr[pos];
        
        // Skip whitespace
        if (std::isspace(c)) {
            pos++;
            continue;
        }
        
        Token token;
        token.position = pos;
        
        // Numbers (integer or floating point)
        if (std::isdigit(c)) {
            token.type = Token::NUMBER;
            size_t start = pos;
            while (pos < expr.length() && (std::isdigit(expr[pos]) || expr[pos] == '.')) {
                pos++;
            }
            token.value = expr.substr(start, pos - start);
            
            // Validate number format
            bool has_dot = false;
            for (char ch : token.value) {
                if (ch == '.') {
                    if (has_dot) {
                        add_error("Invalid number format: multiple decimal points", start);
                        return;
                    }
                    has_dot = true;
                }
            }
        }
        // Variables or function names
        else if (is_valid_variable_char(c, true)) {
            size_t start = pos;
            while (pos < expr.length() && is_valid_variable_char(expr[pos], false)) {
                pos++;
            }
            token.value = expr.substr(start, pos - start);
            
            AttributeOp op;
            if (is_operator(token.value, op)) {
                token.type = Token::OPERATOR;
            } else {
                token.type = Token::VARIABLE;
            }
        }
        // Single character operators and parentheses
        else if (c == '+' || c == '-' || c == '*' || c == '/') {
            token.type = Token::OPERATOR;
            token.value = c;
            pos++;
        }
        else if (c == '(') {
            token.type = Token::LEFT_PAREN;
            token.value = c;
            pos++;
        }
        else if (c == ')') {
            token.type = Token::RIGHT_PAREN;
            token.value = c;
            pos++;
        }
        else if (c == ',') {
            token.type = Token::OPERATOR;
            token.value = c;
            pos++;
        }
        else {
            add_error("Unexpected character: '" + std::string(1, c) + "'", pos);
            pos++;
            continue;
        }
        
        tokens.push_back(token);
    }
    
    // Add end token
    Token end_token;
    end_token.type = Token::END;
    end_token.position = expr.length();
    tokens.push_back(end_token);
}

ExpressionParser::Token ExpressionParser::current_token() {
    if (token_pos < tokens.size()) {
        return tokens[token_pos];
    }
    Token end_token;
    end_token.type = Token::END;
    end_token.position = expr.length();
    return end_token;
}

ExpressionParser::Token ExpressionParser::advance_token() {
    if (token_pos < tokens.size() - 1) {
        token_pos++;
    }
    return current_token();
}

bool ExpressionParser::expect_token(Token::Type type, const std::string& message) {
    Token token = current_token();
    if (token.type != type) {
        std::string error_msg = message.empty() ? 
            "Expected token type " + std::to_string(static_cast<int>(type)) :
            message;
        add_error(error_msg, token.position);
        return false;
    }
    return true;
}

bool ExpressionParser::validate_variable(const std::string& var_name) {
    return std::find(var_list.begin(), var_list.end(), var_name) != var_list.end();
}

// Expression := Term (('+' | '-') Term)*
ExpressionTree ExpressionParser::parse_expression() {
    ExpressionTree left = parse_term();
    if (!left) return nullptr;
    
    while (true) {
        Token token = current_token();
        if (token.type != Token::OPERATOR || 
            (token.value != "+" && token.value != "-")) {
            break;
        }
        
        advance_token();
        ExpressionTree right = parse_term();
        if (!right) return nullptr;
        
        auto node = std::make_shared<ExpressionNode>(NodeType::BINARY_OP);
        node->operation = (token.value == "+") ? OP_ADD : OP_SUB;
        node->left = left;
        node->right = right;
        left = node;
    }
    
    return left;
}

// Term := Factor (('*' | '/') Factor)*
ExpressionTree ExpressionParser::parse_term() {
    ExpressionTree left = parse_factor();
    if (!left) return nullptr;
    
    while (true) {
        Token token = current_token();
        if (token.type != Token::OPERATOR || 
            (token.value != "*" && token.value != "/" && token.value != "pow")) {
            break;
        }
        
        advance_token();
        ExpressionTree right = parse_factor();
        if (!right) return nullptr;
        
        auto node = std::make_shared<ExpressionNode>(NodeType::BINARY_OP);
        if (token.value == "*") node->operation = OP_MUL;
        else if (token.value == "/") node->operation = OP_DIV;
        else if (token.value == "pow") node->operation = OP_POW;
        
        node->left = left;
        node->right = right;
        left = node;
    }
    
    return left;
}

// Factor := Unary | Primary
ExpressionTree ExpressionParser::parse_factor() {
    return parse_unary();
}

// Unary := ('+' | '-' | UnaryFunction) Primary | Primary
ExpressionTree ExpressionParser::parse_unary() {
    Token token = current_token();
    
    // Handle unary + and -
    if (token.type == Token::OPERATOR && (token.value == "+" || token.value == "-")) {
        advance_token();
        ExpressionTree operand = parse_primary();
        if (!operand) return nullptr;
        
        if (token.value == "+") {
            return operand; // Unary + is no-op
        } else {
            // Create a unary minus as 0 - operand
            auto zero_node = std::make_shared<ExpressionNode>(NodeType::NUMBER);
            zero_node->value = 0.0;
            
            auto node = std::make_shared<ExpressionNode>(NodeType::BINARY_OP);
            node->operation = OP_SUB;
            node->left = zero_node;
            node->right = operand;
            return node;
        }
    }
    
    // Handle unary functions and pow
    if (token.type == Token::OPERATOR) {
        AttributeOp op;
        if (is_operator(token.value, op)) {
            const OperationInfo* info = get_operation_info(op);
            if (info && !info->is_binary) {
                // Handle unary functions
                advance_token();
                
                // Expect opening parenthesis for function calls
                if (!expect_token(Token::LEFT_PAREN, "Expected '(' after function name")) {
                    return nullptr;
                }
                advance_token();
                
                ExpressionTree operand = parse_expression();
                if (!operand) return nullptr;
                
                if (!expect_token(Token::RIGHT_PAREN, "Expected ')' after function argument")) {
                    return nullptr;
                }
                advance_token();
                
                auto node = std::make_shared<ExpressionNode>(NodeType::UNARY_OP);
                node->operation = op;
                node->left = operand;
                return node;
            } else if (op == OP_POW) {
                // Handle pow as a special binary function
                advance_token();
                
                // Expect opening parenthesis
                if (!expect_token(Token::LEFT_PAREN, "Expected '(' after 'pow'")) {
                    return nullptr;
                }
                advance_token();
                
                ExpressionTree left_operand = parse_expression();
                if (!left_operand) return nullptr;
                
                // Expect comma
                if (!expect_token(Token::OPERATOR, "Expected ',' in pow function") || 
                    current_token().value != ",") {
                    add_error("Expected ',' between pow arguments", current_token().position);
                    return nullptr;
                }
                advance_token();
                
                ExpressionTree right_operand = parse_expression();
                if (!right_operand) return nullptr;
                
                if (!expect_token(Token::RIGHT_PAREN, "Expected ')' after pow arguments")) {
                    return nullptr;
                }
                advance_token();
                
                auto node = std::make_shared<ExpressionNode>(NodeType::BINARY_OP);
                node->operation = OP_POW;
                node->left = left_operand;
                node->right = right_operand;
                return node;
            }
        }
    }
    
    return parse_primary();
}

// Primary := Number | Variable | '(' Expression ')'
ExpressionTree ExpressionParser::parse_primary() {
    Token token = current_token();
    
    if (token.type == Token::NUMBER) {
        advance_token();
        auto node = std::make_shared<ExpressionNode>(NodeType::NUMBER);
        node->value = std::stod(token.value);
        return node;
    }
    
    if (token.type == Token::VARIABLE) {
        advance_token();
        if (!validate_variable(token.value)) {
            add_error("Undefined variable: '" + token.value + "'", token.position);
            return nullptr;
        }
        
        // Add variable to used_variables if not already present
        if (std::find(used_variables.begin(), used_variables.end(), token.value) == used_variables.end()) {
            used_variables.push_back(token.value);
        }
        
        auto node = std::make_shared<ExpressionNode>(NodeType::VARIABLE);
        node->variable_name = token.value;
        return node;
    }
    
    if (token.type == Token::LEFT_PAREN) {
        advance_token();
        ExpressionTree expr = parse_expression();
        if (!expr) return nullptr;
        
        if (!expect_token(Token::RIGHT_PAREN, "Expected ')' to match '('")) {
            return nullptr;
        }
        advance_token();
        return expr;
    }
    
    add_error("Expected number, variable, or '('", token.position);
    return nullptr;
}

bool ExpressionParser::parse(const std::string& expression, 
                           const std::vector<std::string>& variables, 
                           ExpressionTree& result) {
    expr = expression;
    var_list = variables;
    used_variables.clear();
    errors.clear();
    token_pos = 0;
    
    tokenize();
    if (!errors.empty()) {
        return false;
    }
    
    result = parse_expression();
    if (!result || !errors.empty()) {
        return false;
    }
    
    // Check if we consumed all tokens
    Token token = current_token();
    if (token.type != Token::END) {
        add_error("Unexpected token after expression", token.position);
        return false;
    }
    
    return true;
}

std::string ExpressionParser::get_errors() const {

    std::string str;

    for (const auto& error : errors) {
        str += "Parse Error at position " + std::to_string(error.position) + ": " 
                  + error.message + "\n";
        str += "Context: \"" + error.context + "\"\n";
        
        // Show position indicator
        std::string indicator(error.context.length(), ' ');
        size_t relative_pos = (error.position >= 10) ? 10 : error.position;
        if (relative_pos < indicator.length()) {
            indicator[relative_pos] = '^';
        }
        str += "         \"" + indicator + "\"\n\n";
    }

    return str;
}

void ExpressionParser::print_errors() const {
    std::cout << get_errors();
}

const std::vector<std::string>& ExpressionParser::get_used_variables() const {
    return used_variables;
}

// ExpressionEvaluator implementation
ExpressionEvaluator::ExpressionEvaluator() {}

void ExpressionEvaluator::add_error(const std::string& message) {
    ParseError error;
    error.position = 0; // Position not available during evaluation
    error.message = message;
    error.context = "";
    errors.push_back(error);
}

bool ExpressionEvaluator::evaluate_node(const ExpressionNode* node,
                                       const std::map<std::string, AttrData>& variables,
                                       AttrData* temp_storage,
                                       AttrData*& result_ref) {
    if (!node) {
        add_error("Null expression node encountered");
        return false;
    }
    
    switch (node->type) {
        case NodeType::NUMBER: {
            // Create a constant vector filled with the number value
            double* data = static_cast<double*>(temp_storage->data);
            for (size_t i = 0; i < temp_storage->length; ++i) {
                data[i] = node->value;
            }
            temp_storage->type = as::DataFormat::FORMAT_R64;
            result_ref = temp_storage;
            return true;
        }
        
        case NodeType::VARIABLE: {
            auto it = variables.find(node->variable_name);
            if (it == variables.end()) {
                add_error("Variable '" + node->variable_name + "' not found in provided variables");
                return false;
            }
            result_ref = const_cast<AttrData*>(&it->second);
            return true;
        }
        
        case NodeType::BINARY_OP: {
            // We need additional temporary storage for binary operations
            std::vector<double> left_temp, right_temp;
            AttrData left_attr, right_attr;
            AttrData* left_result = nullptr;
            AttrData* right_result = nullptr;
            
            // Ensure temp storage is large enough
            if (left_temp.size() < temp_storage->length) {
                left_temp.resize(temp_storage->length);
                right_temp.resize(temp_storage->length);
            }
            
            left_attr.data = left_temp.data();
            left_attr.length = temp_storage->length;
            left_attr.type = as::DataFormat::FORMAT_R64;
            
            right_attr.data = right_temp.data();
            right_attr.length = temp_storage->length;
            right_attr.type = as::DataFormat::FORMAT_R64;
            
            if (!evaluate_node(node->left.get(), variables, &left_attr, left_result)) {
                return false;
            }
            
            if (!evaluate_node(node->right.get(), variables, &right_attr, right_result)) {
                return false;
            }
            
            temp_storage->type = as::DataFormat::FORMAT_R64;
            if (!vector_compute(node->operation, temp_storage, left_result, right_result)) {
                add_error("Failed to execute binary operation");
                return false;
            }
            
            result_ref = temp_storage;
            return true;
        }
        
        case NodeType::UNARY_OP: {
            std::vector<double> operand_temp;
            AttrData operand_attr;
            AttrData* operand_result = nullptr;
            
            if (operand_temp.size() < temp_storage->length) {
                operand_temp.resize(temp_storage->length);
            }
            
            operand_attr.data = operand_temp.data();
            operand_attr.length = temp_storage->length;
            operand_attr.type = as::DataFormat::FORMAT_R64;
            
            if (!evaluate_node(node->left.get(), variables, &operand_attr, operand_result)) {
                return false;
            }
            
            temp_storage->type = as::DataFormat::FORMAT_R64;
            if (!vector_compute(node->operation, temp_storage, operand_result, nullptr)) {
                add_error("Failed to execute unary operation");
                return false;
            }
            
            result_ref = temp_storage;
            return true;
        }
    }
    
    add_error("Unknown node type encountered");
    return false;
}

bool ExpressionEvaluator::evaluate(const ExpressionTree& expression,
                                  const std::map<std::string, AttrData>& variables,
                                  AttrData* result) {
    errors.clear();
    
    if (!expression) {
        add_error("Null expression tree provided");
        return false;
    }
    
    if (!result || !result->data) {
        add_error("Invalid result AttrData provided");
        return false;
    }
    
    // Create temporary storage for intermediate calculations
    static std::vector<double> temp_data;
    if (temp_data.size() < result->length) {
        temp_data.resize(result->length);
    }
    
    AttrData temp_storage;
    temp_storage.data = temp_data.data();
    temp_storage.length = result->length;
    temp_storage.type = as::DataFormat::FORMAT_R64;
    
    AttrData* final_result = nullptr;
    if (!evaluate_node(expression.get(), variables, &temp_storage, final_result)) {
        return false;
    }
    
    // Convert result to the desired format if necessary
    if (final_result->type != result->type) {
        // Create a temporary R64 version if needed
        AttrData r64_temp;
        r64_temp.data = final_result->data;
        r64_temp.length = final_result->length;
        r64_temp.type = as::DataFormat::FORMAT_R64;
        
        if (!convert_vector(result, &r64_temp)) {
            add_error("Failed to convert result to target format");
            return false;
        }
    } else {
        // Copy data directly
        std::memcpy(result->data, final_result->data, 
                   result->length * sizeof(double));
    }
    
    return true;
}

std::string ExpressionEvaluator::get_errors() const {
    std::string str;
    for (const auto& error : errors) {
        str += "Evaluation Error: " + error.message + "\n";
    }
    return str;
}

void ExpressionEvaluator::print_errors() const {
    std::cout << get_errors();
}

// Main API functions
bool parse_expression(const std::string& expr, 
                     const std::vector<std::string>& var_list,
                     ExpressionTree& result) {
    ExpressionParser parser;
    bool success = parser.parse(expr, var_list, result);
    if (!success) {
        parser.print_errors();
    }
    return success;
}

bool evaluate_expression(const ExpressionTree& expression,
                        const std::map<std::string, AttrData>& variables,
                        AttrData* result) {
    ExpressionEvaluator evaluator;
    bool success = evaluator.evaluate(expression, variables, result);
    if (!success) {
        evaluator.print_errors();
    }
    return success;
}