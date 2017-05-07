#include "InstructionTreeVm.h"

#include "ExpressionTree.h"

// TODO: VM code generation should use a special pointer type to generate special pointer instructions
#ifdef _M_X64
	#define CreateConstantOffset CreateConstantLong
	#define VM_INST_LOAD_POINTER VM_INST_LOAD_LONG
#else
	#define CreateConstantOffset CreateConstantInt
	#define VM_INST_LOAD_POINTER VM_INST_LOAD_INT
#endif

namespace
{
	bool IsMemberScope(ScopeData *scope)
	{
		return scope->ownerType != NULL;
	}

	bool IsGlobalScope(ScopeData *scope)
	{
		// Not a global scope if there is an enclosing function or a type
		while(scope)
		{
			if(scope->ownerFunction || scope->ownerType)
				return false;

			scope = scope->scope;
		}

		return true;
	}

	bool DoesConstantIntegerMatch(VmValue* value, long long number)
	{
		if(VmConstant *constant = getType<VmConstant>(value))
		{
			if(constant->type == VmType::Int)
				return constant->iValue == number;

			if(constant->type == VmType::Long)
				return constant->lValue == number;
		}

		return false;
	}

	bool DoesConstantMatchEither(VmValue* value, int iValue, double dValue, long long lValue)
	{
		if(VmConstant *constant = getType<VmConstant>(value))
		{
			if(constant->type == VmType::Int)
				return constant->iValue == iValue;

			if(constant->type == VmType::Double)
				return constant->dValue == dValue;

			if(constant->type == VmType::Long)
				return constant->lValue == lValue;
		}

		return false;
	}

	bool IsConstantZero(VmValue* value)
	{
		return DoesConstantMatchEither(value, 0, 0.0, 0ll);
	}

	bool IsConstantOne(VmValue* value)
	{
		return DoesConstantMatchEither(value, 1, 1.0, 1ll);
	}

	VmValue* CheckType(ExpressionContext &ctx, ExprBase* expr, VmValue *value)
	{
		VmType exprType = GetVmType(ctx, expr->type);

		assert(exprType == value->type);

		return value;
	}

	VmValue* CreateConstantInt(int value)
	{
		VmConstant *result = new VmConstant(VmType::Int);

		result->iValue = value;

		return result;
	}

	VmValue* CreateConstantDouble(double value)
	{
		VmConstant *result = new VmConstant(VmType::Double);

		result->dValue = value;

		return result;
	}

	VmValue* CreateConstantLong(long long value)
	{
		VmConstant *result = new VmConstant(VmType::Long);

		result->lValue = value;

		return result;
	}

	VmValue* CreateConstantPointer(int value)
	{
		VmConstant *result = new VmConstant(VmType::Pointer);

		result->iValue = value;

		return result;
	}

	VmValue* CreateConstantStruct(char *value, int size)
	{
		assert(size % 4 == 0);

		VmConstant *result = new VmConstant(VmType::Struct(size));

		result->sValue = value;

		return result;
	}

	bool HasSideEffects(VmInstructionType cmd)
	{
		switch(cmd)
		{
		case VM_INST_STORE_BYTE:
		case VM_INST_STORE_SHORT:
		case VM_INST_STORE_INT:
		case VM_INST_STORE_FLOAT:
		case VM_INST_STORE_DOUBLE:
		case VM_INST_STORE_LONG:
		case VM_INST_STORE_STRUCT:
		case VM_INST_SET_RANGE:
		case VM_INST_JUMP:
		case VM_INST_JUMP_Z:
		case VM_INST_JUMP_NZ:
		case VM_INST_CALL:
		case VM_INST_RETURN:
		case VM_INST_YIELD:
		case VM_INST_CREATE_CLOSURE:
		case VM_INST_CLOSE_UPVALUES:
		case VM_INST_CONVERT_POINTER:
		case VM_INST_CHECKED_RETURN:
			return true;
		}

		return false;
	}

	VmInstruction* CreateInstruction(VmModule *module, VmType type, VmInstructionType cmd, VmValue *first, VmValue *second, VmValue *third)
	{
		assert(module->currentBlock);

		VmInstruction *inst = new VmInstruction(type, cmd, module->nextInstructionId++);

		if(first)
			inst->AddArgument(first);

		if(second)
			inst->AddArgument(second);

		if(third)
			inst->AddArgument(third);

		inst->hasSideEffects = HasSideEffects(cmd);

		module->currentBlock->AddInstruction(inst);

		return inst;
	}

	VmInstruction* CreateInstruction(VmModule *module, VmType type, VmInstructionType cmd)
	{
		return CreateInstruction(module, type, cmd, NULL, NULL, NULL);
	}

	VmInstruction* CreateInstruction(VmModule *module, VmType type, VmInstructionType cmd, VmValue *first)
	{
		return CreateInstruction(module, type, cmd, first, NULL, NULL);
	}

	VmInstruction* CreateInstruction(VmModule *module, VmType type, VmInstructionType cmd, VmValue *first, VmValue *second)
	{
		return CreateInstruction(module, type, cmd, first, second, NULL);
	}

	VmValue* CreateLoad(ExpressionContext &ctx, VmModule *module, TypeBase *type, VmValue *address)
	{
		if(type == ctx.typeBool || type == ctx.typeChar)
			return CreateInstruction(module, VmType::Int, VM_INST_LOAD_BYTE, address);

		if(type == ctx.typeShort)
			return CreateInstruction(module, VmType::Int, VM_INST_LOAD_SHORT, address);

		if(type == ctx.typeInt)
			return CreateInstruction(module, VmType::Int, VM_INST_LOAD_INT, address);

		if(type == ctx.typeFloat)
			return CreateInstruction(module, VmType::Double, VM_INST_LOAD_FLOAT, address);

		if(type == ctx.typeDouble)
			return CreateInstruction(module, VmType::Double, VM_INST_LOAD_DOUBLE, address);

		if(type == ctx.typeLong)
			return CreateInstruction(module, VmType::Long, VM_INST_LOAD_LONG, address);

		if(isType<TypeRef>(type))
			return CreateInstruction(module, VmType::Pointer, VM_INST_LOAD_POINTER, address);

		if(isType<TypeFunction>(type))
			return CreateInstruction(module, VmType::FunctionRef, VM_INST_LOAD_STRUCT, address);

		if(isType<TypeUnsizedArray>(type))
			return CreateInstruction(module, VmType::ArrayRef, VM_INST_LOAD_STRUCT, address);

		if(type == ctx.typeAutoRef)
			return CreateInstruction(module, VmType::AutoRef, VM_INST_LOAD_STRUCT, address);

		if(type == ctx.typeAutoArray)
			return CreateInstruction(module, VmType::AutoArray, VM_INST_LOAD_STRUCT, address);

		if(isType<TypeTypeID>(type))
			return CreateInstruction(module, VmType::Int, VM_INST_LOAD_INT, address);

		if(type->size == 0)
			return CreateConstantInt(0);

		assert(type->size % 4 == 0);
		assert(type->size != 0);
		assert(type->size < NULLC_MAX_TYPE_SIZE);

		return CreateInstruction(module, VmType::Struct(type->size), VM_INST_LOAD_STRUCT, address);
	}

	VmValue* CreateStore(ExpressionContext &ctx, VmModule *module, TypeBase *type, VmValue *address, VmValue *value)
	{
		if(type == ctx.typeBool || type == ctx.typeChar)
		{
			assert(value->type == VmType::Int);

			return CreateInstruction(module, VmType::Void, VM_INST_STORE_BYTE, address, value);
		}

		if(type == ctx.typeShort)
		{
			assert(value->type == VmType::Int);

			return CreateInstruction(module, VmType::Void, VM_INST_STORE_SHORT, address, value);
		}

		if(type == ctx.typeInt)
		{
			assert(value->type == VmType::Int);

			return CreateInstruction(module, VmType::Void, VM_INST_STORE_INT, address, value);
		}

		if(type == ctx.typeFloat)
		{
			assert(value->type == VmType::Double);

			return CreateInstruction(module, VmType::Void, VM_INST_STORE_FLOAT, address, value);
		}

		if(type == ctx.typeDouble)
		{
			assert(value->type == VmType::Double);

			return CreateInstruction(module, VmType::Void, VM_INST_STORE_DOUBLE, address, value);
		}

		if(type == ctx.typeLong)
		{
			assert(value->type == VmType::Long);

			return CreateInstruction(module, VmType::Void, VM_INST_STORE_LONG, address, value);
		}

		if(type->size == 0)
			return new VmVoid();

		assert(type->size % 4 == 0);
		assert(type->size != 0);
		assert(type->size < NULLC_MAX_TYPE_SIZE);
		assert(value->type == GetVmType(ctx, type));

		return CreateInstruction(module, VmType::Void, VM_INST_STORE_STRUCT, address, value);
	}

	VmValue* CreateCast(VmModule *module, VmValue *value, VmType target)
	{
		if(target == value->type)
			return value;

		if(target == VmType::Int)
		{
			if(value->type == VmType::Double)
				return CreateInstruction(module, target, VM_INST_DOUBLE_TO_INT, value);

			if(value->type == VmType::Long)
				return CreateInstruction(module, target, VM_INST_LONG_TO_INT, value);
		}
		else if(target == VmType::Double)
		{
			if(value->type == VmType::Int)
				return CreateInstruction(module, target, VM_INST_INT_TO_DOUBLE, value);

			if(value->type == VmType::Long)
				return CreateInstruction(module, target, VM_INST_LONG_TO_DOUBLE, value);
		}
		else if(target == VmType::Long)
		{
			if(value->type == VmType::Int)
				return CreateInstruction(module, target, VM_INST_INT_TO_LONG, value);

			if(value->type == VmType::Double)
				return CreateInstruction(module, target, VM_INST_DOUBLE_TO_LONG, value);
		}

		assert(!"unknown cast");

		return new VmVoid();
	}

	VmValue* CreateIndex(VmModule *module, VmValue *value, VmValue *size, VmValue *index)
	{
		assert(value->type == VmType::Pointer);
		assert(size->type == VmType::Int);
		assert(index->type == VmType::Int);

		return CreateInstruction(module, VmType::Pointer, VM_INST_INDEX, value, size, index);
	}

	VmValue* CreateIndexUnsized(VmModule *module, VmValue *value, VmValue *index)
	{
		assert(value->type == VmType::ArrayRef);
		assert(index->type == VmType::Int);

		return CreateInstruction(module, VmType::Pointer, VM_INST_INDEX_UNSIZED, value, index);
	}

	VmValue* CreateAdd(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		if(lhs->type == VmType::Pointer)
		{
			assert(rhs->type == VmType::Int);

			return CreateInstruction(module, VmType::Pointer, VM_INST_ADD, lhs, rhs);
		}

		assert(lhs->type == VmType::Int || lhs->type == VmType::Double || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, lhs->type, VM_INST_ADD, lhs, rhs);
	}

	VmValue* CreateSub(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		assert(lhs->type == VmType::Int || lhs->type == VmType::Double || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, lhs->type, VM_INST_SUB, lhs, rhs);
	}

	VmValue* CreateMul(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		assert(lhs->type == VmType::Int || lhs->type == VmType::Double || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, lhs->type, VM_INST_MUL, lhs, rhs);
	}

	VmValue* CreateDiv(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		assert(lhs->type == VmType::Int || lhs->type == VmType::Double || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, lhs->type, VM_INST_DIV, lhs, rhs);
	}

	VmValue* CreatePow(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		assert(lhs->type == VmType::Int || lhs->type == VmType::Double || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, lhs->type, VM_INST_POW, lhs, rhs);
	}

	VmValue* CreateMod(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		assert(lhs->type == VmType::Int || lhs->type == VmType::Double || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, lhs->type, VM_INST_MOD, lhs, rhs);
	}

	VmValue* CreateCompareLess(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		assert(lhs->type == VmType::Int || lhs->type == VmType::Double || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, VmType::Int, VM_INST_LESS, lhs, rhs);
	}

	VmValue* CreateCompareGreater(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		assert(lhs->type == VmType::Int || lhs->type == VmType::Double || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, VmType::Int, VM_INST_GREATER, lhs, rhs);
	}

	VmValue* CreateCompareLessEqual(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		assert(lhs->type == VmType::Int || lhs->type == VmType::Double || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, VmType::Int, VM_INST_LESS_EQUAL, lhs, rhs);
	}

	VmValue* CreateCompareGreaterEqual(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		assert(lhs->type == VmType::Int || lhs->type == VmType::Double || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, VmType::Int, VM_INST_GREATER_EQUAL, lhs, rhs);
	}

	VmValue* CreateCompareEqual(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		assert(lhs->type == VmType::Int || lhs->type == VmType::Double || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, VmType::Int, VM_INST_EQUAL, lhs, rhs);
	}

	VmValue* CreateCompareNotEqual(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		// Some comparisons with null pointer are allowed
		if((lhs->type == VmType::FunctionRef || lhs->type == VmType::ArrayRef || lhs->type == VmType::AutoRef) && rhs->type == VmType::Pointer && isType<VmConstant>(rhs) && ((VmConstant*)rhs)->iValue == 0)
			return CreateInstruction(module, VmType::Int, VM_INST_NOT_EQUAL, lhs, rhs);

		assert(lhs->type == VmType::Int || lhs->type == VmType::Double || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, VmType::Int, VM_INST_NOT_EQUAL, lhs, rhs);
	}

	VmValue* CreateShl(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		assert(lhs->type == VmType::Int || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, lhs->type, VM_INST_SHL, lhs, rhs);
	}

	VmValue* CreateShr(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		assert(lhs->type == VmType::Int || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, lhs->type, VM_INST_SHR, lhs, rhs);
	}

	VmValue* CreateAnd(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		assert(lhs->type == VmType::Int || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, lhs->type, VM_INST_BIT_AND, lhs, rhs);
	}

	VmValue* CreateOr(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		assert(lhs->type == VmType::Int || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, lhs->type, VM_INST_BIT_OR, lhs, rhs);
	}

	VmValue* CreateXor(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		assert(lhs->type == VmType::Int || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, lhs->type, VM_INST_BIT_XOR, lhs, rhs);
	}

	VmValue* CreateLogicalAnd(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		assert(lhs->type == VmType::Int || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, VmType::Int, VM_INST_LOG_AND, lhs, rhs);
	}

	VmValue* CreateLogicalOr(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		assert(lhs->type == VmType::Int || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, VmType::Int, VM_INST_LOG_OR, lhs, rhs);
	}

	VmValue* CreateLogicalXor(VmModule *module, VmValue *lhs, VmValue *rhs)
	{
		assert(lhs->type == VmType::Int || lhs->type == VmType::Long);
		assert(lhs->type == rhs->type);

		return CreateInstruction(module, VmType::Int, VM_INST_LOG_XOR, lhs, rhs);
	}

	VmValue* CreateNeg(VmModule *module, VmValue *value)
	{
		assert(value->type == VmType::Int || value->type == VmType::Double || value->type == VmType::Long);

		return CreateInstruction(module, value->type, VM_INST_NEG, value);
	}

	VmValue* CreateNot(VmModule *module, VmValue *value)
	{
		assert(value->type == VmType::Int || value->type == VmType::Long);

		return CreateInstruction(module, value->type, VM_INST_BIT_NOT, value);
	}

	VmValue* CreateLogicalNot(VmModule *module, VmValue *value)
	{
		assert(value->type == VmType::Int || value->type == VmType::Long || value->type == VmType::Pointer || value->type == VmType::AutoRef);

		return CreateInstruction(module, VmType::Int, VM_INST_LOG_NOT, value);
	}

	VmValue* CreateJump(VmModule *module, VmValue *label)
	{
		assert(label->type == VmType::Label);

		return CreateInstruction(module, VmType::Void, VM_INST_JUMP, label);
	}

	VmValue* CreateJumpZero(VmModule *module, VmValue *value, VmValue *trueLabel, VmValue *falseLabel)
	{
		assert(value->type == VmType::Int);
		assert(trueLabel->type == VmType::Label);
		assert(falseLabel->type == VmType::Label);

		return CreateInstruction(module, VmType::Void, VM_INST_JUMP_Z, value, trueLabel, falseLabel);
	}

	VmValue* CreateJumpNotZero(VmModule *module, VmValue *value, VmValue *trueLabel, VmValue *falseLabel)
	{
		assert(value->type == VmType::Int);
		assert(trueLabel->type == VmType::Label);
		assert(falseLabel->type == VmType::Label);

		return CreateInstruction(module, VmType::Void, VM_INST_JUMP_NZ, value, trueLabel, falseLabel);
	}

	VmValue* CreateReturn(VmModule *module)
	{
		return CreateInstruction(module, VmType::Void, VM_INST_RETURN);
	}

	VmValue* CreateReturn(VmModule *module, VmValue *value)
	{
		return CreateInstruction(module, VmType::Void, VM_INST_RETURN, value);
	}

	VmValue* CreateYield(VmModule *module, VmValue *value)
	{
		return CreateInstruction(module, VmType::Void, VM_INST_YIELD, value);
	}

	VmValue* CreateVariableAddress(VmModule *module, VariableData *variable)
	{
		assert(!IsMemberScope(variable->scope));

		if(IsGlobalScope(variable->scope))
			return CreateConstantPointer(variable->offset);

		return CreateInstruction(module, VmType::Pointer, VM_INST_FRAME_OFFSET, CreateConstantOffset(variable->offset));
	}

	VmValue* CreateTypeIndex(VmModule *module, TypeBase *type)
	{
		return CreateInstruction(module, VmType::Int, VM_INST_TYPE_ID, CreateConstantInt(type->typeIndex));
	}

	VmValue* AllocateScopeVariable(ExpressionContext &ctx, VmModule *module, TypeBase *type)
	{
		FunctionData *function = module->currentFunction->function;

		ScopeData *scope = NULL;
		unsigned offset = 0;

		if(function)
		{
			scope = function->scope;
			offset = unsigned(function->stackSize);

			function->stackSize += type->size; // TODO: alignment
		}
		else
		{
			scope = ctx.globalScope;
			offset = unsigned(scope->globalSize);

			scope->globalSize += type->size; // TODO: alignment
		}

		char *name = new char[16];
		sprintf(name, "$temp%d", ctx.unnamedVariableCount++);

		VariableData *variable = new VariableData(scope, type->alignment, type, InplaceStr(name), offset, 0);

		scope->variables.push_back(variable);

		ctx.variables.push_back(variable);

		return CreateVariableAddress(module, variable);
	}

	void ChangeInstructionTo(VmInstruction *inst, VmInstructionType cmd, VmValue *first, VmValue *second, VmValue *third)
	{
		inst->cmd = cmd;

		SmallArray<VmValue*, 128> arguments;
		arguments.reserve(inst->arguments.size());
		arguments.push_back(inst->arguments.data, inst->arguments.size());

		inst->arguments.clear();

		if(first)
			inst->AddArgument(first);

		if(second)
			inst->AddArgument(second);

		if(third)
			inst->AddArgument(third);

		for(unsigned i = 0; i < arguments.size(); i++)
			arguments[i]->RemoveUse(inst);

		inst->hasSideEffects = HasSideEffects(cmd);
	}

	void ReplaceValue(VmValue *value, VmValue *original, VmValue *replacement)
	{
		if(VmFunction *function = getType<VmFunction>(value))
		{
			for(VmBlock *curr = function->firstBlock; curr; curr = curr->nextSibling)
				ReplaceValue(curr, original, replacement);
		}
		else if(VmBlock *block = getType<VmBlock>(value))
		{
			for(VmInstruction *curr = block->firstInstruction; curr; curr = curr->nextSibling)
				ReplaceValue(curr, original, replacement);
		}
		else if(VmInstruction *inst = getType<VmInstruction>(value))
		{
			assert(original);
			assert(replacement);

			for(unsigned i = 0; i < inst->arguments.size(); i++)
			{
				if(inst->arguments[i] == original)
				{
					replacement->AddUse(inst);
					original->RemoveUse(inst);

					inst->arguments[i] = replacement;
				}
			}
		}
	}

	void ReplaceValueUsersWith(VmValue *inst, VmValue *value)
	{
		SmallArray<VmValue*, 256> users;
		users.reserve(inst->users.size());
		users.push_back(inst->users.data, inst->users.size());

		for(unsigned i = 0; i < users.size(); i++)
			ReplaceValue(users[i], inst, value);
	}
}

const VmType VmType::Void = VmType(VM_TYPE_VOID, 0);
const VmType VmType::Int = VmType(VM_TYPE_INT, 4);
const VmType VmType::Double = VmType(VM_TYPE_DOUBLE, 8);
const VmType VmType::Long = VmType(VM_TYPE_LONG, 8);
const VmType VmType::Label = VmType(VM_TYPE_LABEL, 4);
const VmType VmType::Pointer = VmType(VM_TYPE_POINTER, NULLC_PTR_SIZE);
const VmType VmType::FunctionRef = VmType(VM_TYPE_FUNCTION_REF, NULLC_PTR_SIZE + 4); // context + id
const VmType VmType::ArrayRef = VmType(VM_TYPE_ARRAY_REF, NULLC_PTR_SIZE + 4); // ptr + length
const VmType VmType::AutoRef = VmType(VM_TYPE_AUTO_REF, 4 + NULLC_PTR_SIZE); // type + ptr
const VmType VmType::AutoArray = VmType(VM_TYPE_AUTO_ARRAY, 4 + NULLC_PTR_SIZE + 4); // type + ptr + length

void VmValue::AddUse(VmValue* user)
{
	users.push_back(user);
}

void VmValue::RemoveUse(VmValue* user)
{
	for(unsigned i = 0; i < users.size(); i++)
	{
		if(users[i] == user)
		{
			users[i] = users.back();
			users.pop_back();
			break;
		}
	}

	if(users.empty() && !hasSideEffects)
	{
		if(VmInstruction *instruction = getType<VmInstruction>(this))
			instruction->parent->RemoveInstruction(instruction);
		else if(VmBlock *block = getType<VmBlock>(this))
			block->parent->RemoveBlock(block);
	}
}

void VmInstruction::AddArgument(VmValue *argument)
{
	assert(argument);
	assert(argument->type != VmType::Void);

	arguments.push_back(argument);

	argument->AddUse(this);
}

void VmBlock::AddInstruction(VmInstruction* instruction)
{
	assert(instruction);
	assert(instruction->parent == NULL);

	instruction->parent = this;

	if(!firstInstruction)
	{
		firstInstruction = lastInstruction = instruction;
	}else{
		lastInstruction->nextSibling = instruction;
		instruction->prevSibling = lastInstruction;
		lastInstruction = instruction;
	}
}

void VmBlock::RemoveInstruction(VmInstruction* instruction)
{
	assert(instruction);
	assert(instruction->parent == this);

	if(instruction == firstInstruction)
		firstInstruction = instruction->nextSibling;

	if(instruction == lastInstruction)
		lastInstruction = instruction->prevSibling;

	if(instruction->prevSibling)
		instruction->prevSibling->nextSibling = instruction->nextSibling;
	if(instruction->nextSibling)
		instruction->nextSibling->prevSibling = instruction->prevSibling;

	for(unsigned i = 0; i < instruction->arguments.size(); i++)
		instruction->arguments[i]->RemoveUse(instruction);
}

void VmFunction::AddBlock(VmBlock* block)
{
	assert(block);
	assert(block->parent == NULL);

	block->parent = this;

	if(!firstBlock)
	{
		firstBlock = lastBlock = block;
	}else{
		lastBlock->nextSibling = block;
		block->prevSibling = lastBlock;
		lastBlock = block;
	}
}

void VmFunction::RemoveBlock(VmBlock* block)
{
	assert(block);
	assert(block->parent == this);

	if(block == firstBlock)
		firstBlock = block->nextSibling;

	if(block == lastBlock)
		lastBlock = block->prevSibling;

	if(block->prevSibling)
		block->prevSibling->nextSibling = block->nextSibling;
	if(block->nextSibling)
		block->nextSibling->prevSibling = block->prevSibling;

	while(block->lastInstruction)
		block->RemoveInstruction(block->lastInstruction);
}

VmType GetVmType(ExpressionContext &ctx, TypeBase *type)
{
	if(type == ctx.typeVoid)
		return VmType::Void;

	if(type == ctx.typeBool || type == ctx.typeChar || type == ctx.typeShort || type == ctx.typeInt)
		return VmType::Int;

	if(type == ctx.typeLong)
		return VmType::Long;

	if(type == ctx.typeFloat || type == ctx.typeDouble)
		return VmType::Double;

	if(isType<TypeRef>(type))
		return VmType::Pointer;

	if(isType<TypeFunction>(type))
		return VmType::FunctionRef;

	if(isType<TypeUnsizedArray>(type))
		return VmType::ArrayRef;

	if(isType<TypeAutoRef>(type))
		return VmType::AutoRef;

	if(isType<TypeAutoArray>(type))
		return VmType::AutoArray;

	if(isType<TypeTypeID>(type))
		return VmType::Int;

	if(isType<TypeArray>(type) || isType<TypeClass>(type))
	{
		if(isType<TypeClass>(type) && type->size == 0)
			return VmType::Int;

		assert(type->size % 4 == 0);
		assert(type->size != 0);
		assert(type->size < NULLC_MAX_TYPE_SIZE);

		return VmType::Struct(type->size);
	}

	assert(!"unknown type");

	return VmType::Void;
}

VmValue* CompileVm(ExpressionContext &ctx, VmModule *module, ExprBase *expression)
{
	if(ExprVoid *node = getType<ExprVoid>(expression))
	{
		return CheckType(ctx, expression, new VmVoid());
	}
	else if(ExprBoolLiteral *node = getType<ExprBoolLiteral>(expression))
	{
		return CheckType(ctx, expression, CreateConstantInt(node->value ? 1 : 0));
	}
	else if(ExprCharacterLiteral *node = getType<ExprCharacterLiteral>(expression))
	{
		return CheckType(ctx, expression, CreateConstantInt(node->value));
	}
	else if(ExprStringLiteral *node = getType<ExprStringLiteral>(expression))
	{
		unsigned size = node->length + 1;

		// Align to 4
		size = (size + 3) & ~3;

		char *value = new char[size];
		memset(value, 0, size);

		for(unsigned i = 0; i < node->length; i++)
			value[i] = node->value[i];

		return CheckType(ctx, expression, CreateConstantStruct(value, size));
	}
	else if(ExprIntegerLiteral *node = getType<ExprIntegerLiteral>(expression))
	{
		if(node->type == ctx.typeInt)
			return CheckType(ctx, expression, CreateConstantInt(int(node->value)));

		if(node->type == ctx.typeLong)
			return CheckType(ctx, expression, CreateConstantLong(node->value));

		assert(!"unknown type");
	}
	else if(ExprRationalLiteral *node = getType<ExprRationalLiteral>(expression))
	{
		return CheckType(ctx, expression, CreateConstantDouble(node->value));
	}
	else if(ExprTypeLiteral *node = getType<ExprTypeLiteral>(expression))
	{
		return CheckType(ctx, expression, CreateTypeIndex(module, node->value));
	}
	else if(ExprNullptrLiteral *node = getType<ExprNullptrLiteral>(expression))
	{
		return CheckType(ctx, expression, CreateConstantPointer(0));
	}
	else if(ExprArray *node = getType<ExprArray>(expression))
	{
		VmValue *address = AllocateScopeVariable(ctx, module, node->type);

		TypeArray *arrayType = getType<TypeArray>(node->type);

		assert(arrayType);

		unsigned offset = 0;

		for(ExprBase *value = node->values.head; value; value = value->next)
		{
			VmValue *element = CompileVm(ctx, module, value);

			CreateStore(ctx, module, arrayType->subType, CreateAdd(module, address, CreateConstantInt(offset)), element);

			offset += unsigned(arrayType->subType->size);
		}

		return CheckType(ctx, expression, CreateLoad(ctx, module, node->type, address));
	}
	else if(ExprPreModify *node = getType<ExprPreModify>(expression))
	{
		VmValue *address = CompileVm(ctx, module, node->value);

		TypeRef *refType = getType<TypeRef>(node->value->type);

		assert(refType);

		VmValue *value = CreateLoad(ctx, module, refType->subType, address);

		if(value->type == VmType::Int)
			value = CreateAdd(module, value, CreateConstantInt(node->isIncrement ? 1 : -1));
		else if(value->type == VmType::Double)
			value = CreateAdd(module, value, CreateConstantDouble(node->isIncrement ? 1.0 : -1.0));
		else if(value->type == VmType::Long)
			value = CreateAdd(module, value, CreateConstantLong(node->isIncrement ? 1ll : -1ll));
		else
			assert("!unknown type");

		CreateStore(ctx, module, refType->subType, address, value);

		return CheckType(ctx, expression, value);

	}
	else if(ExprPostModify *node = getType<ExprPostModify>(expression))
	{
		VmValue *address = CompileVm(ctx, module, node->value);

		TypeRef *refType = getType<TypeRef>(node->value->type);

		assert(refType);

		VmValue *value = CreateLoad(ctx, module, refType->subType, address);
		VmValue *result = value;

		if(value->type == VmType::Int)
			value = CreateAdd(module, value, CreateConstantInt(node->isIncrement ? 1 : -1));
		else if(value->type == VmType::Double)
			value = CreateAdd(module, value, CreateConstantDouble(node->isIncrement ? 1.0 : -1.0));
		else if(value->type == VmType::Long)
			value = CreateAdd(module, value, CreateConstantLong(node->isIncrement ? 1ll : -1ll));
		else
			assert("!unknown type");

		CreateStore(ctx, module, refType->subType, address, value);

		return CheckType(ctx, expression, result);
	}
	else if(ExprTypeCast *node = getType<ExprTypeCast>(expression))
	{
		VmValue *value = CompileVm(ctx, module, node->value);

		if(isType<TypeFunction>(node->value->type) && node->type == ctx.typeBool)
			return CheckType(ctx, expression, CreateCompareNotEqual(module, value, CreateConstantPointer(0)));

		if(isType<TypeUnsizedArray>(node->value->type) && node->type == ctx.typeBool)
			return CheckType(ctx, expression, CreateCompareNotEqual(module, value, CreateConstantPointer(0)));

		VmType target = GetVmType(ctx, node->type);

		return CheckType(ctx, expression, CreateCast(module, value, target));
	}
	else if(ExprUnaryOp *node = getType<ExprUnaryOp>(expression))
	{
		VmValue *value = CompileVm(ctx, module, node->value);

		VmValue *result = NULL;

		switch(node->op)
		{
		case SYN_UNARY_OP_PLUS:
			result = value;
			break;
		case SYN_UNARY_OP_NEGATE:
			result = CreateNeg(module, value);
			break;
		case SYN_UNARY_OP_BIT_NOT:
			result = CreateNot(module, value);
			break;
		case SYN_UNARY_OP_LOGICAL_NOT:
			result = CreateLogicalNot(module, value);
			break;
		}

		assert(result);

		return CheckType(ctx, expression, result);
	}
	else if(ExprBinaryOp *node = getType<ExprBinaryOp>(expression))
	{
		VmValue *lhs = CompileVm(ctx, module, node->lhs);
		VmValue *rhs = CompileVm(ctx, module, node->rhs);

		VmValue *result = NULL;

		switch(node->op)
		{
		case SYN_BINARY_OP_ADD:
			result = CreateAdd(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_SUB:
			result = CreateSub(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_MUL:
			result = CreateMul(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_DIV:
			result = CreateDiv(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_MOD:
			result = CreateMod(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_POW:
			result = CreatePow(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_SHL:
			result = CreateShl(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_SHR:
			result = CreateShr(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_LESS:
			result = CreateCompareLess(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_LESS_EQUAL:
			result = CreateCompareLessEqual(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_GREATER:
			result = CreateCompareGreater(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_GREATER_EQUAL:
			result = CreateCompareGreaterEqual(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_EQUAL:
			result = CreateCompareEqual(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_NOT_EQUAL:
			result = CreateCompareNotEqual(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_BIT_AND:
			result = CreateAnd(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_BIT_OR:
			result = CreateOr(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_BIT_XOR:
			result = CreateXor(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_LOGICAL_AND:
			result = CreateLogicalAnd(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_LOGICAL_OR:
			result = CreateLogicalOr(module, lhs, rhs);
			break;
		case SYN_BINARY_OP_LOGICAL_XOR:
			result = CreateLogicalXor(module, lhs, rhs);
			break;
		}

		assert(result);

		return CheckType(ctx, expression, result);
	}
	else if(ExprGetAddress *node = getType<ExprGetAddress>(expression))
	{
		return CheckType(ctx, expression, CreateVariableAddress(module, node->variable));
	}
	else if(ExprDereference *node = getType<ExprDereference>(expression))
	{
		VmValue *value = CompileVm(ctx, module, node->value);

		TypeRef *refType = getType<TypeRef>(node->value->type);

		assert(refType);
		assert(refType->subType == node->type);

		return CheckType(ctx, expression, CreateLoad(ctx, module, node->type, value));
	}
	else if(ExprConditional *node = getType<ExprConditional>(expression))
	{
		VmValue *address = AllocateScopeVariable(ctx, module, node->type);

		VmValue* condition = CompileVm(ctx, module, node->condition);

		VmBlock *trueBlock = new VmBlock(InplaceStr("if_true"), module->nextBlockId++);
		VmBlock *falseBlock = new VmBlock(InplaceStr("if_false"), module->nextBlockId++);
		VmBlock *exitBlock = new VmBlock(InplaceStr("if_exit"), module->nextBlockId++);

		if(node->falseBlock)
			CreateJumpNotZero(module, condition, trueBlock, falseBlock);
		else
			CreateJumpNotZero(module, condition, trueBlock, exitBlock);

		module->currentFunction->AddBlock(trueBlock);
		module->currentBlock = trueBlock;

		CreateStore(ctx, module, node->type, address, CompileVm(ctx, module, node->trueBlock));

		CreateJump(module, exitBlock);

		if(node->falseBlock)
		{
			module->currentFunction->AddBlock(falseBlock);
			module->currentBlock = falseBlock;

			CreateStore(ctx, module, node->type, address, CompileVm(ctx, module, node->falseBlock));

			CreateJump(module, exitBlock);
		}

		module->currentFunction->AddBlock(exitBlock);
		module->currentBlock = exitBlock;

		return CheckType(ctx, expression, CreateLoad(ctx, module, node->type, address));
	}
	else if(ExprAssignment *node = getType<ExprAssignment>(expression))
	{
		TypeRef *refType = getType<TypeRef>(node->lhs->type);

		assert(refType);
		assert(refType->subType == node->rhs->type);

		VmValue *address = CompileVm(ctx, module, node->lhs);

		VmValue *initializer = CompileVm(ctx, module, node->rhs);

		CreateStore(ctx, module, node->rhs->type, address, initializer);

		return CheckType(ctx, expression, CreateLoad(ctx, module, node->rhs->type, address));
	}
	else if(ExprModifyAssignment *node = getType<ExprModifyAssignment>(expression))
	{
		TypeRef *refType = getType<TypeRef>(node->lhs->type);

		assert(refType);

		VmValue *address = CompileVm(ctx, module, node->lhs);

		VmValue *value = CreateLoad(ctx, module, refType->subType, address);

		VmValue *modification = CompileVm(ctx, module, node->rhs);

		switch(node->op)
		{
		case SYN_MODIFY_ASSIGN_ADD:
			value = CreateAdd(module, value, modification);
			break;
		case SYN_MODIFY_ASSIGN_SUB:
			value = CreateSub(module, value, modification);
			break;
		case SYN_MODIFY_ASSIGN_MUL:
			value = CreateMul(module, value, modification);
			break;
		case SYN_MODIFY_ASSIGN_DIV:
			value = CreateDiv(module, value, modification);
			break;
		case SYN_MODIFY_ASSIGN_POW:
			value = CreatePow(module, value, modification);
			break;
		case SYN_MODIFY_ASSIGN_MOD:
			value = CreateMod(module, value, modification);
			break;
		case SYN_MODIFY_ASSIGN_SHL:
			value = CreateShl(module, value, modification);
			break;
		case SYN_MODIFY_ASSIGN_SHR:
			value = CreateShr(module, value, modification);
			break;
		case SYN_MODIFY_ASSIGN_BIT_AND:
			value = CreateAnd(module, value, modification);
			break;
		case SYN_MODIFY_ASSIGN_BIT_OR:
			value = CreateOr(module, value, modification);
			break;
		case SYN_MODIFY_ASSIGN_BIT_XOR:
			value = CreateXor(module, value, modification);
			break;
		}

		CreateStore(ctx, module, refType->subType, address, value);

		return CheckType(ctx, expression, value);
	}
	else if(ExprMemberAccess *node = getType<ExprMemberAccess>(expression))
	{
		VmValue *value = CompileVm(ctx, module, node->value);

		assert(isType<TypeRef>(node->value->type));

		VmValue *offset = CreateConstantInt(node->member->offset);

		return CheckType(ctx, expression, CreateAdd(module, value, offset));
	}
	else if(ExprArrayIndex *node = getType<ExprArrayIndex>(expression))
	{
		VmValue *value = CompileVm(ctx, module, node->value);
		VmValue *index = CompileVm(ctx, module, node->index);

		if(isType<TypeUnsizedArray>(node->value->type))
			return CheckType(ctx, expression, CreateIndexUnsized(module, value, index));

		TypeRef *refType = getType<TypeRef>(node->value->type);

		assert(refType);

		TypeArray *arrayType = getType<TypeArray>(refType->subType);

		assert(arrayType);
		assert(unsigned(arrayType->subType->size) == arrayType->subType->size);

		VmValue *elementSize = CreateConstantInt(unsigned(arrayType->subType->size));

		return CheckType(ctx, expression, CreateIndex(module, value, elementSize, index));
	}
	else if(ExprReturn *node = getType<ExprReturn>(expression))
	{
		VmValue *value = CompileVm(ctx, module, node->value);

		if(node->value->type == ctx.typeVoid)
			return CheckType(ctx, expression, CreateReturn(module));

		return CheckType(ctx, expression, CreateReturn(module, value));
	}
	else if(ExprYield *node = getType<ExprYield>(expression))
	{
		VmValue *value = CompileVm(ctx, module, node->value);

		return CheckType(ctx, expression, CreateYield(module, value));
	}
	else if(ExprVariableDefinition *node = getType<ExprVariableDefinition>(expression))
	{
		if(node->initializer)
			CompileVm(ctx, module, node->initializer);

		return CheckType(ctx, expression, new VmVoid());
	}
	else if(ExprVariableDefinitions *node = getType<ExprVariableDefinitions>(expression))
	{
		for(ExprVariableDefinition *value = node->definitions.head; value; value = getType<ExprVariableDefinition>(value->next))
			CompileVm(ctx, module, value);

		return CheckType(ctx, expression, new VmVoid());
	}
	else if(ExprVariableAccess *node = getType<ExprVariableAccess>(expression))
	{
		VmValue *address = CreateVariableAddress(module, node->variable);

		return CheckType(ctx, expression, CreateLoad(ctx, module, node->variable->type, address));
	}
	else if(ExprFunctionDefinition *node = getType<ExprFunctionDefinition>(expression))
	{
		VmFunction *function = node->function->vmFunction;
		
		if(node->prototype)
			return CheckType(ctx, expression, function);

		// Store state
		unsigned nextBlockId = module->nextBlockId;
		unsigned nextInstructionId = module->nextInstructionId;
		VmFunction *currentFunction = module->currentFunction;
		VmBlock *currentBlock = module->currentBlock;

		// Switch to new function
		module->nextBlockId = 1;
		module->nextInstructionId = 1;

		module->currentFunction = function;

		VmBlock *block = new VmBlock(InplaceStr("start"), module->nextBlockId++);

		module->currentFunction->AddBlock(block);
		module->currentBlock = block;
		block->AddUse(function);

		for(ExprBase *value = node->expressions.head; value; value = value->next)
			CompileVm(ctx, module, value);

		// Restore state
		module->nextBlockId = nextBlockId;
		module->nextInstructionId = nextInstructionId;
		module->currentFunction = currentFunction;
		module->currentBlock = currentBlock;

		return CheckType(ctx, expression, function);
	}
	else if(ExprGenericFunctionPrototype *node = getType<ExprGenericFunctionPrototype>(expression))
	{
		return CheckType(ctx, expression, new VmVoid());
	}
	else if(ExprFunctionAccess *node = getType<ExprFunctionAccess>(expression))
	{
		assert(node->function->vmFunction);

		return CheckType(ctx, expression, node->function->vmFunction);
	}
	else if(ExprFunctionCall *node = getType<ExprFunctionCall>(expression))
	{
		VmValue *function = CompileVm(ctx, module, node->function);

		assert(module->currentBlock);

		VmInstruction *inst = new VmInstruction(GetVmType(ctx, node->type), VM_INST_CALL, module->nextInstructionId++);

		unsigned argCount = 1;

		for(ExprBase *value = node->arguments.head; value; value = value->next)
			argCount++;

		inst->arguments.reserve(argCount);

		inst->AddArgument(function);

		for(ExprBase *value = node->arguments.head; value; value = value->next)
		{
			VmValue *argument = CompileVm(ctx, module, value);

			assert(argument->type != VmType::Void);

			inst->AddArgument(argument);
		}

		inst->hasSideEffects = true;

		module->currentBlock->AddInstruction(inst);

		return CheckType(ctx, expression, inst);
	}
	else if(ExprAliasDefinition *node = getType<ExprAliasDefinition>(expression))
	{
		return CheckType(ctx, expression, new VmVoid());
	}
	else if(ExprGenericClassPrototype *node = getType<ExprGenericClassPrototype>(expression))
	{
		return CheckType(ctx, expression, new VmVoid());
	}
	else if(ExprClassDefinition *node = getType<ExprClassDefinition>(expression))
	{
		for(ExprBase *value = node->functions.head; value; value = value->next)
			CompileVm(ctx, module, value);

		return CheckType(ctx, expression, new VmVoid());
	}
	else if(ExprIfElse *node = getType<ExprIfElse>(expression))
	{
		VmValue* condition = CompileVm(ctx, module, node->condition);

		VmBlock *trueBlock = new VmBlock(InplaceStr("if_true"), module->nextBlockId++);
		VmBlock *falseBlock = new VmBlock(InplaceStr("if_false"), module->nextBlockId++);
		VmBlock *exitBlock = new VmBlock(InplaceStr("if_exit"), module->nextBlockId++);

		if(node->falseBlock)
			CreateJumpNotZero(module, condition, trueBlock, falseBlock);
		else
			CreateJumpNotZero(module, condition, trueBlock, exitBlock);

		module->currentFunction->AddBlock(trueBlock);
		module->currentBlock = trueBlock;

		CompileVm(ctx, module, node->trueBlock);

		CreateJump(module, exitBlock);

		if(node->falseBlock)
		{
			module->currentFunction->AddBlock(falseBlock);
			module->currentBlock = falseBlock;

			CompileVm(ctx, module, node->falseBlock);

			CreateJump(module, exitBlock);
		}

		module->currentFunction->AddBlock(exitBlock);
		module->currentBlock = exitBlock;

		return CheckType(ctx, expression, new VmVoid());
	}
	else if(ExprFor *node = getType<ExprFor>(expression))
	{
		CompileVm(ctx, module, node->initializer);

		VmBlock *conditionBlock = new VmBlock(InplaceStr("for_cond"), module->nextBlockId++);
		VmBlock *bodyBlock = new VmBlock(InplaceStr("for_body"), module->nextBlockId++);
		VmBlock *iterationBlock = new VmBlock(InplaceStr("for_iter"), module->nextBlockId++);
		VmBlock *exitBlock = new VmBlock(InplaceStr("for_exit"), module->nextBlockId++);

		//currLoopIDs.push_back(LoopInfo(exitBlock, iterationBlock));

		CreateJump(module, conditionBlock);

		module->currentFunction->AddBlock(conditionBlock);
		module->currentBlock = conditionBlock;

		VmValue* condition = CompileVm(ctx, module, node->condition);

		CreateJumpNotZero(module, condition, bodyBlock, exitBlock);

		module->currentFunction->AddBlock(bodyBlock);
		module->currentBlock = bodyBlock;

		CompileVm(ctx, module, node->body);

		CreateJump(module, iterationBlock);

		module->currentFunction->AddBlock(iterationBlock);
		module->currentBlock = iterationBlock;

		CompileVm(ctx, module, node->increment);

		CreateJump(module, conditionBlock);

		module->currentFunction->AddBlock(exitBlock);
		module->currentBlock = exitBlock;

		//currLoopIDs.pop_back();

		return CheckType(ctx, expression, new VmVoid());
	}
	else if(ExprWhile *node = getType<ExprWhile>(expression))
	{
		VmBlock *conditionBlock = new VmBlock(InplaceStr("while_cond"), module->nextBlockId++);
		VmBlock *bodyBlock = new VmBlock(InplaceStr("while_body"), module->nextBlockId++);
		VmBlock *exitBlock = new VmBlock(InplaceStr("while_exit"), module->nextBlockId++);

		//currLoopIDs.push_back(LoopInfo(exitBlock, conditionBlock));

		CreateJump(module, conditionBlock);

		module->currentFunction->AddBlock(conditionBlock);
		module->currentBlock = conditionBlock;

		VmValue* condition = CompileVm(ctx, module, node->condition);

		CreateJumpNotZero(module, condition, bodyBlock, exitBlock);

		module->currentFunction->AddBlock(bodyBlock);
		module->currentBlock = bodyBlock;

		CompileVm(ctx, module, node->body);

		CreateJump(module, conditionBlock);

		module->currentFunction->AddBlock(exitBlock);
		module->currentBlock = exitBlock;

		//currLoopIDs.pop_back();

		return CheckType(ctx, expression, new VmVoid());
	}
	else if(ExprDoWhile *node = getType<ExprDoWhile>(expression))
	{
		VmBlock *bodyBlock = new VmBlock(InplaceStr("do_body"), module->nextBlockId++);
		VmBlock *condBlock = new VmBlock(InplaceStr("do_cond"), module->nextBlockId++);
		VmBlock *exitBlock = new VmBlock(InplaceStr("do_exit"), module->nextBlockId++);

		CreateJump(module, bodyBlock);

		module->currentFunction->AddBlock(bodyBlock);
		module->currentBlock = bodyBlock;

		//currLoopIDs.push_back(LoopInfo(exitBlock, condBlock));

		CompileVm(ctx, module, node->body);

		CreateJump(module, condBlock);

		module->currentFunction->AddBlock(condBlock);
		module->currentBlock = condBlock;

		VmValue* condition = CompileVm(ctx, module, node->condition);

		CreateJumpNotZero(module, condition, bodyBlock, exitBlock);

		module->currentFunction->AddBlock(exitBlock);
		module->currentBlock = exitBlock;

		//currLoopIDs.pop_back();
	}
	else if(ExprBlock *node = getType<ExprBlock>(expression))
	{
		for(ExprBase *value = node->expressions.head; value; value = value->next)
			CompileVm(ctx, module, value);

		return CheckType(ctx, expression, new VmVoid());
	}
	else if(!expression)
	{
		return NULL;
	}
	else
	{
		assert(!"unknown type");
	}

	return NULL;
}

VmModule* CompileVm(ExpressionContext &ctx, ExprBase *expression)
{
	if(ExprModule *node = getType<ExprModule>(expression))
	{
		VmModule *module = new VmModule();

		// Generate global function
		VmFunction *global = new VmFunction(VmType::Void, NULL, VmType::Void);

		// Generate type indexes
		for(unsigned i = 0; i < ctx.types.size(); i++)
		{
			TypeBase *type = ctx.types[i];

			type->typeIndex = i;
		}

		// Generate VmFunction object for each function
		for(unsigned i = 0; i < ctx.functions.size(); i++)
		{
			FunctionData *function = ctx.functions[i];

			if(function->type->isGeneric)
				continue;

			VmFunction *vmFunction = new VmFunction(GetVmType(ctx, function->type), function, GetVmType(ctx, function->type->returnType));

			ctx.functions[i]->vmFunction = vmFunction;

			module->functions.push_back(vmFunction);
		}

		// Setup global function
		module->currentFunction = global;

		VmBlock *block = new VmBlock(InplaceStr("start"), module->nextBlockId++);

		global->AddBlock(block);
		module->currentBlock = block;
		block->AddUse(global);

		for(ExprBase *value = node->expressions.head; value; value = value->next)
			CompileVm(ctx, module, value);

		module->functions.push_back(global);

		return module;
	}

	return NULL;
}

void RunPeepholeOptimizations(VmModule *module, VmValue* value)
{
	if(VmFunction *function = getType<VmFunction>(value))
	{
		VmBlock *curr = function->firstBlock;

		while(curr)
		{
			VmBlock *next = curr->nextSibling;
			RunPeepholeOptimizations(module, curr);
			curr = next;
		}
	}
	else if(VmBlock *block = getType<VmBlock>(value))
	{
		VmInstruction *curr = block->firstInstruction;

		while(curr)
		{
			VmInstruction *next = curr->nextSibling;
			RunPeepholeOptimizations(module, curr);
			curr = next;
		}
	}
	else if(VmInstruction *inst = getType<VmInstruction>(value))
	{
		switch(inst->cmd)
		{
		case VM_INST_ADD:
			if(IsConstantZero(inst->arguments[0])) // 0 + x, all types
			{
				module->peepholeOptimizationCount++;
				ReplaceValueUsersWith(inst, inst->arguments[1]);
			}
			else if(IsConstantZero(inst->arguments[1])) // x + 0, all types
			{
				module->peepholeOptimizationCount++;
				ReplaceValueUsersWith(inst, inst->arguments[0]);
			}
			break;
		case VM_INST_SUB:
			if(DoesConstantIntegerMatch(inst->arguments[0], 0)) // 0 - x, integer types
			{
				module->peepholeOptimizationCount++;
				ChangeInstructionTo(inst, VM_INST_NEG, inst->arguments[1], NULL, NULL);
			}
			else if(IsConstantZero(inst->arguments[1])) // x - 0, all types
			{
				module->peepholeOptimizationCount++;
				ReplaceValueUsersWith(inst, inst->arguments[0]);
			}
			break;
		case VM_INST_MUL:
			if(IsConstantZero(inst->arguments[0]) || IsConstantZero(inst->arguments[1])) // 0 * x or x * 0, all types
			{
				if(inst->type == VmType::Int)
				{
					module->peepholeOptimizationCount++;
					ReplaceValueUsersWith(inst, CreateConstantInt(0));
				}
				else if(inst->type == VmType::Double)
				{
					module->peepholeOptimizationCount++;
					ReplaceValueUsersWith(inst, CreateConstantDouble(0));
				}
				else if(inst->type == VmType::Long)
				{
					module->peepholeOptimizationCount++;
					ReplaceValueUsersWith(inst, CreateConstantLong(0));
				}
			}
			else if(IsConstantOne(inst->arguments[0])) // 1 * x, all types
			{
				module->peepholeOptimizationCount++;
				ReplaceValueUsersWith(inst, inst->arguments[1]);
			}
			else if(IsConstantOne(inst->arguments[1])) // x * 1, all types
			{
				module->peepholeOptimizationCount++;
				ReplaceValueUsersWith(inst, inst->arguments[0]);
			}
			break;
		}
	}
}

void RunOptimizationPass(VmModule *module, VmOptimizationType type)
{
	for(VmFunction *value = module->functions.head; value; value = value->next)
	{
		switch(type)
		{
		case VM_OPT_PEEPHOLE:
			RunPeepholeOptimizations(module, value);
			break;
		}
	}
}
