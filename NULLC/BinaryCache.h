#pragma once
#include "stdafx.h"

namespace BinaryCache
{
	void Initialize();
	void Terminate();

	void	PutBytecode(const char* path, char* bytecode);
	char*	GetBytecode(const char* path);

	void		SetImportPath(const char* path);
	const char*	GetImportPath();

	struct	CodeDescriptor
	{
		const char		*name;
		unsigned int	nameHash;
		char			*binary;
	};
}