# Attrcalc

## Objective

To create or update an attribute by using a mathematical expression. 

User-defined Parameters:

- attrname: The name of the attribute to be created or updated
- type: The data type of the current attribute: int8, int16, int32, int64, float and double. It is not required, with float as default. For action: update, if it is not set, the previous data type will be kept
- action: There are three options: create, update, and remove. create is the default option. create will create a new attribute, update will modify an existing attribute, and remove will delete an existing attribute.
- expr: The expression calculates the attribute value, which can include the following operators: +, -, *, /, sin, cos, tan, (, ), as well as integers, floating-point numbers and attribute names.

## Key Features

- Support basic mathematical operations
