#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include "helper.h"
#include "lexer.h"
#include "parser.h"

/**
 * Number of decimals used for the numbers.
 */
int precision = -1;

/**
 * Number of decimals used for the result.
 */
int result_precision = -1;

/**
 * Manage angles in degree or not.
 */
bool degree = false;

/**
 * Warning about division by zero.
 */
bool division_warning = false;

/**
 * Warning about an invalid value for
 * a trigonometric function.
 */
bool trigonometric_warning = false;

/**
 * The current operation is defining a variable or not.
 */
bool variable_defined = false;


static void push(struct list **list, const struct token *node)
{
    size_t value_len = strlen(node->value) + 1;
    struct token *new = calloc(3, sizeof(struct token));

    if (isEmpty(node->value)) {
        return;
    }

    new->next = NULL;
    new->prev = NULL;
    new->value = malloc_(value_len);
    strncpy_(new->value, node->value, value_len);

    if ((*list)->last == NULL) {
        (*list)->last = new;
        (*list)->first = new;
    } else {
        (*list)->last->next = new;
        new->prev = (*list)->last;
        (*list)->last = new;
    }
}


static void moveToken(struct list **dest, struct list **src)
{
    size_t value_len = strlen((*src)->last->value) + 1;
    struct token *cpy, *tmp;

    if (!strcmp((*src)->last->value, "(")) {
        tmp = (*src)->last;
        (*src)->last = (*src)->last->prev;
        free(tmp->value);
        free(tmp);
    }

    cpy = calloc(3, sizeof(struct token));
    cpy->prev = (*dest)->last;
    cpy->next = NULL;
    cpy->value = malloc_(value_len);
    strncpy_(cpy->value, (*src)->last->value, value_len);

    if ((*dest)->last != NULL) {
        (*dest)->last->next = cpy;
    } else {
        (*dest)->first = cpy;
    }

    (*dest)->last = cpy;
    tmp = (*src)->last;
    (*src)->last = (*src)->last->prev;
    free(tmp->value);
    free(tmp);

    if ((*src)->last != NULL) {
        (*src)->last->next = NULL;
    } else {
        (*src)->first = NULL;
    }
}


static void migrateUntilParenthesis(struct list *output,
                                    struct list *operators)
{
    struct token *tmp;

    while (operators->last != NULL && strcmp(operators->last->value, "(")) {
        moveToken(&output, &operators);
    }

    if (operators->last != NULL && !strcmp(operators->last->value, "(")) {
        tmp = operators->last;
        operators->last = operators->last->prev;
        free(tmp->value);
        free(tmp);
    }
}


static int hasHigherEqualPrec(struct token *first, struct token *second)
{
    return first != NULL && second != NULL &&
        getPrec(first->value) >= getPrec(second->value);
}


static void infixToPostfix(struct list *tokens,
                           struct list *output,
                           struct list *operators)
{
    struct token *node = tokens->first;

    division_warning = false;
    trigonometric_warning = false;

    while (node != NULL) {
        if (!strcmp(node->value, "(")) {
            push(&operators, node);
        } else if (!strcmp(node->value, ")")) {
            migrateUntilParenthesis(output, operators);
        } else if (isFunction(node->value)) {
            push(&operators, node);
        } else if (isOperator(node->value)) {
            while (hasHigherEqualPrec(node, operators->last)) {
                moveToken(&output, &operators);
            }

            push(&operators, node);
        } else {
            push(&output, node);
        }

        node = node->next;
    }

    while (operators->last != NULL) {
        moveToken(&output, &operators);
    }
}


static double getVariableValue(const char *key)
{
    struct variable *var = variables_first;

    while (var != NULL) {
        if (!strcmp(key, var->key)) {
            return var->value;
        }

        var = var->next;
    }

    return 0;
}


static double getValue(const char *str)
{
    return isVariable(str) ?
        getVariableValue(str) :
        strToDouble(str);
}


static double getOpResult(const char *operator,
                          const char *a,
                          const char *b)
{
    double x, y;

    variable_defined = !strcmp(operator, "=");
    if (variable_defined) {
        addVariable(a, getValue(b));
        return 0;
    }

    x = getValue(a);
    y = getValue(b);

    if (precision >= 0) {
        x = round_(x, precision);
        y = round_(y, precision);
    }

    if (!strcmp(operator, "+")) {
        return x + y;
    }

    if (!strcmp(operator, "-")) {
        return x - y;
    }

    if (!strcmp(operator, "*")) {
        return x * y;
    }

    if (!strcmp(operator, "/")) {
        if (y == 0) {
            division_warning = true;
            return 0;
        }

        return x / y;
    }

    if (!strcmp(operator, "^")) {
        return pow(x, y);
    }

    if (!strcmp(operator, "!")) {
        return fact((int)y);
    }

    if (!strcmp(operator, "sqrt")) {
        return sqrt(y);
    }

    if (!strcmp(operator, "abs")) {
        return fabs(y);
    }

    if (!strcmp(operator, "log")) {
        return log(y);
    }

    if (!strcmp(operator, "floor")) {
        return floor(y);
    }

    if (!strcmp(operator, "ceil")) {
        return ceil(y);
    }

    if (!strcmp(operator, "round")) {
        return round(y);
    }

    if (!strcmp(operator, "rand")) {
        srand(time(NULL));
        return (double) abs(rand() * 100) / RAND_MAX;
    }

    if (!strcmp(operator, "mb")) {
        return y * ONE_MB;
    }

    if (!strcmp(operator, "gb")) {
        return y * ONE_GB;
    }

    if (!strcmp(operator, "tb")) {
        return y * ONE_TB;
    }

    if (!strcmp(operator, "pb")) {
        return y * ONE_PT;
    }

    if (degree && isTrigonometric(operator)) {
        y = y / 180 * M_PI;
    }

    if (!strcmp(operator, "sin")) {
        return sin(y);
    }

    if (!strcmp(operator, "cos")) {
        return cos(y);
    }

    if (!strcmp(operator, "tan")) {
        return tan(y);
    }

    if ((!strcmp(operator, "asin") ||
        !strcmp(operator, "acos") ||
        !strcmp(operator, "atan")) &&
        (y < -1 || y > 1)) {
        trigonometric_warning = true;
        return 0;
    }

    if (!strcmp(operator, "asin")) {
        return asin(y);
    }

    if (!strcmp(operator, "acos")) {
        return acos(y);
    }

    if (!strcmp(operator, "atan")) {
        return atan(y);
    }

    return 0;
}


static char *pop(struct list *list)
{
    struct token *tmp;
    size_t len;
    char *str;

    if (list->last == NULL) {
        return 0;
    }

    len = strlen(list->last->value) + 1;

    str = malloc_(len);
    strncpy_(str, list->last->value, len);
    tmp = list->last;
    list->last = list->last->prev;
    free(tmp->value);
    free(tmp);

    return str;
}


static void pushResult(struct list *list, const struct token *node)
{
    struct token *new;
    char *x = NULL, *y = NULL;
    double result;

    if (list == NULL || list->last == NULL) {
        return;
    }

    y = pop(list);

    if (!isFunction(node->value) &&
        !isDataUnit(node->value)) {
        x = pop(list);
    }

    result = getOpResult(node->value, x, y);
    if (precision >= 0) {
        result = round_(result, precision);
    }

    new = malloc_(sizeof(struct token));
    new->value = malloc_(BUFFER);
    snprintf(new->value, BUFFER, NUMBER_FORMAT, result);

    push(&list, new);
    free(new->value);
    free(new);
    free(y);

    if (x != NULL) {
        free(x);
    }
}


static double getPostfixResult(const struct list *postfix)
{
    struct list *result_list;
    struct token *node = postfix->first;
    double result = 0.0;

    initList(&result_list);

    while (node != NULL) {
        if (isOperator(node->value) ||
            isFunction(node->value) ||
            isDataUnit(node->value)) {
            pushResult(result_list, node);
        } else {
            push(&result_list, node);
        }

        node = node->next;
    }

    if (result_list != NULL && result_list->last != NULL) {
        result = getValue(result_list->last->value);
    }

    if (result_precision >= 0) {
        result = round_(result, result_precision);
    }

    freeList(result_list);

    return result;
}


static void replaceVariable(const char *key, double val)
{
    struct variable *var = variables_first;

    while (var != NULL) {
        if (!strcmp(key, var->key)) {
            var->value = val;
            break;
        }

        var = var->next;
    }
}


void addVariable(const char *key, double val)
{
    struct variable *var;

    if (isVariable(key)) {
        replaceVariable(key, val);
        return;
    }

    var = malloc_(sizeof(struct variable));
    var->key = malloc_(BUFFER);
    strncpy_(var->key, key, strlen(key) + 1);
    var->value = val;

    var->next = variables_first;
    variables_first = var;
}


char *getResult(const char *func,
                struct list *tokens,
                struct list *output)
{
    struct list *operators;
    char *result = malloc_(BUFFER);
    result[0] = '\0';
    variable_defined = false;
    initList(&operators);

    tokenize(tokens, func);
    infixToPostfix(tokens, output, operators);
    snprintf(result, BUFFER, NUMBER_FORMAT, getPostfixResult(output));
    freeList(operators);

    if (variable_defined) {
        free(result);
        return NULL;
    }

    return result;
}


void printWarnings(const struct list *list)
{
    struct token *node = list->first;
    size_t warnings_quantity = 0;

    while (node != NULL) {
        if (!isValid(node->value)) {
            printf(TOKEN_WARNING_MSG, node->value);
            warnings_quantity++;
        }

        node = node->next;
    }

    if (division_warning) {
        printf(ZERO_DIVISION_WARNING_MSG);
        warnings_quantity++;
    }

    if (trigonometric_warning) {
        printf(TRIGONOMETRIC_WARNING_MSG);
        warnings_quantity++;
    }

    if (warnings_quantity > 0) {
        printf(INACCURATE_RESULT_MSG);
    }
}


void freeVariables(void)
{
    struct variable *var;

    while ((var = variables_first) != NULL) {
        variables_first = variables_first->next;
        free(var->key);
        free(var);
    }

    free(variables_first);
}
