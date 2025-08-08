// 方案3：基于配置的复合检查器
#include "testexpect.h"
#include "dynamic_value.h"
#include <vector>
#include <memory>

// 检查器基类
class BaseChecker {
public:
    virtual ~BaseChecker() = default;
    virtual bool check(Testexpect* my_data, const std::string& attr_name, 
                      AttrData& attr_data, std::map<std::string, AttrData>& variables) = 0;
    virtual void configure(const DynamicValue& config) {}
    virtual std::string describe() const = 0;
};

// 复合检查器 - 支持AND/OR逻辑
class CompositeChecker : public BaseChecker {
public:
    enum LogicOperator { AND, OR };
    
    CompositeChecker(LogicOperator op) : logic_op_(op) {}
    
    void addChecker(std::unique_ptr<BaseChecker> checker) {
        checkers_.push_back(std::move(checker));
    }
    
    bool check(Testexpect* my_data, const std::string& attr_name, 
              AttrData& attr_data, std::map<std::string, AttrData>& variables) override {
        if (checkers_.empty()) return true;
        
        if (logic_op_ == AND) {
            for (const auto& checker : checkers_) {
                if (!checker->check(my_data, attr_name, attr_data, variables)) {
                    return false;
                }
            }
            return true;
        } else { // OR
            for (const auto& checker : checkers_) {
                if (checker->check(my_data, attr_name, attr_data, variables)) {
                    return true;
                }
            }
            return false;
        }
    }
    
    std::string describe() const override {
        std::string result = "(";
        std::string op_str = (logic_op_ == AND) ? " AND " : " OR ";
        
        for (size_t i = 0; i < checkers_.size(); ++i) {
            if (i > 0) result += op_str;
            result += checkers_[i]->describe();
        }
        result += ")";
        return result;
    }
    
private:
    LogicOperator logic_op_;
    std::vector<std::unique_ptr<BaseChecker>> checkers_;
};

// 具体检查器实现
class RangeChecker : public BaseChecker {
public:
    bool check(Testexpect* my_data, const std::string& attr_name, 
              AttrData& attr_data, std::map<std::string, AttrData>& variables) override {
        float* data = static_cast<float*>(attr_data.data);
        int length = attr_data.length * my_data->group_size;
        
        for(int i = 0; i < length; ++i) {
            if(data[i] < min_value_ || data[i] > max_value_) {
                return false;
            }
        }
        return true;
    }
    
    void configure(const DynamicValue& config) override {
        min_value_ = config.get("min", DynamicValue(-1e6)).as_double();
        max_value_ = config.get("max", DynamicValue(1e6)).as_double();
    }
    
    std::string describe() const override {
        return "RANGE[" + std::to_string(min_value_) + "," + std::to_string(max_value_) + "]";
    }
    
private:
    double min_value_ = -1e6;
    double max_value_ = 1e6;
};

class StatisticalChecker : public BaseChecker {
public:
    bool check(Testexpect* my_data, const std::string& attr_name, 
              AttrData& attr_data, std::map<std::string, AttrData>& variables) override {
        float* data = static_cast<float*>(attr_data.data);
        int length = attr_data.length * my_data->group_size;
        
        // 计算统计量
        double sum = 0, sum_sq = 0;
        for(int i = 0; i < length; ++i) {
            sum += data[i];
            sum_sq += data[i] * data[i];
        }
        
        double mean = sum / length;
        double variance = sum_sq / length - mean * mean;
        double std_dev = std::sqrt(variance);
        
        // 检查统计量是否符合要求
        return (mean >= min_mean_ && mean <= max_mean_ && 
                std_dev >= min_std_ && std_dev <= max_std_);
    }
    
    void configure(const DynamicValue& config) override {
        min_mean_ = config.get("min_mean", DynamicValue(-1e6)).as_double();
        max_mean_ = config.get("max_mean", DynamicValue(1e6)).as_double();
        min_std_ = config.get("min_std", DynamicValue(0.0)).as_double();
        max_std_ = config.get("max_std", DynamicValue(1e6)).as_double();
    }
    
    std::string describe() const override {
        return "STATS[mean:" + std::to_string(min_mean_) + "-" + std::to_string(max_mean_) + 
               ",std:" + std::to_string(min_std_) + "-" + std::to_string(max_std_) + "]";
    }
    
private:
    double min_mean_ = -1e6, max_mean_ = 1e6;
    double min_std_ = 0.0, max_std_ = 1e6;
};

// 检查器构建器
class CheckerBuilder {
public:
    static std::unique_ptr<BaseChecker> buildFromConfig(const DynamicValue& config) {
        std::string type = config.get("type", DynamicValue("")).as_string();
        
        if (type == "range") {
            auto checker = std::make_unique<RangeChecker>();
            checker->configure(config);
            return checker;
        } else if (type == "stats") {
            auto checker = std::make_unique<StatisticalChecker>();
            checker->configure(config);
            return checker;
        } else if (type == "composite") {
            std::string op = config.get("operator", DynamicValue("AND")).as_string();
            auto composite = std::make_unique<CompositeChecker>(
                op == "OR" ? CompositeChecker::OR : CompositeChecker::AND);
            
            const auto& checkers_config = config.get("checkers", DynamicValue::make_array()).as_array();
            for (const auto& checker_config : checkers_config) {
                auto sub_checker = buildFromConfig(checker_config);
                if (sub_checker) {
                    composite->addChecker(std::move(sub_checker));
                }
            }
            return composite;
        }
        
        return nullptr;
    }
};

// 使用示例的配置格式
/*
YAML配置示例:
check_patterns:
  - name: "complex_validation"
    type: "composite"
    operator: "AND"
    checkers:
      - type: "range"
        min: 0.0
        max: 100.0
      - type: "stats"
        min_mean: 10.0
        max_mean: 90.0
        min_std: 1.0
        max_std: 15.0
*/