#pragma once
#include "stdafx.h"
#include "Lexer.h"

bool ParseTypename(Lexeme** str);

bool ParseNumber(Lexeme** str);
bool ParseArrayDefinition(Lexeme** str);
bool ParseSelectType(Lexeme** str);
bool ParseIsConst(Lexeme** str);

bool ParseClassDefinition(Lexeme** str);

bool ParseFunctionCall(Lexeme** str);

bool ParseFunctionVariables(Lexeme** str);
bool ParseFunctionDefinition(Lexeme** str);
bool ParseFunctionPrototype(Lexeme** str);

bool ParseAlignment(Lexeme** str);

bool ParseAddVariable(Lexeme** str);
bool ParseVariableDefineSub(Lexeme** str);
bool ParseVariableDefine(Lexeme** str);

bool ParseIfExpr(Lexeme** str);
bool ParseForExpr(Lexeme** str);
bool ParseWhileExpr(Lexeme** str);
bool ParseDoWhileExpr(Lexeme** str);
bool ParseSwitchExpr(Lexeme** str);

bool ParseReturnExpr(Lexeme** str);
bool ParseBreakExpr(Lexeme** str);
bool ParseContinueExpr(Lexeme** str);

bool ParseGroup(Lexeme** str);

bool ParseVariable(Lexeme** str);
bool ParsePostExpression(Lexeme** str);

bool ParseTerminal(Lexeme** str);
bool ParsePower(Lexeme** str);
bool ParseMultiplicative(Lexeme** str);
bool ParseAdditive(Lexeme** str);
bool ParseBinaryShift(Lexeme** str);
bool ParseComparision(Lexeme** str);
bool ParseStrongComparision(Lexeme** str);
bool ParseBinaryAnd(Lexeme** str);
bool ParseBinaryXor(Lexeme** str);
bool ParseBinaryOr(Lexeme** str);
bool ParseLogicalAnd(Lexeme** str);
bool ParseLogicalXor(Lexeme** str);
bool ParseLogicalOr(Lexeme** str);
bool ParseTernaryExpr(Lexeme** str);
bool ParseVaribleSet(Lexeme** str);

bool ParseBlock(Lexeme** str);
bool ParseExpression(Lexeme** str);
bool ParseCode(Lexeme** str);